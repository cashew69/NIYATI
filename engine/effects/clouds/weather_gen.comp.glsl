#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba8, binding = 0) uniform image2D uWeatherMap;

uniform int   uWidth;
uniform int   uHeight;
uniform uint  uSeed;

// Pattern selector: 0=FBM Noise  1=Spiral  2=Cyclone  3=Bands  4=Cellular
uniform int   uPatternType;
uniform float uCenterX;        // spiral/cyclone center (UV space)
uniform float uCenterY;
uniform int   uArms;           // spiral/cyclone arm count (1-5)
uniform float uTightness;      // spiral: radians per UV radius (higher = tighter)
uniform float uFalloffRadius;  // spiral/cyclone outer envelope radius (UV units)
uniform float uBandAngle;      // bands: direction in radians
uniform float uBandWidth;      // bands: cloud fraction of band period (0-1)
uniform float uBandSpacing;    // bands: period in UV units
uniform float uBandTurbulence; // bands: turbulence displacement strength
uniform float uNoiseFreq;      // noise/cellular: frequency multiplier
uniform float uCoverageScale;  // global coverage density (0-1)
uniform float uCoverageMin;    // remap output dark end  (default 0.0)
uniform float uCoverageMax;    // remap output bright end (default 1.0)

const float PI     = 3.14159265358979;
const float TWO_PI = 6.28318530717959;
const int   MAX_ARMS = 5;

// Seed UV offset set in main() — used by center-relative patterns so uCenterX/Y
// stays in pixel [0,1] space regardless of the seed shift applied to u/v.
float g_ox = 0.0;
float g_oz = 0.0;

// ---------------------------------------------------------------------------
// Noise helpers (matching CPU vcn_ functions — same hash/period scheme)
// ---------------------------------------------------------------------------

float hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    z = ((z % period) + period) % period;
    uint n = uint(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013);
    n ^= n >> 13;  n *= 0xb5297a4du;
    n ^= n >> 7;   n *= 0x68e31da4u;
    n ^= n >> 11;
    return float(n & 0x00ffffffu) / 16777216.0;
}

float valueNoise(vec3 p, int period) {
    ivec3 p0 = ivec3(floor(p));
    vec3 f = fract(p);
    f = f*f*(3.0 - 2.0*f);
    float h000 = hash(p0.x,   p0.y,   p0.z,   period, 0);
    float h100 = hash(p0.x+1, p0.y,   p0.z,   period, 0);
    float h010 = hash(p0.x,   p0.y+1, p0.z,   period, 0);
    float h110 = hash(p0.x+1, p0.y+1, p0.z,   period, 0);
    float h001 = hash(p0.x,   p0.y,   p0.z+1, period, 0);
    float h101 = hash(p0.x+1, p0.y,   p0.z+1, period, 0);
    float h011 = hash(p0.x,   p0.y+1, p0.z+1, period, 0);
    float h111 = hash(p0.x+1, p0.y+1, p0.z+1, period, 0);
    return mix(mix(mix(h000, h100, f.x), mix(h010, h110, f.x), f.y),
               mix(mix(h001, h101, f.x), mix(h011, h111, f.x), f.y), f.z);
}

float fbm(vec3 p, int baseFreq, int oct) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < oct; i++) {
        int f = baseFreq << i;
        v += a * valueNoise(p * float(f), f);
        a *= 0.5;
    }
    return v;
}

float worley(vec3 p, int freq) {
    vec3  fp = p * float(freq);
    ivec3 cp = ivec3(floor(fp));
    float md = 9.0;
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        ivec3 np = cp + ivec3(dx, dy, dz);
        vec3 cell = vec3(np) + vec3(hash(np.x, np.y, np.z, freq, 0),
                                    hash(np.x, np.y, np.z, freq, 1),
                                    hash(np.x, np.y, np.z, freq, 2));
        float d = dot(fp - cell, fp - cell);
        if (d < md) md = d;
    }
    return 1.0 - sqrt(md) / 1.73205;
}

float perlinWorley(vec3 p, int freq) {
    float perlin = clamp(fbm(p, freq, 3) * 1.5, 0.0, 1.0);
    float invW   = worley(p, freq);
    return invW + perlin * (1.0 - invW);
}

// ---------------------------------------------------------------------------
// Pattern 0 — FBM Noise
// uNoiseFreq scales overall noise frequency; uCoverageScale shifts threshold.
// ---------------------------------------------------------------------------
float patternNoise(float u, float v) {
    float freq = max(uNoiseFreq, 0.1);
    float cov  = perlinWorley(vec3(u * freq, 0.45, v * freq), 3);
    float threshold = max(0.0, 1.0 - uCoverageScale * 1.2);
    cov = cov < threshold ? 0.0 : (cov - threshold) / max(1.0 - threshold, 0.001);
    return clamp(cov * cov * (3.0 - 2.0 * cov), 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Pattern 1 — Spiral (Archimedean)
// Arms wind outward from center; uTightness = radians of winding per UV radius.
// ---------------------------------------------------------------------------
float patternSpiral(float u, float v) {
    vec2  delta = vec2((u - g_ox) - uCenterX, (v - g_oz) - uCenterY);
    float r     = length(delta);
    float theta = atan(delta.y, delta.x);
    if (theta < 0.0) theta += TWO_PI;

    int   nArms     = clamp(uArms, 1, MAX_ARMS);
    float armPeriod = TWO_PI / float(nArms);
    float tightness = max(uTightness, 0.1);

    // Angular position relative to nearest arm of the Archimedean spiral
    float spiralPhase = mod(theta - r * tightness + 100.0 * PI, armPeriod);
    float armDist     = min(spiralPhase, armPeriod - spiralPhase); // [0, armPeriod/2]
    float armWidth    = armPeriod * 0.28;
    float cov         = 1.0 - smoothstep(0.0, armWidth, armDist);

    // Radial envelope — fade beyond falloffRadius
    float envelope = exp(-r * r / max(uFalloffRadius * uFalloffRadius, 0.001));
    cov *= envelope;

    // Noise variation breaks up the arms into cloud clumps
    float noiseBreak = 0.4 + 0.6 * fbm(vec3(u * 7.0, 0.5, v * 7.0), 2, 2);
    cov *= noiseBreak;

    return clamp(cov * uCoverageScale * 1.6, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Pattern 2 — Cyclone
// Dense eye wall + curved spiral rain bands; uFalloffRadius sets storm size.
// ---------------------------------------------------------------------------
float patternCyclone(float u, float v) {
    vec2  delta = vec2((u - g_ox) - uCenterX, (v - g_oz) - uCenterY);
    float r     = length(delta);
    float theta = atan(delta.y, delta.x);
    if (theta < 0.0) theta += TWO_PI;

    int   nArms     = clamp(uArms, 1, MAX_ARMS);
    float armPeriod = TWO_PI / float(nArms);
    float tightness = max(uTightness, 0.1);

    // Cyclone arms curve more at outer radii: phase offset grows with r
    float spiralPhase = mod(theta - r * tightness * 8.0 + 100.0 * PI, armPeriod);
    float armDist     = min(spiralPhase, armPeriod - spiralPhase);
    float armWidth    = armPeriod * 0.22;
    float bands       = 1.0 - smoothstep(0.0, armWidth, armDist);

    // Eye wall: a dense ring just outside the clear eye
    float eyeRadius = uFalloffRadius * 0.09;
    float eyeWall   = exp(-pow(abs(r - eyeRadius) / max(eyeRadius * 0.6, 0.001), 2.0));

    // Clear the eye center smoothly
    float eyeClear  = smoothstep(0.0, eyeRadius * 0.6, r);

    // Radial envelope
    float envelope  = exp(-r * r / max(uFalloffRadius * uFalloffRadius, 0.001)) * eyeClear;

    float cov = max(eyeWall * eyeClear, bands * envelope);
    cov *= 0.55 + 0.45 * fbm(vec3(u * 6.0, 0.4, v * 6.0), 2, 2);

    return clamp(cov * uCoverageScale * 1.3, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Pattern 3 — Bands (frontal cloud streets)
// uBandAngle controls direction; turbulence warps the band edges.
// ---------------------------------------------------------------------------
float patternBands(float u, float v) {
    float cosA = cos(uBandAngle);
    float sinA = sin(uBandAngle);

    // Project UV onto band normal
    float proj = (u - 0.5) * cosA + (v - 0.5) * sinA;

    // Turbulence displacement
    float turb  = fbm(vec3(u * 5.0 + 0.31, 0.5, v * 5.0 + 0.73), 2, 3) - 0.5;
    proj += turb * uBandTurbulence;

    // Periodic bands: sine -> smooth envelope
    float spacing = max(uBandSpacing, 0.01);
    float phase   = mod(proj / spacing + 100.0, 1.0);  // [0,1] within one period
    // Tent function: peak at 0.5, zero at 0 and 1
    float bandVal = 1.0 - abs(phase - 0.5) * 2.0;
    float halfW   = clamp(uBandWidth * 0.5, 0.01, 0.49);
    float cov     = smoothstep(0.5 - halfW, 0.5, bandVal);

    return clamp(cov * uCoverageScale, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Pattern 4 — Cellular (scattered Cumulus fields)
// Worley cells create isolated cloud masses; uNoiseFreq controls cell scale.
// ---------------------------------------------------------------------------
float patternCellular(float u, float v) {
    float freq      = clamp(uNoiseFreq, 0.3, 8.0);
    float w         = worley(vec3(u * freq, 0.5, v * freq), int(freq * 3.5 + 0.5));
    float threshold = max(0.0, 1.0 - uCoverageScale * 0.9);
    float cov       = w < threshold ? 0.0 : (w - threshold) / max(1.0 - threshold, 0.001);
    // FBM modulation to break up perfectly round cells
    cov *= 0.5 + 0.5 * fbm(vec3(u * freq * 1.3 + 0.17, 0.5, v * freq * 1.3 + 0.43), 2, 2);
    return clamp(cov * cov * 1.4, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (id.x >= uWidth || id.y >= uHeight) return;

    // Seed-based UV offset so every cloud node gets a unique pattern
    g_ox = float((uSeed * 1619u) & 0xffffu) / 65535.0;
    g_oz = float((uSeed * 31337u) & 0xffffu) / 65535.0;

    float u = float(id.x) / float(uWidth)  + g_ox;
    float v = float(id.y) / float(uHeight) + g_oz;

    // R: coverage — dispatched to the selected pattern
    float cov;
    if      (uPatternType == 1) cov = patternSpiral  (u, v);
    else if (uPatternType == 2) cov = patternCyclone (u, v);
    else if (uPatternType == 3) cov = patternBands   (u, v);
    else if (uPatternType == 4) cov = patternCellular(u, v);
    else                        cov = patternNoise   (u, v);  // default: 0

    // Coverage remapping: expands/contracts the [0,1] range to [min,max].
    // coverageMin > 0 raises the floor (lifts thin areas); coverageMax < 1 caps dense regions.
    cov = clamp(mix(uCoverageMin, uCoverageMax, cov), 0.0, 1.0);

    // G: precipitation — high in dense coverage cores
    float prec = clamp((cov - 0.72) * 3.5, 0.0, 1.0);

    // B: cloud type — Worley cells (stratus vs cumulus blend)
    float type = worley(vec3(u * 1.17 + 0.13, 0.5, v * 0.93 + 0.07), 5);
    type = type * type;

    // A: height scale — FBM variation in cloud tower height
    float hs = fbm(vec3(u * 0.61 + 0.19, 0.5, v * 0.73 + 0.11), 2, 3);
    hs = clamp(hs * 1.9 - 0.35, 0.0, 1.0);

    imageStore(uWeatherMap, id, vec4(cov, prec, type, hs));
}
