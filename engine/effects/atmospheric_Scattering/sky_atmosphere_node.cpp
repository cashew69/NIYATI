// sky_atmosphere_node.cpp
// Physically-based sky atmosphere — Hillaire 2020 / sebh/UnrealEngineSkyAtmosphere
// Three-LUT pipeline:
//   1. Transmittance LUT    256×64  RGBA16F   (static, rebuild when params change)
//   2. Multi-Scatter LUT    32×32   RGBA16F   (static, rebuild when params change)
//   3. Sky-View LUT         192×108 RGBA16F   (dynamic, rebuilt every frame)
// Fullscreen triangle renders the sky behind all geometry (NDC depth = 0.9999, GL_LEQUAL).

#include "engine/core/gl/structs.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Transmittance LUT compute shader
// Hillaire 2020 Appendix A UV parameterization, 40 ray-march steps.
// ---------------------------------------------------------------------------
static const char* s_TransmittanceSrc = R"GLSL(
#version 430 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) writeonly uniform image2D uTransmittanceLUT;

// Atmosphere parameters
uniform float uBotR;               // planet bottom radius (km)
uniform float uTopR;               // atmosphere top radius (km)
uniform vec3  uRayleighScattering; // per km
uniform float uRayleighExpScale;   // negative
uniform float uMieScattering;      // per km
uniform float uMieAbsorption;      // per km
uniform float uMieDensityExpScale; // negative
uniform vec3  uAbsorptionExtinction; // ozone per km

// Ray-sphere intersection returning the far intersection distance (positive).
// Returns -1 if no intersection.
float raySphereIntersectNearest(vec3 orig, vec3 dir, float R) {
    float b  = dot(orig, dir);
    float c  = dot(orig, orig) - R*R;
    float d  = b*b - c;
    if (d < 0.0) return -1.0;
    float sd = sqrt(d);
    float t1 = -b - sd;
    float t2 = -b + sd;
    if (t1 > 0.0) return t1;
    if (t2 > 0.0) return t2;
    return -1.0;
}

// Decode UV to (viewH, cosAngle) using Hillaire 2020 Appendix A.
void uvToAtmosParams(vec2 uv, out float viewH, out float cosAngle) {
    float H   = sqrt(max(0.0, uTopR*uTopR - uBotR*uBotR));
    float rho = H * uv.y;
    viewH = sqrt(rho*rho + uBotR*uBotR);

    float dMin = uTopR - viewH;
    float dMax = rho + H;
    float d    = dMin + uv.x * (dMax - dMin);
    // Guard against viewH * d = 0
    if (viewH * d < 1e-9) {
        cosAngle = 1.0;
    } else {
        cosAngle = clamp((H*H - rho*rho - d*d) / (2.0*viewH*d), -1.0, 1.0);
    }
}

// Atmosphere density at altitude (km above sea level).
vec3 sampleAtmosDensity(float h) {
    float hKm      = max(0.0, h);  // km above surface
    float rayleigh = exp(uRayleighExpScale * hKm);
    float mie      = exp(uMieDensityExpScale * hKm);
    // Ozone: tent function peaking at 25 km
    float ozone    = max(0.0, 1.0 - abs(hKm - 25.0) / 15.0);
    return vec3(rayleigh, mie, ozone);
}

// Extinction (scattering + absorption) at a density sample.
vec3 extinctionFromDensity(vec3 density) {
    vec3 rayleighExt = uRayleighScattering * density.x;
    vec3 mieExt      = (vec3(uMieScattering) + vec3(uMieAbsorption)) * density.y;
    vec3 ozoneExt    = uAbsorptionExtinction * density.z;
    return rayleighExt + mieExt + ozoneExt;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(uTransmittanceLUT);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 uv = (vec2(coord) + 0.5) / vec2(size);

    float viewH, cosAngle;
    uvToAtmosParams(uv, viewH, cosAngle);

    // Ray origin is on a sphere of radius viewH, looking along cosAngle.
    vec3  orig = vec3(0.0, viewH, 0.0);
    vec3  dir  = vec3(sqrt(max(0.0, 1.0 - cosAngle*cosAngle)), cosAngle, 0.0);

    // Find exit distance (hit top of atmosphere).
    float tMax = raySphereIntersectNearest(orig, dir, uTopR);
    if (tMax < 0.0) {
        imageStore(uTransmittanceLUT, coord, vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }
    // If ray hits ground first, transmittance is zero (blocked).
    float tGround = raySphereIntersectNearest(orig, dir, uBotR);
    if (tGround > 0.0 && tGround < tMax) {
        imageStore(uTransmittanceLUT, coord, vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    const int STEPS = 40;
    float dt         = tMax / float(STEPS);
    vec3  opticalDepth = vec3(0.0);

    for (int i = 0; i < STEPS; i++) {
        float t    = (float(i) + 0.5) * dt;
        vec3  pos  = orig + dir * t;
        float alt  = length(pos) - uBotR;
        vec3  dens = sampleAtmosDensity(alt);
        opticalDepth += extinctionFromDensity(dens) * dt;
    }

    vec3 transmittance = exp(-opticalDepth);
    imageStore(uTransmittanceLUT, coord, vec4(transmittance, 1.0));
}
)GLSL";

// ---------------------------------------------------------------------------
// Multi-Scatter LUT compute shader
// 64-direction Fibonacci sphere, 20 ray-march steps, geometric series approx.
// ---------------------------------------------------------------------------
static const char* s_MultiScatterSrc = R"GLSL(
#version 430 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) writeonly uniform image2D uMultiScatterLUT;

uniform sampler2D uTransmittanceLUT;

uniform float uBotR;
uniform float uTopR;
uniform vec3  uRayleighScattering;
uniform float uRayleighExpScale;
uniform float uMieScattering;
uniform float uMieAbsorption;
uniform float uMieDensityExpScale;
uniform vec3  uAbsorptionExtinction;
uniform vec3  uGroundAlbedo;

// Sample transmittance LUT with Hillaire parameterization.
vec3 sampleTransmittance(float viewH, float cosAngle) {
    float H   = sqrt(max(0.0, uTopR*uTopR - uBotR*uBotR));
    float rho = sqrt(max(0.0, viewH*viewH - uBotR*uBotR));
    float disc = viewH*viewH*(cosAngle*cosAngle - 1.0) + uTopR*uTopR;
    float d    = -viewH*cosAngle + sqrt(max(0.0, disc));
    float dMin = uTopR - viewH;
    float dMax = rho + H;
    float u    = (dMax > dMin + 1e-6) ? (d - dMin) / (dMax - dMin) : 0.0;
    float v    = (H > 1e-6) ? rho / H : 0.0;
    return texture(uTransmittanceLUT, vec2(u, v)).rgb;
}

float raySphereIntersectNearest(vec3 orig, vec3 dir, float R) {
    float b  = dot(orig, dir);
    float c  = dot(orig, orig) - R*R;
    float d  = b*b - c;
    if (d < 0.0) return -1.0;
    float sd = sqrt(d);
    float t1 = -b - sd;
    float t2 = -b + sd;
    if (t1 > 0.0) return t1;
    if (t2 > 0.0) return t2;
    return -1.0;
}

vec3 sampleAtmosDensity(float h) {
    float hKm      = max(0.0, h);
    float rayleigh = exp(uRayleighExpScale * hKm);
    float mie      = exp(uMieDensityExpScale * hKm);
    float ozone    = max(0.0, 1.0 - abs(hKm - 25.0) / 15.0);
    return vec3(rayleigh, mie, ozone);
}

vec3 extinctionFromDensity(vec3 density) {
    vec3 rayleighExt = uRayleighScattering * density.x;
    vec3 mieExt      = (vec3(uMieScattering) + vec3(uMieAbsorption)) * density.y;
    vec3 ozoneExt    = uAbsorptionExtinction * density.z;
    return rayleighExt + mieExt + ozoneExt;
}

vec3 scatteringFromDensity(vec3 density) {
    // Rayleigh + Mie scattering (no absorption)
    return uRayleighScattering * density.x + vec3(uMieScattering) * density.y;
}

// Fibonacci sphere for uniform sampling
const float GOLDEN_RATIO  = 1.6180339887498948482;
const float PI            = 3.14159265358979323846;
const float TWO_PI        = 6.28318530717958647692;

vec3 fibonacciSphere(int i, int n) {
    float theta = acos(1.0 - 2.0*(float(i)+0.5)/float(n));
    float phi   = TWO_PI * float(i) / GOLDEN_RATIO;
    return vec3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
}

// Decode UV: x = sunCosZenith [-1,1], y = viewH [botR, topR]
void uvToParams(vec2 uv, out float viewH, out float sunCosZenith) {
    viewH       = uBotR + uv.y * (uTopR - uBotR);
    sunCosZenith = uv.x * 2.0 - 1.0;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(uMultiScatterLUT);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2  uv = (vec2(coord) + 0.5) / vec2(size);
    float viewH, sunCosZenith;
    uvToParams(uv, viewH, sunCosZenith);

    vec3 sunDir = vec3(0.0, sunCosZenith, sqrt(max(0.0, 1.0 - sunCosZenith*sunCosZenith)));
    vec3 orig   = vec3(0.0, viewH, 0.0);

    const int NUM_DIRS = 64;
    const int STEPS    = 20;

    vec3 L2ndOrder  = vec3(0.0); // Luminance from 2nd-order scattering
    vec3 fms        = vec3(0.0); // "f_ms" denominator accumulator

    float sampleWeight = (4.0 * PI) / float(NUM_DIRS);

    for (int di = 0; di < NUM_DIRS; di++) {
        vec3 rayDir = fibonacciSphere(di, NUM_DIRS);

        // Find tMax: exit atmosphere or hit ground
        float tTop    = raySphereIntersectNearest(orig, rayDir, uTopR);
        float tGround = raySphereIntersectNearest(orig, rayDir, uBotR);
        float tMax    = (tTop > 0.0) ? tTop : 0.0;
        bool  hitGround = (tGround > 0.0 && tGround < tMax);
        if (hitGround) tMax = tGround;
        if (tMax <= 0.0) continue;

        float dt = tMax / float(STEPS);
        vec3  optDepth   = vec3(0.0);
        vec3  dirScatter = vec3(0.0); // luminance from 2nd-order: sun → sample → camera
        vec3  dirFms     = vec3(0.0); // scattering feedback for geometric series

        for (int si = 0; si < STEPS; si++) {
            float t   = (float(si) + 0.5) * dt;
            vec3  pos = orig + rayDir * t;
            float alt = length(pos) - uBotR;
            vec3  dens = sampleAtmosDensity(alt);

            vec3 ext  = extinctionFromDensity(dens);
            vec3 scat = scatteringFromDensity(dens);

            // Transmittance from camera to sample
            vec3 Tr  = exp(-optDepth);
            optDepth += ext * dt;

            // Transmittance from sample to sun
            float viewH  = length(pos);
            float cosSunAtSample = dot(normalize(pos), sunDir);
            vec3 Tsun = sampleTransmittance(viewH, cosSunAtSample);

            // Isotropic phase 1/(4π) for multi-scatter
            float isoPhaseDt = dt / (4.0 * PI);

            // Luminance: sun → sample (attenuated by Tsun) → along this dir to camera
            dirScatter += Tr * scat * Tsun * isoPhaseDt;

            // fms: scattering that can re-enter the system (isotropic, no sun)
            dirFms     += Tr * scat * isoPhaseDt;
        }

        // Ground reflection (Lambertian)
        if (hitGround) {
            vec3  groundPos = orig + rayDir * tMax;
            float groundH   = length(groundPos);
            float cosSunG   = dot(normalize(groundPos), sunDir);
            if (cosSunG > 0.0) {
                vec3 Tr   = exp(-optDepth);
                vec3 Tsun = sampleTransmittance(groundH, cosSunG);
                dirScatter += Tr * Tsun * uGroundAlbedo * (cosSunG / PI);
            }
        }

        L2ndOrder += dirScatter * sampleWeight;
        fms       += dirFms    * sampleWeight;
    }

    // Geometric series: L_ms = L_2nd / (1 - fms)
    vec3 denom = max(vec3(0.001), vec3(1.0) - fms);
    vec3 multiScatter = L2ndOrder / denom;

    imageStore(uMultiScatterLUT, coord, vec4(multiScatter, 1.0));
}
)GLSL";

// ---------------------------------------------------------------------------
// Sky-View LUT compute shader
// 192×108 RGBA16F, latitude/longitude with non-linear horizon mapping.
// Rebuilt every frame (camera / sun movement).
// ---------------------------------------------------------------------------
static const char* s_SkyViewSrc = R"GLSL(
#version 430 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) writeonly uniform image2D uSkyViewLUT;

uniform sampler2D uTransmittanceLUT;
uniform sampler2D uMultiScatterLUT;

uniform float uBotR;
uniform float uTopR;
uniform vec3  uRayleighScattering;
uniform float uRayleighExpScale;
uniform float uMieScattering;
uniform float uMieAbsorption;
uniform float uMieAnisotropy;
uniform float uMieDensityExpScale;
uniform vec3  uAbsorptionExtinction;
uniform vec3  uGroundAlbedo;
uniform vec3  uSunDir;    // normalized, world-space toward sun
uniform float uCamHeight; // camera altitude in km (atmosphere-space Y)

const float PI     = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;

float raySphereIntersectNearest(vec3 orig, vec3 dir, float R) {
    float b  = dot(orig, dir);
    float c  = dot(orig, orig) - R*R;
    float d  = b*b - c;
    if (d < 0.0) return -1.0;
    float sd = sqrt(d);
    float t1 = -b - sd;
    float t2 = -b + sd;
    if (t1 > 0.0) return t1;
    if (t2 > 0.0) return t2;
    return -1.0;
}

vec3 sampleAtmosDensity(float h) {
    float hKm = max(0.0, h);
    return vec3(
        exp(uRayleighExpScale   * hKm),
        exp(uMieDensityExpScale * hKm),
        max(0.0, 1.0 - abs(hKm - 25.0) / 15.0)
    );
}

vec3 extinctionFromDensity(vec3 d) {
    return uRayleighScattering * d.x
         + (vec3(uMieScattering) + vec3(uMieAbsorption)) * d.y
         + uAbsorptionExtinction * d.z;
}

vec3 scatteringFromDensity(vec3 d) {
    return uRayleighScattering * d.x + vec3(uMieScattering) * d.y;
}

// Rayleigh phase function (isotropic correction factor = 3/16pi * (1+cos^2)).
float phaseRayleigh(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta*cosTheta);
}

// Henyey-Greenstein phase for Mie.
float phaseMie(float cosTheta, float g) {
    float g2  = g*g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta*cosTheta);
    float den = 8.0 * PI * (2.0 + g2) * pow(abs(1.0 + g2 - 2.0*g*cosTheta), 1.5);
    return num / max(den, 1e-7);
}

// Sample transmittance LUT using Hillaire parameterization.
vec3 sampleTransmittance(float viewH, float cosAngle) {
    float H   = sqrt(max(0.0, uTopR*uTopR - uBotR*uBotR));
    float rho = sqrt(max(0.0, viewH*viewH - uBotR*uBotR));
    float disc = viewH*viewH*(cosAngle*cosAngle - 1.0) + uTopR*uTopR;
    float d    = -viewH*cosAngle + sqrt(max(0.0, disc));
    float dMin = uTopR - viewH;
    float dMax = rho + H;
    float u    = (dMax > dMin + 1e-6) ? (d - dMin) / (dMax - dMin) : 0.0;
    float v    = (H > 1e-6) ? rho / H : 0.0;
    return texture(uTransmittanceLUT, vec2(u, v)).rgb;
}

// Sample multi-scatter LUT.
vec3 sampleMultiScatter(float viewH, float sunCosZenith) {
    float u = sunCosZenith * 0.5 + 0.5;
    float v = clamp((viewH - uBotR) / (uTopR - uBotR), 0.0, 1.0);
    return texture(uMultiScatterLUT, vec2(u, v)).rgb;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(uSkyViewLUT);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 uv = (vec2(coord) + 0.5) / vec2(size);

    // Decode UV → (longitude, latitude) with non-linear vertical mapping.
    float lon = uv.x * TWO_PI;  // 0 → 2π
    float lat;
    if (uv.y < 0.5) {
        float t = 1.0 - 2.0*uv.y;         // t: 0 at horizon, 1 at bottom
        lat = -(PI*0.5) * t*t;            // negative = below horizon
    } else {
        float t = 2.0*uv.y - 1.0;         // t: 0 at horizon, 1 at top
        lat = (PI*0.5) * t*t;             // positive = above horizon
    }

    // View direction (Y-up)
    vec3 viewDir = vec3(cos(lat)*sin(lon), sin(lat), cos(lat)*cos(lon));

    // Camera position in atmosphere space (clamped above ground)
    float camH   = max(uBotR + 0.001, uCamHeight);
    vec3  camPos = vec3(0.0, camH, 0.0);

    // Ray march from camera along viewDir
    float tTop    = raySphereIntersectNearest(camPos, viewDir, uTopR);
    float tGround = raySphereIntersectNearest(camPos, viewDir, uBotR);

    // If ray doesn't hit the atmosphere at all, output black.
    if (tTop < 0.0) {
        imageStore(uSkyViewLUT, coord, vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    float tMax    = tTop;

    const int STEPS = 32;
    float dt = tMax / float(STEPS);

    vec3 L         = vec3(0.0); // accumulated luminance
    vec3 optDepth  = vec3(0.0); // accumulated optical depth from camera

    float cosSun = dot(viewDir, uSunDir);

    for (int i = 0; i < STEPS; i++) {
        float t   = (float(i) + 0.5) * dt;
        vec3  pos = camPos + viewDir * t;
        float alt = max(0.0, length(pos) - uBotR);
        vec3  dens = sampleAtmosDensity(alt);

        vec3 ext  = extinctionFromDensity(dens);
        vec3 scat = scatteringFromDensity(dens);

        // Transmittance from camera to this sample
        vec3 Tcam = exp(-optDepth);
        optDepth += ext * dt;

        float viewH     = length(pos);
        float cosSunL   = dot(normalize(pos), uSunDir);

        // Transmittance from sample to sun
        vec3 Tsun = sampleTransmittance(viewH, cosSunL);

        // Phase functions
        float pR = phaseRayleigh(cosSun);
        float pM = phaseMie(cosSun, uMieAnisotropy);

        vec3 scatR = uRayleighScattering * dens.x * pR;
        vec3 scatM = vec3(uMieScattering) * dens.y * pM;

        // Single scattering contribution (sun as point light)
        vec3 singleScatter = (scatR + scatM) * Tsun;

        // Multi-scatter contribution
        vec3 ms = sampleMultiScatter(viewH, cosSunL);
        vec3 multiScatter = scat * ms;

        L += Tcam * (singleScatter + multiScatter) * dt;
    }

    imageStore(uSkyViewLUT, coord, vec4(L, 1.0));
}
)GLSL";

// ---------------------------------------------------------------------------
// Fullscreen triangle vertex shader
// NDC depth is forced to 0.9999 (behind all normal geometry at GL_LEQUAL).
// ---------------------------------------------------------------------------
static const char* s_QuadVertSrc = R"GLSL(
#version 430 core
out vec2 vUV;
out vec3 vRayDir;

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;

void main() {
    // Generate fullscreen triangle from gl_VertexID (no VBO needed)
    vec2 pos;
    if      (gl_VertexID == 0) { pos = vec2(-1.0, -1.0); vUV = vec2(0.0, 0.0); }
    else if (gl_VertexID == 1) { pos = vec2( 3.0, -1.0); vUV = vec2(2.0, 0.0); }
    else                        { pos = vec2(-1.0,  3.0); vUV = vec2(0.0, 2.0); }

    // Reconstruct world-space ray direction
    vec4 ndcNear = vec4(pos, -1.0, 1.0);
    vec4 wNear   = uInvViewProj * ndcNear;
    wNear /= wNear.w;

    vec4 ndcFar  = vec4(pos,  1.0, 1.0);
    vec4 wFar    = uInvViewProj * ndcFar;
    wFar  /= wFar.w;

    vRayDir = normalize(wFar.xyz - wNear.xyz);

    // Force depth to 0.9999 so sky renders behind everything (GL_LEQUAL)
    gl_Position = vec4(pos, 0.9999, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Fullscreen triangle fragment shader
// Samples sky-view LUT, adds sun disk, applies exposure + gamma.
// ---------------------------------------------------------------------------
static const char* s_QuadFragSrc = R"GLSL(
#version 430 core
in  vec2 vUV;
in  vec3 vRayDir;
out vec4 fragColor;

uniform sampler2D uSkyViewLUT;
uniform sampler2D uTransmittanceLUT;

uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;
uniform float uSunAngularRadius; // half-angle, radians
uniform float uExposure;
uniform float uBotR;
uniform float uTopR;
uniform float uCamHeight; // km

const float PI     = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;

// Sample transmittance LUT for sun disk.
vec3 sampleTransmittance(float viewH, float cosAngle) {
    float H   = sqrt(max(0.0, uTopR*uTopR - uBotR*uBotR));
    float rho = sqrt(max(0.0, viewH*viewH - uBotR*uBotR));
    float disc = viewH*viewH*(cosAngle*cosAngle - 1.0) + uTopR*uTopR;
    float d    = -viewH*cosAngle + sqrt(max(0.0, disc));
    float dMin = uTopR - viewH;
    float dMax = rho + H;
    float u    = (dMax > dMin + 1e-6) ? (d - dMin) / (dMax - dMin) : 0.0;
    float v    = (H > 1e-6) ? rho / H : 0.0;
    return texture(uTransmittanceLUT, vec2(u, v)).rgb;
}

// Invert the non-linear latitude mapping to get sky-view UV.
vec2 dirToSkyViewUV(vec3 dir) {
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float lon = atan(dir.x, dir.z);   // atan2(x, z)
    if (lon < 0.0) lon += TWO_PI;

    float u = lon / TWO_PI;

    // Invert the non-linear latitude mapping.
    // Below horizon: lat ∈ [-π/2, 0], v ∈ [0, 0.5]
    //   lat = -(π/2) * t²  →  t = sqrt(-lat / (π/2)),  v = 0.5*(1 - t)
    // Above horizon: lat ∈ [0, π/2], v ∈ [0.5, 1]
    //   lat = (π/2) * t²   →  t = sqrt(lat / (π/2)),   v = 0.5 + 0.5*t
    float halfPi = PI * 0.5;
    float v;
    if (lat < 0.0) {
        float t = sqrt(clamp(-lat / halfPi, 0.0, 1.0));
        v = 0.5 * (1.0 - t);
    } else {
        float t = sqrt(clamp(lat / halfPi, 0.0, 1.0));
        v = 0.5 + 0.5 * t;
    }

    return vec2(u, v);
}

void main() {
    vec3 dir = normalize(vRayDir);

    // Sample sky-view LUT
    vec2  skyUV   = dirToSkyViewUV(dir);
    vec3  skyColor = texture(uSkyViewLUT, skyUV).rgb;

    // Sun disk
    float cosToSun  = dot(dir, uSunDir);
    float cosDisk   = cos(uSunAngularRadius);
    if (cosToSun > cosDisk) {
        float camH  = max(uBotR + 0.001, uCamHeight);
        float cosSun = dot(normalize(vec3(0.0, camH, 0.0)), uSunDir);
        vec3 Tsun   = sampleTransmittance(camH, cosSun);
        // Smooth limb darkening at edge of disk
        float edge  = (cosToSun - cosDisk) / (1.0 - cosDisk);
        skyColor   += uSunColor * uSunIntensity * Tsun * smoothstep(0.0, 0.05, edge);
    }

    // Exposure-based tonemapping: 1 - exp(-L * exposure) keeps the sky from
    // blowing out to white while still making the sun disk punch through.
    vec3 mapped = vec3(1.0) - exp(-skyColor * uExposure);
    mapped = pow(max(mapped, vec3(0.0)), vec3(1.0 / 2.2));

    fragColor = vec4(mapped, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Sky-to-Cubemap compute shader
// Renders the atmosphere into a 64x64 cubemap for IBL.
// ---------------------------------------------------------------------------
static const char* s_BakeCubemapSrc = R"GLSL(
#version 430 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) writeonly uniform imageCube uOutCubemap;

uniform sampler2D uSkyViewLUT;

const float PI     = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;

vec2 dirToSkyViewUV(vec3 dir) {
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float lon = atan(dir.x, dir.z);
    if (lon < 0.0) lon += TWO_PI;
    float u = lon / TWO_PI;
    float halfPi = PI * 0.5;
    float v;
    if (lat < 0.0) {
        float t = sqrt(clamp(-lat / halfPi, 0.0, 1.0));
        v = 0.5 * (1.0 - t);
    } else {
        float t = sqrt(clamp(lat / halfPi, 0.0, 1.0));
        v = 0.5 + 0.5 * t;
    }
    return vec2(u, v);
}

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    ivec2 size  = imageSize(uOutCubemap);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 uv = (vec2(coord.xy) + 0.5) / vec2(size);
    vec2 p  = uv * 2.0 - 1.0;

    vec3 dir;
    // Map coord.z to cubemap face direction
    if      (coord.z == 0) dir = vec3( 1.0, -p.y, -p.x); // +X
    else if (coord.z == 1) dir = vec3(-1.0, -p.y,  p.x); // -X
    else if (coord.z == 2) dir = vec3( p.x,  1.0,  p.y); // +Y
    else if (coord.z == 3) dir = vec3( p.x, -1.0, -p.y); // -Y
    else if (coord.z == 4) dir = vec3( p.x, -p.y,  1.0); // +Z
    else                   dir = vec3(-p.x, -p.y, -1.0); // -Z
    dir = normalize(dir);

    vec2 skyUV = dirToSkyViewUV(dir);
    vec3 col   = texture(uSkyViewLUT, skyUV).rgb;

    imageStore(uOutCubemap, coord, vec4(col, 1.0));
}
)GLSL";

// ---------------------------------------------------------------------------
// Helper: compile a single shader stage
// ---------------------------------------------------------------------------
static GLuint sa_CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        const char* typeName =
            (type == GL_COMPUTE_SHADER)        ? "COMPUTE"   :
            (type == GL_VERTEX_SHADER)         ? "VERT"      :
            (type == GL_FRAGMENT_SHADER)       ? "FRAG"      : "UNKNOWN";
        LOG_E("[SkyAtmosphere] %s shader compile error:\n%s", typeName, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// ---------------------------------------------------------------------------
// Helper: build a compute shader program
// ---------------------------------------------------------------------------
static GLuint sa_BuildComputeProg(const char* src, const char* name) {
    GLuint cs = sa_CompileShader(GL_COMPUTE_SHADER, src);
    if (!cs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    glDeleteShader(cs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOG_E("[SkyAtmosphere] Compute program '%s' link error:\n%s", name, log);
        glDeleteProgram(prog);
        return 0;
    }
    LOG_I("[SkyAtmosphere] Compiled compute program: %s", name);
    return prog;
}

// ---------------------------------------------------------------------------
// Helper: build a vert+frag shader program
// ---------------------------------------------------------------------------
static GLuint sa_BuildVertFragProg(const char* vertSrc, const char* fragSrc, const char* name) {
    GLuint vs = sa_CompileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint fs = sa_CompileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOG_E("[SkyAtmosphere] Vert/Frag program '%s' link error:\n%s", name, log);
        glDeleteProgram(prog);
        return 0;
    }
    LOG_I("[SkyAtmosphere] Compiled vert/frag program: %s", name);
    return prog;
}

// ---------------------------------------------------------------------------
// Helper: allocate a 2D RGBA16F texture
// ---------------------------------------------------------------------------
static GLuint sa_MakeLUT2D(int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ---------------------------------------------------------------------------
// Upload atmosphere uniforms to a compute program
// (transmittance and multi-scatter share the same layout)
// ---------------------------------------------------------------------------
static void sa_UploadAtmosUniforms(GLuint prog, SkyAtmosphereNodeData* d) {
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "uBotR"),             d->bottomRadius);
    glUniform1f(glGetUniformLocation(prog, "uTopR"),             d->topRadius);
    glUniform3fv(glGetUniformLocation(prog, "uRayleighScattering"), 1, (float*)&d->rayleighScattering);
    glUniform1f(glGetUniformLocation(prog, "uRayleighExpScale"), d->rayleighDensityExpScale);
    glUniform1f(glGetUniformLocation(prog, "uMieScattering"),    d->mieScattering);
    glUniform1f(glGetUniformLocation(prog, "uMieAbsorption"),    d->mieAbsorption);
    glUniform1f(glGetUniformLocation(prog, "uMieDensityExpScale"), d->mieDensityExpScale);
    glUniform3fv(glGetUniformLocation(prog, "uAbsorptionExtinction"), 1, (float*)&d->absorptionExtinction);
    glUniform3fv(glGetUniformLocation(prog, "uGroundAlbedo"),    1, (float*)&d->groundAlbedo);
}

// ---------------------------------------------------------------------------
// Rebuild static LUTs (transmittance + multi-scatter)
// Called on first init and whenever lutsDirty is set.
// ---------------------------------------------------------------------------
void sg_RebuildSkyAtmosphereStaticLUTs(SceneNode* node) {
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;

    // --- Transmittance LUT (256×64) ---
    {
        GLuint prog = d->transmittanceProg;
        glUseProgram(prog);

        glUniform1f(glGetUniformLocation(prog, "uBotR"),             d->bottomRadius);
        glUniform1f(glGetUniformLocation(prog, "uTopR"),             d->topRadius);
        glUniform3fv(glGetUniformLocation(prog, "uRayleighScattering"), 1, (float*)&d->rayleighScattering);
        glUniform1f(glGetUniformLocation(prog, "uRayleighExpScale"), d->rayleighDensityExpScale);
        glUniform1f(glGetUniformLocation(prog, "uMieScattering"),    d->mieScattering);
        glUniform1f(glGetUniformLocation(prog, "uMieAbsorption"),    d->mieAbsorption);
        glUniform1f(glGetUniformLocation(prog, "uMieDensityExpScale"), d->mieDensityExpScale);
        glUniform3fv(glGetUniformLocation(prog, "uAbsorptionExtinction"), 1, (float*)&d->absorptionExtinction);

        glBindImageTexture(0, d->transmittanceLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute((256 + 7) / 8, (64 + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    // --- Multi-Scatter LUT (32×32) ---
    {
        GLuint prog = d->multiScatterProg;
        glUseProgram(prog);

        glUniform1f(glGetUniformLocation(prog, "uBotR"),             d->bottomRadius);
        glUniform1f(glGetUniformLocation(prog, "uTopR"),             d->topRadius);
        glUniform3fv(glGetUniformLocation(prog, "uRayleighScattering"), 1, (float*)&d->rayleighScattering);
        glUniform1f(glGetUniformLocation(prog, "uRayleighExpScale"), d->rayleighDensityExpScale);
        glUniform1f(glGetUniformLocation(prog, "uMieScattering"),    d->mieScattering);
        glUniform1f(glGetUniformLocation(prog, "uMieAbsorption"),    d->mieAbsorption);
        glUniform1f(glGetUniformLocation(prog, "uMieDensityExpScale"), d->mieDensityExpScale);
        glUniform3fv(glGetUniformLocation(prog, "uAbsorptionExtinction"), 1, (float*)&d->absorptionExtinction);
        glUniform3fv(glGetUniformLocation(prog, "uGroundAlbedo"),    1, (float*)&d->groundAlbedo);

        // Multi-scatter also reads the transmittance LUT
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d->transmittanceLUT);
        glUniform1i(glGetUniformLocation(prog, "uTransmittanceLUT"), 1);

        glBindImageTexture(0, d->multiScatterLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        // Image2D binding 1 is read-only — multi-scatter shader uses sampler2D,
        // not image2D, for transmittance. Only write-binding for output.
        glDispatchCompute((32 + 7) / 8, (32 + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    glUseProgram(0);
    d->lutsDirty = false;
    LOG_I("[SkyAtmosphere] Static LUTs rebuilt.");
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void sg_UpdateSkyAtmosphereIBL(SceneNode* node);

void sg_InitSkyAtmosphereNode(SceneNode* node) {
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;

    // Compile shaders (only once; use static flag so re-inits don't recompile)
    static bool s_ShadersCompiled = false;
    static GLuint s_TransmittanceProg = 0;
    static GLuint s_MultiScatterProg  = 0;
    static GLuint s_SkyViewProg       = 0;
    static GLuint s_BakeCubemapProg   = 0;
    static GLuint s_QuadProg          = 0;

    if (!s_ShadersCompiled) {
        s_TransmittanceProg = sa_BuildComputeProg(s_TransmittanceSrc, "SkyAtmos_Transmittance");
        s_MultiScatterProg  = sa_BuildComputeProg(s_MultiScatterSrc,  "SkyAtmos_MultiScatter");
        s_SkyViewProg       = sa_BuildComputeProg(s_SkyViewSrc,       "SkyAtmos_SkyView");
        s_BakeCubemapProg   = sa_BuildComputeProg(s_BakeCubemapSrc,   "SkyAtmos_BakeCubemap");
        s_QuadProg          = sa_BuildVertFragProg(s_QuadVertSrc, s_QuadFragSrc, "SkyAtmos_Quad");
        s_ShadersCompiled   = true;
    }

    d->transmittanceProg = s_TransmittanceProg;
    d->multiScatterProg  = s_MultiScatterProg;
    d->skyViewProg       = s_SkyViewProg;
    d->bakeToCubemapProg = s_BakeCubemapProg;
    d->quadProg          = s_QuadProg;

    // Create LUT textures
    d->transmittanceLUT = sa_MakeLUT2D(256, 64);
    d->multiScatterLUT  = sa_MakeLUT2D(32,  32);
    d->skyViewLUT       = sa_MakeLUT2D(192, 108);

    // Cache quad uniform locations
    if (d->quadProg) {
        d->quadSkyViewLoc       = glGetUniformLocation(d->quadProg, "uSkyViewLUT");
        d->quadTransmittanceLoc = glGetUniformLocation(d->quadProg, "uTransmittanceLUT");
        d->quadInvViewProjLoc   = glGetUniformLocation(d->quadProg, "uInvViewProj");
        d->quadCamPosLoc        = glGetUniformLocation(d->quadProg, "uCamPos");
        d->quadSunDirLoc        = glGetUniformLocation(d->quadProg, "uSunDir");
        d->quadSunColorLoc      = glGetUniformLocation(d->quadProg, "uSunColor");
        d->quadSunIntensityLoc  = glGetUniformLocation(d->quadProg, "uSunIntensity");
        d->quadSunRadiusLoc     = glGetUniformLocation(d->quadProg, "uSunAngularRadius");
        d->quadExposureLoc      = glGetUniformLocation(d->quadProg, "uExposure");
        d->quadBotRLoc          = glGetUniformLocation(d->quadProg, "uBotR");
        d->quadTopRLoc          = glGetUniformLocation(d->quadProg, "uTopR");
        d->quadCamHeightLoc     = glGetUniformLocation(d->quadProg, "uCamHeight");
    }

    // Create empty VAO for fullscreen triangle (gl_VertexID trick, no VBO)
    glGenVertexArrays(1, &d->emptyVAO);

    d->lutsDirty    = true;
    d->prevIBLSunDir = vec3(0.0f, 0.0f, 0.0f); // zero = never baked

    // Build static LUTs immediately
    sg_RebuildSkyAtmosphereStaticLUTs(node);
    // IBL bake happens on the first render call once sky-view LUT is populated.

    LOG_I("[SkyAtmosphere] Node '%s' initialized.", node->name ? node->name : "?");
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------
void sg_RenderSkyAtmosphereNode(SceneNode* node, mat4 view, mat4 proj) {
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;

    if (!d->transmittanceProg || !d->skyViewProg || !d->quadProg) return;

    // Rebuild static LUTs if parameters changed
    if (d->lutsDirty) {
        sg_RebuildSkyAtmosphereStaticLUTs(node);
    }

    // --- Determine camera position ---
    vec3 camWorldPos = vec3(0.0f, 0.0f, 0.0f);
    if (g_ActiveCameraNode) {
        Camera* cam = sg_Camera(g_ActiveCameraNode);
        if (cam) camWorldPos = cam->position;
    }

    // Convert world Y to atmosphere km
    float camHeightKm = d->bottomRadius + fmaxf(0.0f, camWorldPos[1] * d->worldScale);

    // Normalize sun direction (defensive)
    vec3 sunDir = d->sunDirection;
    float sunLen = sqrtf(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    if (sunLen > 1e-5f) { sunDir[0] /= sunLen; sunDir[1] /= sunLen; sunDir[2] /= sunLen; }
    else                { sunDir = vec3(0.0f, 1.0f, 0.0f); }

    // --- Sky-View LUT compute pass (per-frame) ---
    {
        GLuint prog = d->skyViewProg;
        glUseProgram(prog);

        glUniform1f(glGetUniformLocation(prog, "uBotR"),   d->bottomRadius);
        glUniform1f(glGetUniformLocation(prog, "uTopR"),   d->topRadius);
        glUniform3fv(glGetUniformLocation(prog, "uRayleighScattering"), 1, (float*)&d->rayleighScattering);
        glUniform1f(glGetUniformLocation(prog, "uRayleighExpScale"),    d->rayleighDensityExpScale);
        glUniform1f(glGetUniformLocation(prog, "uMieScattering"),       d->mieScattering);
        glUniform1f(glGetUniformLocation(prog, "uMieAbsorption"),       d->mieAbsorption);
        glUniform1f(glGetUniformLocation(prog, "uMieAnisotropy"),       d->mieAnisotropy);
        glUniform1f(glGetUniformLocation(prog, "uMieDensityExpScale"),  d->mieDensityExpScale);
        glUniform3fv(glGetUniformLocation(prog, "uAbsorptionExtinction"), 1, (float*)&d->absorptionExtinction);
        glUniform3fv(glGetUniformLocation(prog, "uGroundAlbedo"),       1, (float*)&d->groundAlbedo);
        glUniform3fv(glGetUniformLocation(prog, "uSunDir"),             1, (float*)&sunDir);
        glUniform1f(glGetUniformLocation(prog, "uCamHeight"),           camHeightKm);

        // Bind transmittance and multi-scatter as samplers (not image units)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, d->transmittanceLUT);
        glUniform1i(glGetUniformLocation(prog, "uTransmittanceLUT"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d->multiScatterLUT);
        glUniform1i(glGetUniformLocation(prog, "uMultiScatterLUT"), 1);

        glBindImageTexture(0, d->skyViewLUT, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute((192 + 7) / 8, (108 + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    // Bake sky-view LUT into IBL cubemap only when the sun has moved enough to matter.
    // cos(2°) ≈ 0.9994 — tight enough to track sun arcs, loose enough to avoid every-frame bakes.
    // prevIBLSunDir == zero means "never baked" → force first bake after sky-view is populated.
    {
        float prevLen = sqrtf(d->prevIBLSunDir[0]*d->prevIBLSunDir[0] +
                              d->prevIBLSunDir[1]*d->prevIBLSunDir[1] +
                              d->prevIBLSunDir[2]*d->prevIBLSunDir[2]);
        float cosAngle = (prevLen > 0.5f)
            ? (d->prevIBLSunDir[0]*sunDir[0] + d->prevIBLSunDir[1]*sunDir[1] + d->prevIBLSunDir[2]*sunDir[2])
            : -1.0f; // force bake when prevIBLSunDir is zeroed (first frame)
        if (cosAngle < 0.9994f) {
            sg_UpdateSkyAtmosphereIBL(node);
            d->prevIBLSunDir = sunDir;
        }
    }

    // --- Compute inverse view-projection ---
    mat4 vp    = proj * view;
    // Invert vp using cofactor expansion (vmath may not have inverse; do it manually)
    // We'll use the standard 4×4 inverse. Since vmath uses column-major mat4,
    // extract as float array and compute the inverse.
    float m[16], inv[16];
    // vmath mat4: m[col][row], so to get row-major for the inversion loop:
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            m[col*4 + row] = vp[col][row];

    // Compute 4×4 inverse (row-major layout)
    float det;
    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    mat4 invVP = mat4::identity();
    if (fabsf(det) > 1e-9f) {
        float invDet = 1.0f / det;
        for (int i = 0; i < 16; i++) inv[i] *= invDet;
        // Store back into vmath mat4 (column-major)
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                invVP[col][row] = inv[col*4 + row];
    }

    // --- Fullscreen triangle render pass ---
    {
        // Preserve existing GL state
        GLboolean depthWrite;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        GLint depthFunc;
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);

        // Sky renders at z=0.9999, behind everything (GL_LEQUAL)
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        glUseProgram(d->quadProg);

        // Bind LUT textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, d->skyViewLUT);
        glUniform1i(d->quadSkyViewLoc, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, d->transmittanceLUT);
        glUniform1i(d->quadTransmittanceLoc, 1);

        // Matrices and other uniforms
        glUniformMatrix4fv(d->quadInvViewProjLoc, 1, GL_FALSE, (float*)&invVP);
        glUniform3fv(d->quadCamPosLoc,        1, (float*)&camWorldPos);
        glUniform3fv(d->quadSunDirLoc,         1, (float*)&sunDir);
        glUniform3fv(d->quadSunColorLoc,       1, (float*)&d->sunColor);
        glUniform1f(d->quadSunIntensityLoc,    d->sunIntensity);
        glUniform1f(d->quadSunRadiusLoc,       d->sunAngularRadius);
        glUniform1f(d->quadExposureLoc,        d->exposure);
        glUniform1f(d->quadBotRLoc,            d->bottomRadius);
        glUniform1f(d->quadTopRLoc,            d->topRadius);
        glUniform1f(d->quadCamHeightLoc,       camHeightKm);

        glBindVertexArray(d->emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Restore state
        glDepthMask(depthWrite);
        glDepthFunc(depthFunc);
        if (blendEnabled) glEnable(GL_BLEND);
        glUseProgram(0);
    }
}

// ---------------------------------------------------------------------------
// Free
// ---------------------------------------------------------------------------
void sg_FreeSkyAtmosphereNode(SceneNode* node) {
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;

    if (d->transmittanceLUT)  { glDeleteTextures(1, &d->transmittanceLUT);  d->transmittanceLUT  = 0; }
    if (d->multiScatterLUT)   { glDeleteTextures(1, &d->multiScatterLUT);   d->multiScatterLUT   = 0; }
    if (d->skyViewLUT)        { glDeleteTextures(1, &d->skyViewLUT);        d->skyViewLUT        = 0; }
    if (d->atmosphereCubemap) { glDeleteTextures(1, &d->atmosphereCubemap); d->atmosphereCubemap = 0; }
    if (d->emptyVAO)          { glDeleteVertexArrays(1, &d->emptyVAO);      d->emptyVAO          = 0; }

    // If atmosphere was the IBL source (no skybox present), clear the shared IBL maps.
    extern unsigned int envCubemap;
    if (envCubemap == 0) {
        extern void clearIBLMaps();
        clearIBLMaps();
    }

    // Note: shader programs are shared (static); don't delete them here.
    LOG_I("[SkyAtmosphere] Node '%s' freed.", node->name ? node->name : "?");
}

// ---------------------------------------------------------------------------
// PBR IBL Integration
// ---------------------------------------------------------------------------
// Atmosphere owns its own cubemap (atmosphereCubemap) and never touches the
// skybox-owned envCubemap.  When a skybox node is present (envCubemap != 0)
// skybox IBL takes priority and this function is a no-op.
// ---------------------------------------------------------------------------
extern unsigned int envCubemap;   // owned by skybox.cpp — read-only here
extern void generateIBLMaps(unsigned int cubemap);

void sg_UpdateSkyAtmosphereIBL(SceneNode* node) {
    SkyAtmosphereNodeData* d = &node->data.skyAtmosphere;
    if (!d->bakeToCubemapProg || !d->skyViewLUT) return;

    // Skybox IBL takes priority — don't overwrite it.
    if (envCubemap != 0) return;

    const int CUBE_RES = 64;

    // Allocate the atmosphere-owned cubemap once.
    if (d->atmosphereCubemap == 0) {
        glGenTextures(1, &d->atmosphereCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, d->atmosphereCubemap);
        for (int i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
                         CUBE_RES, CUBE_RES, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }

    // Bake sky-view LUT into the atmosphere cubemap via compute shader.
    glUseProgram(d->bakeToCubemapProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, d->skyViewLUT);
    glUniform1i(glGetUniformLocation(d->bakeToCubemapProg, "uSkyViewLUT"), 0);
    glBindImageTexture(0, d->atmosphereCubemap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(CUBE_RES / 8, CUBE_RES / 8, 6);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    glBindTexture(GL_TEXTURE_CUBE_MAP, d->atmosphereCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    generateIBLMaps(d->atmosphereCubemap);
    LOG_I("[SkyAtmosphere] IBL maps updated from atmosphere.");
}
