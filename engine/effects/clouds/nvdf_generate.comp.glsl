#version 460 core

// NVDF (Nubis Voxel Density Field) generator.
// Bakes a low-resolution 3D density texture from:
//   - cloud-type aware height profile (stratus / stratocumulus / cumulus / cumulonimbus)
//   - constant coverage (or a 2D weather map driving XZ coverage)
//   - 3D Perlin-Worley + Worley noise (shared with the cloud raymarcher)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(r8, binding = 0) writeonly uniform image3D u_targetNVDF;

uniform float u_cloudType;     // 0=stratus  0.5=strato  1=cumulus
uniform float u_coverage;      // 0..1 global coverage
uniform float u_noiseScale;    // base shape frequency (texture-space)
uniform float u_detailScale;   // erosion frequency
uniform float u_densityScale;  // output multiplier
uniform float u_erosion;       // 0..1 erosion strength
uniform float u_heightBias;    // shifts layer profile up/down
uniform float u_anvilBias;     // cumulonimbus anvil widening
uniform int   u_useWeatherMap; // 1 if u_weatherMap should drive XZ coverage
uniform sampler3D u_noiseBase;   // 64^3 RGBA8 from cloud system
uniform sampler3D u_noiseDetail; // 32^3 RGB8  from cloud system
uniform sampler2D u_weatherMap;  // optional R-channel coverage map

float remap(float v, float a, float b, float c, float d) {
    return c + (v - a) / (b - a) * (d - c);
}

// sorted bounds — AMD UB workaround for clamp(lo>hi)
float remapC(float v, float a, float b, float c, float d) {
    return clamp(remap(v, a, b, c, d), min(c, d), max(c, d));
}

float cloudLayerDensity(float h, float t) {
    h = clamp(h, 0.0, 1.0);
    float stratus = max(0.0, remapC(h, 0.00, 0.10, 0.0, 1.0)
                            * remapC(h, 0.20, 0.30, 1.0, 0.0));
    float strato  = max(0.0, remapC(h, 0.00, 0.20, 0.0, 1.0)
                            * remapC(h, 0.40, 0.70, 1.0, 0.0));
    float cumulus = max(0.0, remapC(h, 0.00, 0.15, 0.0, 1.0)
                            * remapC(h, 0.70, 0.95, 1.0, 0.0));
    float d1 = mix(stratus, strato,  clamp(t * 2.0,         0.0, 1.0));
    float d2 = mix(strato,  cumulus, clamp((t - 0.5) * 2.0, 0.0, 1.0));
    return mix(d1, d2, t);
}

void main() {
    ivec3 voxel = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 sz    = imageSize(u_targetNVDF);
    if (voxel.x >= sz.x || voxel.y >= sz.y || voxel.z >= sz.z) return;

    vec3 uvw = (vec3(voxel) + 0.5) / vec3(sz);   // [0,1]
    float h  = clamp(uvw.y + u_heightBias, 0.0, 1.0);

    // 1. Height profile
    float layer = cloudLayerDensity(h, u_cloudType);
    if (layer < 0.001) { imageStore(u_targetNVDF, voxel, vec4(0.0)); return; }

    // 2. Coverage (optionally from weather map)
    float covBase = u_coverage;
    if (u_useWeatherMap != 0) covBase *= texture(u_weatherMap, uvw.xz).r;

    // Anvil widening — cumulonimbus expands at high altitudes.
    // Pushes coverage up the higher we go, scaled by u_anvilBias.
    float anvil   = mix(1.0, 1.0 + h * h * 1.5, clamp(u_anvilBias, 0.0, 1.0));
    float coverage = clamp(covBase * anvil, 0.0, 1.0);
    if (coverage < 0.001) { imageStore(u_targetNVDF, voxel, vec4(0.0)); return; }

    // 3. Base shape — Perlin-Worley + Worley octaves from u_noiseBase (RGBA8)
    vec4 bn = texture(u_noiseBase, uvw * u_noiseScale);
    float pw   = bn.r;
    float base = remapC(pw, 1.0 - coverage, 1.0, 0.0, 1.0) * layer;
    if (base < 0.001) { imageStore(u_targetNVDF, voxel, vec4(0.0)); return; }

    float worleyFBM = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    base = remapC(base, worleyFBM * u_erosion, 1.0, 0.0, 1.0);
    if (base < 0.001) { imageStore(u_targetNVDF, voxel, vec4(0.0)); return; }

    // 4. Detail erosion — inverted near base for wispy tendrils
    vec4 dn = texture(u_noiseDetail, uvw * u_detailScale);
    float det = dn.r * 0.625 + dn.g * 0.25 + dn.b * 0.125;
    det = mix(1.0 - det, det, clamp(h * 6.0, 0.0, 1.0));
    base = remapC(base, det * u_erosion, 1.0, 0.0, 1.0);

    float density = clamp(base * u_densityScale, 0.0, 1.0);
    imageStore(u_targetNVDF, voxel, vec4(density, 0.0, 0.0, 0.0));
}
