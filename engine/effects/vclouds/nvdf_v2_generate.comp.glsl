#version 460 core

// Improved NVDF (Nubis Voxel Density Field) generator.
// Supports multi-profile baking (multiple cloudscapes in one volume).

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(r8, binding = 0) writeonly uniform image3D u_targetNVDF;

uniform float u_cloudType;     // 0=stratus  0.5=strato  1=cumulus
uniform float u_coverage;      // 0..1 global coverage
uniform float u_noiseScale;    // base shape frequency
uniform float u_detailScale;   // erosion frequency
uniform float u_densityScale;  // output multiplier
uniform float u_erosion;       // 0..1 erosion strength
uniform float u_heightBias;    // shifts layer profile up/down
uniform float u_anvilBias;     // cumulonimbus anvil widening
uniform float u_curlyness;     // distortion strength

uniform int   u_profileIndex;  // Which profile are we baking (used for X-offsetting)
uniform int   u_numProfiles;

uniform sampler3D u_noiseBase;   // 64^3 RGBA8
uniform sampler3D u_noiseDetail; // 32^3 RGB8

float remap(float v, float a, float b, float c, float d) {
    return c + (v - a) / (b - a) * (d - c);
}

float remapC(float v, float a, float b, float c, float d) {
    return clamp(remap(v, a, b, c, d), min(c, d), max(c, d));
}

// Improved height profile with sharper control
float cloudLayerDensity(float h, float t) {
    h = clamp(h, 0.0, 1.0);
    float stratus = max(0.0, remapC(h, 0.00, 0.10, 0.0, 1.0) * remapC(h, 0.20, 0.30, 1.0, 0.0));
    float strato  = max(0.0, remapC(h, 0.00, 0.20, 0.0, 1.0) * remapC(h, 0.40, 0.70, 1.0, 0.0));
    float cumulus = max(0.0, remapC(h, 0.00, 0.15, 0.0, 1.0) * remapC(h, 0.70, 0.95, 1.0, 0.0));
    
    float d1 = mix(stratus, strato,  clamp(t * 2.0,         0.0, 1.0));
    float d2 = mix(strato,  cumulus, clamp((t - 0.5) * 2.0, 0.0, 1.0));
    return mix(d1, d2, t);
}

void main() {
    ivec3 voxel = ivec3(gl_GlobalInvocationID.xyz);
    ivec3 sz    = imageSize(u_targetNVDF);
    if (voxel.x >= sz.x || voxel.y >= sz.y || voxel.z >= sz.z) return;

    vec3 uvw = (vec3(voxel) + 0.5) / vec3(sz);
    
    // Multi-profile mapping: Each profile gets a vertical slice in X
    // (Actually, usually profiles are stored as different height-fields or separate textures,
    // but here we pack them side-by-side in X for a single 3D volume fetch with offset)
    float profileWidth = 1.0 / float(u_numProfiles);
    float profileStart = float(u_profileIndex) * profileWidth;
    
    // Map local X [0,1] to global slice
    vec3 localUVW = uvw;
    localUVW.x = profileStart + uvw.x * profileWidth;

    float h = clamp(uvw.y + u_heightBias, 0.0, 1.0);

    // 1. Height profile
    float layer = cloudLayerDensity(h, u_cloudType);
    if (layer < 0.001) { return; } // Keep existing content if not baking all at once? 
                                   // No, we usually clear first.

    // 2. Coverage & Anvil
    float anvil = mix(1.0, 1.0 + h * h * 1.5, clamp(u_anvilBias, 0.0, 1.0));
    float coverage = clamp(u_coverage * anvil, 0.0, 1.0);

    // 3. Base shape with "curlyness" (coordinate distortion)
    vec3 noiseUVW = uvw * u_noiseScale;
    if (u_curlyness > 0.0) {
        vec3 distort = texture(u_noiseDetail, uvw * u_detailScale).rgb * 2.0 - 1.0;
        noiseUVW += distort * u_curlyness;
    }
    
    vec4 bn = texture(u_noiseBase, noiseUVW);
    float pw = bn.r;
    float base = remapC(pw, 1.0 - coverage, 1.0, 0.0, 1.0) * layer;

    // Worley erosion
    float worleyFBM = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    base = remapC(base, worleyFBM * u_erosion, 1.0, 0.0, 1.0);

    // 4. Detail erosion
    vec4 dn = texture(u_noiseDetail, uvw * u_detailScale);
    float det = dn.r * 0.625 + dn.g * 0.25 + dn.b * 0.125;
    det = mix(1.0 - det, det, clamp(h * 6.0, 0.0, 1.0));
    base = remapC(base, det * u_erosion, 1.0, 0.0, 1.0);

    float density = clamp(base * u_densityScale, 0.0, 1.0);
    imageStore(u_targetNVDF, voxel, vec4(density, 0.0, 0.0, 0.0));
}
