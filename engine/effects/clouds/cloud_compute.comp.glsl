#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 0) writeonly uniform image2D u_outputImage;

layout(std430, binding = 1) readonly buffer CloudSpheresBuffer { vec4 spheres[]; };

// ---- Uniforms ---------------------------------------------------------------
uniform int   u_sphereCount;
uniform vec3  u_cameraPos;
uniform mat4  u_view;
uniform mat4  u_proj;
uniform mat4  u_nonJitteredProj;
uniform mat4  u_worldMatrix;
uniform float u_time;

uniform float u_densityScale;
uniform int   u_maxSteps;
uniform float u_stepSize;
uniform float u_turbulence;
uniform float u_windSpeed;       // global drift: slides base noise + NVDF through the volume
uniform float u_localNoiseSpeed; // local churn: animates detail/curl noise independently
uniform vec3  u_boxMin;
uniform vec3  u_boxMax;

uniform vec3  u_sunDir;
uniform vec3  u_sunColor;
uniform float u_sunIntensity;
uniform float u_ambientStrength;
uniform float u_scatterG;

uniform vec3  u_cloudColorTop;
uniform vec3  u_cloudColorBottom;
uniform float u_absorption;
uniform float u_coverage;
uniform float u_erosion;
uniform float u_silverLining;
uniform int   u_flatBottom;     // kept for API compat, unused

// Nubis params
uniform float u_cloudType;      // 0=stratus  0.5=stratocumulus  1.0=cumulus
uniform float u_noiseScale;     // base shape noise frequency  (e.g. 0.006)
uniform float u_detailScale;    // erosion noise frequency     (e.g. 0.035)
uniform sampler3D u_noiseBase;  // 64^3 RGBA8: R=Perlin-Worley, GBA=Worley×3
uniform sampler3D u_noiseDetail;// 32^3 RGB8:  RGB=Worley high-freq erosion

uniform int   u_enableTAA;
uniform int   u_frameIndex;
uniform float u_taaBlend;
uniform vec3  u_prevCameraPos;
uniform mat4  u_prevView;       // previous frame view matrix — for reprojection
uniform mat4  u_prevProj;       // previous frame proj matrix — for reprojection
uniform sampler2D u_historyTex; // history colour (texture unit 2, bilinear filtered)

// NVDF sampling path (HZD/Nubis-2): sample a pre-baked 3D density texture
// instead of evaluating the full Nubis density function every step.
uniform int       u_useNVDF;        // 1 = sample u_nvdfTex; 0 = full procedural
uniform sampler3D u_nvdfTex;        // 3D R8 density, texture unit 3
uniform float     u_hwModStrength;  // height-width modulation amount (0..1)
uniform float     u_hwModScale;     // frequency of the HW field (XZ)
uniform float     u_curlStrength;   // high-freq "curly alligator" erosion
uniform float     u_nvdfTileScale;  // XZ tile rate: 1 / worldMetersPerTile
uniform vec3      u_nvdfWorldOffset;// XZ slide offset in world space (Y component unused)
uniform mat3      u_nvdfRotMat;     // 3D rotation applied to texture UVW (built from Euler X/Y/Z on CPU)
uniform float     u_nvdfYOffset;    // slides which height-slice of the texture maps to the cloud layer

// Adaptive raymarching / dual-pass controls
uniform float u_adaptiveFactor;    // step grows by t * this factor in empty space
uniform float u_jitterSwitchDist;  // below = animated jitter, above = static (no shimmer)
uniform int   u_passMode;          // 0=full  1=near only  2=far only
uniform float u_nearFarSplit;      // distance threshold for dual-pass split (metres)

// Circle field — masks the cloud volume to a cylinder in XZ instead of a box
uniform int   u_useCircleField;
uniform float u_circleRadius;      // world-space XZ radius from box centre

// Sphere field — wraps clouds around the viewer as a spherical shell
// (planet centre is anchored to camera XZ so the dome always surrounds you,
// Y is offset by planetRadius below cloudBaseHeight to give realistic curvature)
uniform int   u_useSphereField;
uniform float u_planetRadius;      // shell curvature radius; bigger = flatter horizon
uniform float u_cloudBaseHeight;   // world Y where the cloud layer starts
uniform float u_cloudThickness;    // shell thickness (outer - inner)
uniform float u_domeExtent;        // 1.0 = full sphere, 0.5 = hemisphere

// Blue noise jitter — 32×32 R8 tiled across the screen
uniform sampler2D u_blueNoise;

// Weather map — 2D RGBA projected top-down over the world
// R=coverage, G=precipitation (darkening), B=cloud type, A=height scale
uniform int       u_useWeatherMap;
uniform sampler2D u_weatherMap;
uniform float     u_weatherMapScale;   // p.xz * scale → UV  (manual mode only)
uniform int       u_autoWeatherMap;    // 1 = procedural auto map; UV is box/sphere-relative

// Atmospheric fog (aerial perspective)
uniform vec3  u_fogColor;
uniform float u_fogDensity;            // exp falloff per metre
uniform float u_fogStart;              // metres before fog kicks in

// Scene depth — sampled to early-exit pixels occluded by terrain/geometry
uniform int       u_useSceneDepth;
uniform sampler2D u_sceneDepth;

// ---- Workgroup-shared -------------------------------------------------------
shared mat4  gs_invProj;
shared mat4  gs_invView;
shared float gs_nodeScale;
shared int   gs_tileHits;

// ============================================================
// Utilities (Nubis remap — core of the Schneider density model)
// ============================================================

float remap(float v, float oMin, float oMax, float nMin, float nMax) {
    return nMin + ((v - oMin) / (oMax - oMin)) * (nMax - nMin);
}

float remapC(float v, float oMin, float oMax, float nMin, float nMax) {
    // GLSL clamp(x, lo, hi) is UB when lo > hi (AMD returns 0).
    // Decreasing remaps (e.g. 1→0) need the clamp bounds sorted.
    return clamp(remap(v, oMin, oMax, nMin, nMax), min(nMin, nMax), max(nMin, nMax));
}

// ============================================================
// Per-pixel temporal jitter
// ============================================================

// Distance-aware jitter (replaces the old stepJitter).
//
// Near camera (tNear < u_jitterSwitchDist):
//   Animated — the golden-ratio frame shift lets TAA accumulate a smooth result.
// Far camera (tNear >= u_jitterSwitchDist):
//   STATIC — the frame-index term is removed so the offset never changes between
//   frames. This eliminates the "shimmering" artifact that animated jitter produces
//   at large step sizes, where each frame samples a completely different part of
//   the cloud and the temporal blend can't keep up.
// HZD-style dual jitter source.
//
// Two hashes blended by distance — no `if` branch, so there is no tear line
// where one mode ends and the other begins. The static hash is anchored in
// WORLD space so the noise pattern sticks to the cloud geometry: if the
// camera rotates 1°, the same cloud point keeps the same hash value, instead
// of getting a new screen-space hash that would visually smear the noise.
float stepJitterAdaptive(vec3 rayOrigin, vec3 rayDir, float tNear) {
    // ---- Animated (screen-space, golden-ratio advance) -----------------------
    // Blue noise tiled across the screen, frame-shifted by 1/φ. TAA resolves
    // the resulting high-frequency grain perfectly. Used for close clouds where
    // animation is important to break up step banding.
    vec2  screenUV = (vec2(gl_GlobalInvocationID.xy) + 0.5) / 32.0;
    float animated = fract(texture(u_blueNoise, screenUV).r
                         + float(u_frameIndex) * 0.6180339887);

    // ---- Static (world-anchored) ---------------------------------------------
    // Uses the world position at tNear (entry point). As the camera moves, the
    // world point changes slightly but the hash remains stable relative to the
    // cloud's coordinate system.
    vec3  pos    = rayOrigin + rayDir * tNear;
    float staticH = fract(sin(dot(pos * 100.0, vec3(12.9898, 78.233, 45.164))) * 43758.5453);

    float blend = smoothstep(u_jitterSwitchDist * 0.5, u_jitterSwitchDist * 1.5, tNear);
    return mix(animated, staticH, blend);
}

// ============================================================
// Smooth sub-texel UVW jitter to break 3D texture slab visibility.
//
// WHY: rays marching near-parallel to one texture axis sample the same slice of
// the 64^3 texture for many steps in a row. The trilinear boundary between slices
// shows as a visible flat plane. A small UV perturbation that varies continuously
// in world space breaks the alignment without introducing any discontinuity.
//
// Uses low-frequency sinusoids with mutually irrational periods (no common factor)
// so no new grid pattern emerges. Magnitude ~0.018 UV ≈ 1.15 texels at 64^3.
// Unlike the previous floor-hash version, there are NO cell boundaries here, so
// no voxel / checkerboard cube artifacts.
// ============================================================

vec3 uvwJitter(vec3 p) {
    return vec3(
        sin(p.x * 0.073 + p.z * 0.051),
        sin(p.y * 0.079 + p.x * 0.063),
        sin(p.z * 0.067 + p.y * 0.057)
    ) * 0.018;
}

// ============================================================
// Weather map UV helper
//
// Auto mode (u_autoWeatherMap == 1):
//   Box mode    → UV centred on box, extent = box XZ size.  No tiling.
//   Sphere mode → UV centred on camera XZ, extent = planetRadius.  No tiling.
//   Result is always clamped by GL_CLAMP_TO_EDGE on the texture side.
//
// Manual mode (u_autoWeatherMap == 0):
//   Legacy: UV = p.xz * u_weatherMapScale  (world-space, may tile)
// ============================================================
vec2 weatherMapUV(vec2 xz) {
    if (u_autoWeatherMap != 0) {
        if (u_useSphereField != 0) {
            // Sphere: centre on camera, scale so the entire visible shell hemisphere
            // fits in [0,1].  planetRadius is the inner-shell radius, which equals
            // the maximum XZ reach visible at the shell horizon.
            float extent = max(u_planetRadius, 1.0);
            return (xz - u_cameraPos.xz) / extent * 0.5 + 0.5;
        }
        // Box: centre on the box, full box XZ extent → [0,1]
        vec2  boxCentre = (u_boxMin.xz + u_boxMax.xz) * 0.5;
        float extent    = max(max(u_boxMax.x - u_boxMin.x,
                                  u_boxMax.z - u_boxMin.z), 1.0);
        return (xz - boxCentre) / extent + 0.5;
    }
    return xz * u_weatherMapScale;
}

// ============================================================
// Nubis height-layer density profiles
// Source: Schneider "The Real-time Volumetric Cloudscapes of HZD", SIGGRAPH 2015
//
// Three types; cloudType blends between them:
//   stratus        — flat sheet, dense at 10–25% height
//   stratocumulus  — medium height, 0–50%
//   cumulus        — tall, flat base, domed top (most photogenic)
// ============================================================

float cloudLayerDensity(float h, float cloudType) {
    h = clamp(h, 0.0, 1.0);

    float stratus        = max(0.0, remapC(h, 0.00, 0.10, 0.0, 1.0)
                                  * remapC(h, 0.20, 0.30, 1.0, 0.0));
    float stratocumulus  = max(0.0, remapC(h, 0.00, 0.20, 0.0, 1.0)
                                  * remapC(h, 0.40, 0.70, 1.0, 0.0));
    float cumulus        = max(0.0, remapC(h, 0.00, 0.15, 0.0, 1.0)
                                  * remapC(h, 0.70, 0.95, 1.0, 0.0));

    float d1 = mix(stratus, stratocumulus, clamp(cloudType * 2.0,        0.0, 1.0));
    float d2 = mix(stratocumulus, cumulus,  clamp((cloudType - 0.5)*2.0,  0.0, 1.0));
    return mix(d1, d2, cloudType);
}

// ============================================================
// Coverage from sphere footprints projected onto the XZ plane.
//
// This is the KEY architectural change: spheres are no longer 3D SDF
// shapes. They are 2D Gaussian discs on the XZ plane. The cloud shape
// in 3D is governed entirely by the height profile + 3D noise. This is
// why the sphere-shaped look is gone — there is no sphere in 3D anymore.
// ============================================================

float sphereCoverage(vec3 p) {
    if (u_sphereCount == 0) return 1.0;
    float sc  = gs_nodeScale;
    float cov = 0.0;
    for (int i = 0; i < u_sphereCount; i++) {
        vec3  c = (u_worldMatrix * vec4(spheres[i].xyz, 1.0)).xyz;
        // Scale the footprint radius so each sphere controls a cloud-sized patch.
        // The sphere radius in the grid is 2-3 local units; multiplying by 8 turns
        // that into a 16-24 world-unit footprint, appropriate for a 500-unit box.
        float r = spheres[i].w * sc * 8.0;
        float d = length(p.xz - c.xz) / max(r, 0.001);
        cov = max(cov, 1.0 - smoothstep(0.0, 1.0, d));
    }
    return cov;
}

// ============================================================
// NVDF sampling — pre-baked 3D density field
//
// HZD/Nubis anti-repetition tricks applied at SAMPLE TIME so a single tiled
// NVDF can fill an arbitrarily large cloud box without visible tiling:
//   1. Per-entity random XZ rotation.
//   2. Per-entity random XZ phase offset.
//   3. High-frequency XZ domain warp driven by low-frequency noise.
// ============================================================

// Normalised height inside the cloud layer (0 at base, 1 at top).
// Sphere mode: parabolic sag d^2/(2R) approximates planet curvature —
//   the cloud base curves down by sag at horizontal distance d, so
//   adding sag to p.y gives height above the curved surface. No sphere
//   math needed; AABB intersection handles all camera positions correctly.
// Box mode: Y above boxMin mapped over box height.
float cloudHeight(vec3 p) {
    if (u_useSphereField != 0) {
        vec2  d   = p.xz - u_cameraPos.xz;
        float sag = dot(d, d) / (2.0 * max(u_planetRadius, 1.0));
        return (p.y + sag - u_cloudBaseHeight) / max(u_cloudThickness, 1.0);
    }
    return (p.y - u_boxMin.y) / max(u_boxMax.y - u_boxMin.y, 1.0);
}

float nvdfDensityCore(vec3 p, out float hOut) {
    float h = cloudHeight(p);
    hOut = h;
    if (h < 0.0 || h > 1.0) return 0.0;

    // 0. XZ world offset (slide). Rotation is applied to UVW after scaling (see step 4).
    vec2  pxz  = vec2(p.x + u_nvdfWorldOffset.x,
                      p.z + u_nvdfWorldOffset.z);

    // 1. Low-freq HW modulation field.
    //    When a weather map is present, its A channel drives cloud height variation
    //    (flat stratus patches vs tall cumulus towers) and replaces the procedural
    //    HW noise so the two sources agree spatially.
    vec2  hwUV = pxz * u_hwModScale;
    float hwN;
    if (u_useWeatherMap != 0) {
        hwN = texture(u_weatherMap, weatherMapUV(p.xz)).a;
    } else {
        hwN = texture(u_noiseBase, vec3(hwUV, 0.317)).r;  // arbitrary fixed Z
    }

    // 2. Height-Width modulation:
    //    hwScale > 1 → clouds appear taller (full profile used); < 1 → squat/stratus.
    //    This distributes varied cloud heights across the sky from a single NVDF.
    float hwScale = mix(1.0 - u_hwModStrength, 1.0 + u_hwModStrength,
                        clamp(hwN, 0.0, 1.0));
    // Y slide: u_nvdfYOffset shifts which height-slice of the texture is visible.
    // Positive → sample higher into the texture at a given cloud altitude.
    // Negative → sample lower.  CLAMP_TO_EDGE on T handles values outside [0,1].
    float effH = clamp(h / max(hwScale, 0.05) + u_nvdfYOffset, 0.0, 1.0);

    // 3. XZ domain warp + tile + wind drift.
    vec3  wind   = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    vec2  warp   = vec2(hwN - 0.5, hwN * 0.7 - 0.35) * u_hwModStrength * 80.0;
    vec2  uvxz   = (pxz + warp + wind.xz) * u_nvdfTileScale;

    // 4. Apply 3D rotation around the texture centre (0.5,0.5,0.5), then sample.
    //    u_nvdfRotMat is built on the CPU from three Euler angles (X tilt, Y spin, Z tilt).
    vec3  uvw_raw = vec3(uvxz.x, effH, uvxz.y);
    vec3  nvdfUVW = u_nvdfRotMat * (uvw_raw - 0.5) + vec3(0.5);
    float density = texture(u_nvdfTex, nvdfUVW).r;
    return density;
}

float cloudDensityNVDF(vec3 p) {
    float h;
    float density = nvdfDensityCore(p, h);
    if (density < 0.001) return 0.0;

    // Sphere mode: cluster footprints don't make sense for a global sky dome —
    // those local-space sphere positions at world origin, so sphereCoverage
    // returns 0 everywhere in the shell and kills all cloud density.
    float coverage;
    if (u_useSphereField != 0 || u_sphereCount == 0) {
        coverage = clamp(u_coverage * 0.5 + 0.5, 0.0, 1.0);
    } else {
        float spCov = sphereCoverage(p);
        coverage = clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    }
    density *= coverage;
    if (density < 0.001) return 0.0;

    // Curly-alligator erosion — high-freq detail at ~4× normal detail scale.
    // Inverted at cloud base for wispy tendrils, normal at top.
    // Uses localNoiseSpeed so curl churns independently of global wind drift.
    vec3  localWind = vec3(u_localNoiseSpeed, 0.0, u_localNoiseSpeed * 0.4) * u_time;
    vec3  curlUVW = (p + localWind * 1.4) * u_detailScale * 4.0 + uvwJitter(p) * 0.5;
    vec4  curlN   = texture(u_noiseDetail, curlUVW);
    float curl    = curlN.r * 0.5 + curlN.g * 0.3 + curlN.b * 0.2;
    curl = mix(1.0 - curl, curl, clamp(h * 6.0, 0.0, 1.0));
    density = remapC(density, curl * u_curlStrength, 1.0, 0.0, 1.0);

    return density * u_densityScale;
}

float cloudDensityNVDFFast(vec3 p) {
    float h;
    float density = nvdfDensityCore(p, h);
    if (density < 0.001) return 0.0;

    float coverage;
    if (u_useSphereField != 0 || u_sphereCount == 0) {
        coverage = clamp(u_coverage * 0.5 + 0.5, 0.0, 1.0);
    } else {
        float spCov = sphereCoverage(p);
        coverage = clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    }
    return density * coverage * u_densityScale;
}

// ============================================================
// Cloud density — Nubis/HZD style
//
// Density = layerProfile(height) × coverage(XZ) × perlinWorley
//           then eroded by multi-octave Worley (removes the solid-sphere look)
// ============================================================

float cloudDensity(vec3 p) {
    if (u_useSphereField == 0 && u_useCircleField != 0) {
        vec2 centre = (u_boxMin.xz + u_boxMax.xz) * 0.5;
        if (length(p.xz - centre) >= u_circleRadius) return 0.0;
    }
    if (u_useNVDF != 0) return cloudDensityNVDF(p);

    float h = cloudHeight(p);
    if (h < 0.0 || h > 1.0) return 0.0;

    // Weather map drives coverage + cloud type + height scale per XZ region.
    // R=coverage  G=precipitation darkening  B=cloud type  A=height scale
    float wmCoverage    = 1.0;
    float wmPrecipMul   = 1.0;
    float effCloudType  = u_cloudType;
    if (u_useWeatherMap != 0) {
        vec4 wm = texture(u_weatherMap, weatherMapUV(p.xz));
        wmCoverage   = wm.r;
        effCloudType = wm.b;
        wmPrecipMul  = mix(1.0, 1.7, wm.g);   // dense rainy cores darken/thicken
        // A channel: 0=flat stratus, 1=tall cumulus.
        // Rescale h so a small A squashes the cloud layer (stratus) and
        // a large A stretches it (tall cumulus towers).
        float hmScale = mix(0.45, 1.55, wm.a);
        h = clamp(h / max(hmScale, 0.05), 0.0, 1.0);
    }

    // 1. Height layer profile — gives the flat-base / domed-top shape
    float layerDensity = cloudLayerDensity(h, effCloudType);
    if (layerDensity < 0.001) return 0.0;

    // 2. Coverage: sphere footprints (XZ) biased by u_coverage * weather map R
    //    Skydome (sphere field) skips cluster footprints — the skydome is
    //    inherently global and shouldn't be cut up by 3×3 grid spheres.
    vec3 wind     = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float covBase;
    if (u_useSphereField != 0 || u_sphereCount == 0) {
        covBase = clamp(u_coverage, 0.0, 1.0);
    } else {
        float spCov = sphereCoverage(p);
        covBase = clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    }
    float coverage = clamp(covBase * wmCoverage, 0.0, 1.0);
    if (coverage < 0.001) return 0.0;

    // 3. Base shape: sample 3D Perlin-Worley texture
    //    R = Perlin-Worley (main cloud mass)
    //    G,B,A = Worley octaves (erosion channels)
    vec4 bn    = texture(u_noiseBase,   (p + wind) * u_noiseScale + uvwJitter(p));
    float pw   = bn.r;   // Perlin-Worley composite

    // Remap Perlin-Worley against coverage: sky areas (low coverage) erode away,
    // covered areas keep their full cloud shape.
    float base  = remapC(pw, 1.0 - coverage, 1.0, 0.0, 1.0);
    base       *= layerDensity;
    if (base < 0.001) return 0.0;

    // 4. Erode with multi-octave Worley to break up the surface
    //    GBA channels = Worley at 3 increasing frequencies
    float erosion = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    base = remapC(base, erosion, 1.0, 0.0, 1.0);
    if (base < 0.001) return 0.0;

    // 5. Detail erosion: high-frequency Worley from the detail texture.
    //    Inverted at cloud base (wispy tendrils hang down), normal at top.
    //    Uses localNoiseSpeed — surface churn is independent of global wind.
    vec3  localWind = vec3(u_localNoiseSpeed, 0.0, u_localNoiseSpeed * 0.4) * u_time;
    vec4 dn     = texture(u_noiseDetail, (p + localWind * 1.6) * u_detailScale + uvwJitter(p) * 0.5);
    float det   = dn.r * 0.625 + dn.g * 0.25 + dn.b * 0.125;
    det         = mix(1.0 - det, det, clamp(h * 6.0, 0.0, 1.0));
    base        = remapC(base, det * u_erosion, 1.0, 0.0, 1.0);

    return base * u_densityScale * wmPrecipMul;
}

// Fast variant — skip detail texture, one less texture sample
// Used for light marching where detail isn't needed for shadows
float cloudDensityFast(vec3 p) {
    if (u_useSphereField == 0 && u_useCircleField != 0) {
        vec2 centre = (u_boxMin.xz + u_boxMax.xz) * 0.5;
        if (length(p.xz - centre) >= u_circleRadius) return 0.0;
    }
    if (u_useNVDF != 0) return cloudDensityNVDFFast(p);

    float h = cloudHeight(p);
    if (h < 0.0 || h > 1.0) return 0.0;

    float wmCoverage   = 1.0, wmPrecipMul = 1.0;
    float effCloudType = u_cloudType;
    if (u_useWeatherMap != 0) {
        vec4 wm = texture(u_weatherMap, weatherMapUV(p.xz));
        wmCoverage   = wm.r;
        effCloudType = wm.b;
        wmPrecipMul  = mix(1.0, 1.7, wm.g);
        float hmScale = mix(0.45, 1.55, wm.a);
        h = clamp(h / max(hmScale, 0.05), 0.0, 1.0);
    }

    float layerDensity = cloudLayerDensity(h, effCloudType);
    if (layerDensity < 0.001) return 0.0;

    vec3  wind    = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float covBase;
    if (u_useSphereField != 0 || u_sphereCount == 0) {
        covBase = clamp(u_coverage, 0.0, 1.0);
    } else {
        float spCov = sphereCoverage(p);
        covBase = clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    }
    float coverage = clamp(covBase * wmCoverage, 0.0, 1.0);
    if (coverage < 0.001) return 0.0;

    // textureLod(... , 1.0) — light march only needs the cached mip 1 to escape
    // the cloud volume. Saves bandwidth vs sampling full-res 3D texture.
    vec4  bn     = textureLod(u_noiseBase, (p + wind) * u_noiseScale, 1.0);
    float base   = remapC(bn.r, 1.0 - coverage, 1.0, 0.0, 1.0) * layerDensity;
    float erosion = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    return remapC(base, erosion, 1.0, 0.0, 1.0) * u_densityScale * wmPrecipMul;
}

// ============================================================
// Box edge fade — hides AABB boundary
// ============================================================

float boxEdgeFade(vec3 p) {
    if (u_useSphereField != 0) {
        float h         = cloudHeight(p);
        float vertFade  = smoothstep(0.0, 0.08, h) * smoothstep(1.0, 0.92, h);
        float dist      = length(p.xz - u_cameraPos.xz);
        float maxR      = u_planetRadius * max(u_domeExtent, 0.01);
        float horizFade = smoothstep(maxR, maxR * 0.75, dist);
        return vertFade * horizFade;
    }

    vec3 t = (p - u_boxMin) / (u_boxMax - u_boxMin);
    vec3 d = min(t, 1.0 - t);
    float yFade = smoothstep(0.0, 0.10, d.y);

    if (u_useCircleField != 0) {
        vec2  centre = (u_boxMin.xz + u_boxMax.xz) * 0.5;
        float dist   = length(p.xz - centre);
        float inner  = u_circleRadius * 0.85;
        return yFade * smoothstep(u_circleRadius, inner, dist);
    }

    return yFade * smoothstep(0.0, 0.15, min(d.x, d.z));
}

// ============================================================
// Sun direction (can be driven by a real sun later)
// ============================================================

vec3 sunDir() {
    return u_sunDir;
}

// ============================================================
// Henyey-Greenstein phase function
// ============================================================

float PhaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(max(1.0 + g2 - 2.0*g*cosTheta, 1e-4), 1.5) * 0.079577;
}

// ============================================================
// Light march (Nubis-style: few steps toward sun, exponential strides)
// Uses cloudDensityFast — shadow quality doesn't need detail noise.
// ============================================================

float lightMarch(vec3 p) {
    // 4 steps with exponential growth — covers ~95% of the volume a 6-step march
    // would reach, at 2/3 the texture-sample cost. cloudDensityFast uses textureLod
    // at mip 1, so each fetch is also cheaper than the main march.
    //   step sequence: 12, 21.6, 38.9, 70.0  → total ≈ 142 world units toward sun
    vec3  sd   = sunDir();
    float d    = 0.0;
    float step = 12.0;
    for (int i = 0; i < 4; i++) {
        p += sd * step;
        d += cloudDensityFast(p) * step;
        step *= 1.8;
    }
    return d;
}

// ============================================================
// Multiple-scattering approximation (Nubis paper)
// Primary + two cheaper octaves with reduced density sensitivity
// Simulates light scattering through thick cloud cores
// ============================================================

float multiScatter(float absorption, float lightDensity) {
    return exp(-absorption * lightDensity)
         + exp(-absorption * lightDensity * 0.25) * 0.45
         + exp(-absorption * lightDensity * 0.06) * 0.25;
}

// Beer-Powder: bright edges, darker self-shadowed interior
float powderEffect(float density) {
    return 1.0 - exp(-density * 4.0);
}

// ============================================================
// Ray-AABB intersection
// ============================================================

bool rayAABB(vec3 ro, vec3 rd, vec3 bMin, vec3 bMax, out float tNear, out float tFar) {
    vec3 inv = 1.0 / rd;
    vec3 t0  = (bMin - ro) * inv;
    vec3 t1  = (bMax - ro) * inv;
    vec3 tMn = min(t0, t1), tMx = max(t0, t1);
    tNear = max(max(tMn.x, tMn.y), tMn.z);
    tFar  = min(min(tMx.x, tMx.y), tMx.z);
    return tFar > max(tNear, 0.0);
}

// Unified ray-vs-cloud-volume intersection.
// Sphere mode: camera-centred AABB at cloud altitude. Curvature is baked
//   per-sample into cloudHeight via parabolic sag — no sphere math here.
// Box mode: standard AABB from u_boxMin/u_boxMax.
bool rayCloudVolume(vec3 ro, vec3 rd, out float tNear, out float tFar) {
    if (u_useSphereField != 0) {
        float extent = u_planetRadius * max(u_domeExtent, 0.01);
        vec3 bMin = vec3(ro.x - extent, u_cloudBaseHeight,                    ro.z - extent);
        vec3 bMax = vec3(ro.x + extent, u_cloudBaseHeight + u_cloudThickness, ro.z + extent);
        return rayAABB(ro, rd, bMin, bMax, tNear, tFar);
    }
    return rayAABB(ro, rd, u_boxMin, u_boxMax, tNear, tFar);
}

// ============================================================
// Full ray march
// ============================================================

// Returns vec4(rgb, alpha) and writes the first-density-hit world distance to
// outFirstHit (−1.0 if the ray never intersected dense cloud). Caller uses
// outFirstHit for TAA reprojection (better than guessing the box midpoint) and
// for aerial-perspective fog blending.
vec4 raymarchCloud(vec3 rayOrigin, vec3 rayDir, out float outFirstHit) {
    outFirstHit = -1.0;

    float tNear, tFar;
    if (!rayCloudVolume(rayOrigin, rayDir, tNear, tFar))
        return vec4(0.0);
    tNear = max(tNear, 0.001);

    // ---- Dual-pass range clamp --------------------------------------------------
    if (u_passMode == 1) tFar  = min(tFar,  u_nearFarSplit);
    if (u_passMode == 2) tNear = max(tNear, u_nearFarSplit);
    if (tNear >= tFar) return vec4(0.0);

    float rayLen = min(tFar - tNear, 3000.0);

    vec3  sd       = sunDir();
    float cosTheta = dot(rayDir, sd);
    float phase    = PhaseHG(cosTheta, u_scatterG) * 0.7
                   + PhaseHG(cosTheta, -0.15) * 0.3;
    // Sun-direction bias for the Beer-Powder dark-edge term. Looking away
    // from the sun → no powder darkening (rear-lit clouds look uniformly bright);
    // looking toward the sun → full powder bias (visible dark inner core).
    float powderBias = smoothstep(-0.3, 0.6, cosTheta);

    float transmittance = 1.0;
    vec3  color         = vec3(0.0);

    // In sphere mode the box step-size (designed for ~100-unit boxes) is far too
    // small for a shell whose horizontal cross-section can be 6000+ units. Scale
    // it by the ratio of shell thickness to a reference box height so the same
    // maxSteps budget gives adequate coverage at any view angle.
    float dt_base = u_stepSize;
    if (u_useSphereField != 0) {
        float shellThick = max(u_cloudThickness, 1.0);
        dt_base = max(u_stepSize, shellThick / float(max(u_maxSteps, 1)) * 2.0);
    }

    float t = stepJitterAdaptive(rayOrigin, rayDir, tNear) * dt_base;

    int maxS = min(u_enableTAA != 0 ? u_maxSteps / 2 : u_maxSteps, 256);

    float prevCoarseDt = 0.0;

    for (int i = 0; i < maxS; i++) {
        if (t >= rayLen) break;

        vec3  pos     = rayOrigin + rayDir * (tNear + t);
        float density = cloudDensity(pos) * boxEdgeFade(pos);

        if (density > 0.001) {
            // Record the FIRST point where the ray actually intersects cloud.
            // Used downstream for TAA reprojection and aerial-perspective fog.
            if (outFirstHit < 0.0) outFirstHit = tNear + t;

            if (prevCoarseDt > 0.0) {
                t = max(t - prevCoarseDt * 0.6, 0.0);
                prevCoarseDt = 0.0;
                continue;
            }
            prevCoarseDt = 0.0;

            float dt    = dt_base;
            float ext   = density * u_absorption;
            float stepT = exp(-ext * dt);
            float lDen  = lightMarch(pos);

            // ---- Refined Beer-Powder (Schneider) ----------------------------
            // AAA refinement: Powder now darkens edges when looking at the sun
            // instead of boosting the center. This preserves internal gradients.
            float beer    = multiScatter(u_absorption, lDen);
            float powder  = powderEffect(density);
            float lightReach = beer * mix(1.0, powder, powderBias);

            float hFrac = clamp(cloudHeight(pos), 0.0, 1.0);

            // ---- Vertical Lighting Gradient (AAA "Dark Bottoms") ------------
            // 1. Height-Based Ambient Occlusion (HAO): Bottoms get less sky light.
            vec3  ambTop = u_cloudColorTop    * u_ambientStrength * 0.55
                         + vec3(0.25, 0.45, 0.75) * u_ambientStrength * 0.45;
            vec3  ambBot = u_cloudColorBottom * u_ambientStrength * 0.3;
            vec3  ambient = mix(ambBot, ambTop, hFrac) * pow(hFrac, 0.5);

            // 2. Vertical Transmittance (Global Shadow Proxy): Sunlight is 
            // attenuated by the cloud mass above the current point.
            float vertShadow = exp(-u_absorption * (1.0 - hFrac) * u_cloudThickness * 0.25);

            // 3. Albedo mix: Base color is primarily vertical.
            vec3 cloudAlbedo = mix(u_cloudColorBottom, u_cloudColorTop, hFrac);
            vec3 sunTint     = mix(vec3(1.0), u_sunColor, 0.3);
            vec3 sunLight    = cloudAlbedo * sunTint * u_sunIntensity
                             * lightReach * phase * vertShadow;

            float silverG = pow(max(0.0, dot(rayDir, sd)), 8.0);
            vec3  silver  = u_sunColor * u_silverLining * silverG
                          * (1.0 - transmittance) * beer * 0.3;

            color         += (sunLight + silver + ambient)
                           * density * transmittance * dt * u_absorption;
            transmittance *= stepT;

            if (transmittance < 0.005) break;
            t += dt;
        } else {
            // Fast-path: if a 2D weather map is loaded, sample it at the
            // current XZ and take a big leap when coverage is near zero.
            // This skips 3D texture reads entirely in empty sky patches —
            // the most expensive case for rays that cross cloud-free zones.
            if (u_useWeatherMap != 0) {
                vec3  probe   = rayOrigin + rayDir * (tNear + t);
                float wmCov   = texture(u_weatherMap, weatherMapUV(probe.xz)).r;
                if (wmCov < 0.05) {
                    prevCoarseDt = dt_base * 20.0;
                    t += prevCoarseDt;
                    continue;
                }
            }
            float raw = (tNear + t) * u_adaptiveFactor;
            prevCoarseDt = clamp(raw, dt_base, dt_base * 4.0);
            t += prevCoarseDt;
        }
    }

    // ACES filmic tone mapping
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    color = clamp((color*(a*color+b))/(color*(c*color+d)+e), 0.0, 1.0);
    color = pow(max(color, vec3(0.0)), vec3(1.0/2.2));

    // ---- Aerial perspective ---------------------------------------------------
    // Blend cloud color toward fog color based on first-hit distance.
    // Distant clouds visually merge into the atmospheric haze instead of
    // appearing as hard dark shapes against the horizon.
    if (u_fogDensity > 0.0 && outFirstHit > 0.0) {
        float fogT  = max(outFirstHit - u_fogStart, 0.0);
        float fogMix = 1.0 - exp(-fogT * u_fogDensity);
        color = mix(color, u_fogColor, fogMix);
    }

    return vec4(color, clamp(1.0 - transmittance, 0.0, 1.0));
}

// ============================================================
// Main
// ============================================================

// Reproject a world-space position into the previous frame's UV coordinates.
// Uses the approximate cloud midpoint so both camera rotation and translation
// (parallax) are handled — unlike same-pixel history reads which only handle static cameras.
// Returns vec2(-1) if the position is behind the previous camera or out of frame.
vec2 reprojectUV(vec3 worldPos) {
    vec4 clip = u_prevProj * (u_prevView * vec4(worldPos, 1.0));
    if (clip.w < 0.001) return vec2(-1.0);
    vec2 ndc = clip.xy / clip.w;
    if (any(greaterThan(abs(ndc), vec2(1.0)))) return vec2(-1.0);
    return ndc * 0.5 + 0.5;
}

void main() {
    ivec2 pixel    = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize  = imageSize(u_outputImage);
    bool  inBounds = (pixel.x < imgSize.x && pixel.y < imgSize.y);

    if (gl_LocalInvocationIndex == 0) {
        gs_invProj   = inverse(u_nonJitteredProj); // Used for depth unprojection
        gs_invView   = inverse(u_view);
        gs_nodeScale = length(vec3(u_worldMatrix[0]));
        gs_tileHits  = 0;
    }
    barrier();

    vec3 rayDir = vec3(0.0, 0.0, -1.0);
    if (inBounds) {
        vec2 uv  = (vec2(pixel) + 0.5) / vec2(imgSize);
        vec2 ndc = uv * 2.0 - 1.0;
        
        // RAY DIRECTION: uses jittered inverse projection to ensure TAA resolves correctly.
        mat4 invJittered = inverse(u_proj);
        vec4 vD  = invJittered * vec4(ndc, -1.0, 1.0);
        vD.xyz  /= vD.w;
        rayDir   = normalize((gs_invView * vec4(vD.xyz, 0.0)).xyz);

        float tN, tF;
        if (rayCloudVolume(u_cameraPos, rayDir, tN, tF))
            atomicOr(gs_tileHits, 1);
    }
    barrier();

    // Tile-based early exit: if no pixel in this 8x8 block hit the cloud volume,
    // skip everything. Saves massive cost in clear-sky regions.
    if (gs_tileHits == 0) {
        if (inBounds) imageStore(u_outputImage, pixel, vec4(0.0));
        return;
    }

    // ---- Checkerboard interleave ------------------------------------------------
    // When TAA is on: each frame only raymarches 1 of 4 pixels in a 2×2 pattern.
    // The other 3 pixels are filled from reprojected history — 4× fewer rays for free.
    //   slot 0: (even x, even y)   slot 1: (odd x, even y)
    //   slot 2: (even x, odd y)    slot 3: (odd x,  odd y)
    int  slot     = u_frameIndex % 4;
    bool isActive = (u_enableTAA == 0)   // TAA off → every pixel marches every frame
                  || (u_frameIndex == 0) // first frame always marches to seed history
                  || (pixel.x % 2 == slot % 2 && pixel.y % 2 == slot / 2);

    vec4  result    = vec4(0.0);
    float firstHit  = -1.0;
    if (isActive)
        result = raymarchCloud(u_cameraPos, rayDir, firstHit);

    // ---- Reprojection -----------------------------------------------------------
    // Use the exact first-density-hit distance for reprojection so close clouds
    // and far clouds project to their actual previous-frame screen positions.
    // The old box-midpoint heuristic ghosted because a thin cloud near the
    // viewer was reprojected as if it were in the middle of a 3-km-deep box.
    vec4 history  = vec4(0.0);
    bool hasHist  = false;
    if (u_enableTAA != 0 && u_frameIndex > 0) {
        float reproT = firstHit;
        if (reproT < 0.0) {
            // Active ray with no hit, or non-active checkerboard pixel — fall back
            // to the volume-entry point so the reprojection stays anchored to the
            // cloud geometry rather than some arbitrary screen-space position.
            // IMPORTANT: use the correct intersection for the current mode.
            // Using rayAABB in sphere mode gives wrong results because u_boxMin/Max
            // are meaningless in that path, causing TAA to produce black pixels.
            float tN, tF;
            bool hit = rayCloudVolume(u_cameraPos, rayDir, tN, tF);
            if (hit) reproT = max(tN, 0.001);
        }
        if (reproT > 0.0) {
            vec2 prevUV = reprojectUV(u_cameraPos + rayDir * reproT);
            if (prevUV.x >= 0.0) {
                history = texture(u_historyTex, prevUV);
                hasHist = (history.a > 0.001 || result.a > 0.001);
            }
        }
    }

    // ---- Blend ------------------------------------------------------------------
    if (!isActive) {
        // Non-active pixel: use reprojected history directly.
        result = hasHist ? history : vec4(0.0);
    } else if (hasHist) {
        // Active pixel: blend new result with reprojected history.
        // Scale toward more current-frame when camera moves fast so stale history
        // doesn't linger and cause ghosting.
        float speed  = length(u_cameraPos - u_prevCameraPos);
        float mBias  = clamp(speed * 0.08, 0.0, 1.0);
        float blend  = mix(u_taaBlend, min(u_taaBlend * 5.0, 0.85), mBias);
        result = mix(history, result, blend);
    }

    imageStore(u_outputImage, pixel, result);
}
