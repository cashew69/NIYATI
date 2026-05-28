// Jai Shree Ram!!!
// Volumetric Cloud — Standalone Export Version (Upgraded)
//
// Single self-contained C++ file. All shaders are embedded as string literals.
// Features: 3D Perlin-Worley noise (CPU baked), Nubis density model,
// GPU-baked Weather Maps, TAA with Reprojection, Advanced Lighting.
//
// Build:
//   g++ volumetricExportVersion.cpp -std=c++17 -O2 -I../../dependancies -lX11 -lGL -lGLEW -o vcloud
//   ./vcloud

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <chrono>
#include <algorithm>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "vmath.h"
using namespace vmath;

// ============================================================================
// Window / platform globals
// ============================================================================

#define WIN_W 1280
#define WIN_H  720

Display*     gDisplay   = NULL;
XVisualInfo* gVisInfo   = NULL;
Window       gWindow;
Colormap     gColormap;
GLXContext   gGLXCtx    = NULL;
GLXFBConfig  gFBCfg;
bool         gFullscreen = false;
bool         gActive     = false;

typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;

FILE* gpFile     = NULL;
mat4  gProjMatrix;

auto  gStartTime = std::chrono::high_resolution_clock::now();

// ============================================================================
// Camera
// ============================================================================

struct Camera {
    vec3  pos   = vec3(0.0f, 80.0f, 250.0f);
    float yaw   = 3.14159f; // face -Z
    float pitch = -0.15f;
    float speed = 200.0f;
};
Camera gCam;

bool gKey[256]     = {};
bool gKeyLeft      = false;
bool gKeyRight     = false;
bool gKeyUp        = false;
bool gKeyDown      = false;

mat4 cameraView() {
    float cp = cosf(gCam.pitch), sp = sinf(gCam.pitch);
    float cy = cosf(gCam.yaw),   sy = sinf(gCam.yaw);
    vec3 dir = vec3(cp * sy, sp, cp * cy);
    vec3 right = normalize(cross(dir, vec3(0,1,0)));
    vec3 up    = cross(right, dir);
    return vmath::lookat(gCam.pos, gCam.pos + dir, up);
}

vec3 cameraForward() {
    float cp = cosf(gCam.pitch), sp = sinf(gCam.pitch);
    return vec3(cp * sinf(gCam.yaw), sp, cp * cosf(gCam.yaw));
}
vec3 cameraRight() {
    return normalize(cross(cameraForward(), vec3(0,1,0)));
}

// ============================================================================
// Noise helpers (CPU side)
// ============================================================================

static float vcn_hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period;
    y = ((y % period) + period) % period;
    z = ((z % period) + period) % period;
    unsigned n = (unsigned)(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013);
    n ^= n >> 13; n *= 0xb5297a4du;
    n ^= n >> 7;  n *= 0x68e31da4u;
    n ^= n >> 11;
    return (float)(n & 0x00ffffffu) / (float)0x01000000u;
}

static float vcn_valueNoise(float x, float y, float z, int period) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), z0 = (int)floorf(z);
    float u = x-x0, v = y-y0, w = z-z0;
    u = u*u*(3.0f-2.0f*u); v = v*v*(3.0f-2.0f*v); w = w*w*(3.0f-2.0f*w);
    auto h  = [&](int a,int b,int c){ return vcn_hash(a,b,c,period,0); };
    auto lp = [](float a,float b,float t){ return a+(b-a)*t; };
    return lp(lp(lp(h(x0,y0,z0),h(x0+1,y0,z0),u),lp(h(x0,y0+1,z0),h(x0+1,y0+1,z0),u),v),
              lp(lp(h(x0,y0,z0+1),h(x0+1,y0,z0+1),u),lp(h(x0,y0+1,z0+1),h(x0+1,y0+1,z0+1),u),v),w);
}

static float vcn_fbm(float x, float y, float z, int baseFreq, int oct) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < oct; i++) {
        int f = baseFreq << i;
        v += a * vcn_valueNoise(x*f, y*f, z*f, f);
        a *= 0.5f;
    }
    return v;
}

static float vcn_worley(float x, float y, float z, int freq) {
    float fx = x*freq, fy = y*freq, fz = z*freq;
    int cx = (int)floorf(fx), cy = (int)floorf(fy), cz = (int)floorf(fz);
    float md = 9.0f;
    for (int dx=-1;dx<=1;dx++) for (int dy=-1;dy<=1;dy++) for (int dz=-1;dz<=1;dz++) {
        int nx=cx+dx, ny=cy+dy, nz=cz+dz;
        float px=(float)nx+vcn_hash(nx,ny,nz,freq,0);
        float py=(float)ny+vcn_hash(nx,ny,nz,freq,1);
        float pz=(float)nz+vcn_hash(nx,ny,nz,freq,2);
        float d=(fx-px)*(fx-px)+(fy-py)*(fy-py)+(fz-pz)*(fz-pz);
        if (d<md) md=d;
    }
    return 1.0f - sqrtf(md)/1.73205f;
}

static float vcn_perlinWorley(float x, float y, float z, int freq) {
    float p = vcn_fbm(x,y,z,freq,3)*1.5f;
    p = p<0.0f?0.0f:(p>1.0f?1.0f:p);
    float w = vcn_worley(x,y,z,freq);
    return w + p*(1.0f-w);
}

static inline float vcn_c01(float v){ return v<0.0f?0.0f:(v>1.0f?1.0f:v); }

// ============================================================================
// Shaders
// ============================================================================

static const char* kComputeSrc = R"GLSL(
#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba16f, binding = 0) writeonly uniform image2D u_outputImage;
layout(std430, binding = 1) readonly buffer CloudSpheresBuffer { vec4 spheres[]; };

uniform int   u_sphereCount;
uniform vec3  u_cameraPos;
uniform mat4  u_view;
uniform mat4  u_proj;
uniform mat4  u_worldMatrix;
uniform float u_time;
uniform float u_densityScale;
uniform int   u_maxSteps;
uniform float u_stepSize;
uniform float u_turbulence;
uniform float u_windSpeed;
uniform vec3  u_boxMin;
uniform vec3  u_boxMax;
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
uniform float u_cloudType;
uniform float u_noiseScale;
uniform float u_detailScale;
uniform sampler3D u_noiseBase;
uniform sampler3D u_noiseDetail;
uniform int       u_useWeatherMap;
uniform sampler2D u_weatherMap;
uniform vec2      u_weatherMapAnchor;
uniform float     u_weatherMapGridExtent;
uniform int   u_enableTAA;
uniform int   u_frameIndex;
uniform float u_taaBlend;
uniform vec3  u_prevCameraPos;
uniform mat4  u_prevView;
uniform mat4  u_prevProj;
uniform sampler2D u_historyTex;

shared mat4  gs_invProj;
shared mat4  gs_invView;
shared float gs_nodeScale;
shared int   gs_tileHits;

float remapC(float v, float oMin, float oMax, float nMin, float nMax) {
    return clamp(nMin + ((v - oMin) / (oMax - oMin)) * (nMax - nMin), min(nMin,nMax), max(nMin,nMax));
}
float stepJitter() {
    float ign = fract(52.9829189 * fract(0.06711056 * float(gl_GlobalInvocationID.x) + 0.00583715 * float(gl_GlobalInvocationID.y)));
    return fract(ign + float(u_frameIndex) * 0.6180339887) * 0.5;
}
vec3 uvwJitter(vec3 p) {
    return vec3(sin(p.x*0.073+p.z*0.051), sin(p.y*0.079+p.x*0.063), sin(p.z*0.067+p.y*0.057)) * 0.018;
}
vec2 weatherMapUV(vec2 xz) {
    float extent = (u_weatherMapGridExtent > 0.0) ? u_weatherMapGridExtent : 1000.0;
    return (xz - u_weatherMapAnchor) / extent * 0.5 + 0.5;
}
float cloudLayerDensity(float h, float cloudType) {
    h = clamp(h, 0.0, 1.0);
    float stratus       = max(0.0, remapC(h,0.00,0.10,0.0,1.0) * remapC(h,0.20,0.30,1.0,0.0));
    float stratocumulus = max(0.0, remapC(h,0.00,0.20,0.0,1.0) * remapC(h,0.40,0.70,1.0,0.0));
    float cumulus       = max(0.0, remapC(h,0.00,0.15,0.0,1.0) * remapC(h,0.70,0.95,1.0,0.0));
    return mix(mix(stratus, stratocumulus, clamp(cloudType*2.0, 0.0, 1.0)), mix(stratocumulus, cumulus, clamp((cloudType-0.5)*2.0, 0.0, 1.0)), cloudType);
}
float sphereCoverage(vec3 p) {
    if (u_sphereCount == 0) return 1.0;
    float sc = gs_nodeScale; float cov = 0.0;
    for (int i=0; i<u_sphereCount; i++) {
        vec3 c = (u_worldMatrix * vec4(spheres[i].xyz, 1.0)).xyz;
        float r = spheres[i].w * sc * 8.0;
        cov = max(cov, 1.0 - smoothstep(0.0, 1.0, length(p.xz - c.xz) / max(r, 0.001)));
    }
    return cov;
}
float cloudDensity(vec3 p) {
    float boxH = max(u_boxMax.y - u_boxMin.y, 1.0); float h = (p.y - u_boxMin.y) / boxH;
    if (h < 0.0 || h > 1.0) return 0.0;
    float wmCoverage = 1.0, wmPrecipMul = 1.0, effType = u_cloudType;
    if (u_useWeatherMap != 0) {
        vec4 wm = texture(u_weatherMap, weatherMapUV(p.xz));
        wmCoverage = wm.r; wmPrecipMul = mix(1.0, 1.7, wm.g); effType = wm.b;
        h = clamp(h / max(mix(0.45, 1.55, wm.a), 0.05), 0.0, 1.0);
    }
    float layerD = cloudLayerDensity(h, effType); if (layerD < 0.001) return 0.0;
    vec3 wind = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float covBase = (u_useWeatherMap != 0) ? 1.0 : ((u_sphereCount == 0) ? clamp(u_coverage,0.0,1.0) : clamp(sphereCoverage(p)*(1.0+u_coverage*0.6),0.0,1.0));
    float coverage = clamp(covBase * wmCoverage + ((u_useWeatherMap != 0) ? clamp(u_coverage*0.15,-0.5,0.5) : 0.0), 0.0, 1.0);
    if (coverage < 0.001) return 0.0;
    vec4 bn = texture(u_noiseBase, (p + wind) * u_noiseScale + uvwJitter(p));
    float base = remapC(bn.r, 1.0 - coverage, 1.0, 0.0, 1.0) * layerD;
    if (base < 0.001) return 0.0;
    base = remapC(base, bn.g*0.625 + bn.b*0.25 + bn.a*0.125, 1.0, 0.0, 1.0);
    if (base < 0.001) return 0.0;
    vec4 dn = texture(u_noiseDetail, (p + wind*1.6) * u_detailScale + uvwJitter(p)*0.5);
    float det = mix(1.0-(dn.r*0.625+dn.g*0.25+dn.b*0.125), dn.r*0.625+dn.g*0.25+dn.b*0.125, clamp(h*6.0,0.0,1.0));
    return remapC(base, det * u_erosion, 1.0, 0.0, 1.0) * u_densityScale * wmPrecipMul;
}
float cloudDensityFast(vec3 p) {
    float boxH = max(u_boxMax.y - u_boxMin.y, 1.0); float h = (p.y - u_boxMin.y) / boxH;
    if (h < 0.0 || h > 1.0) return 0.0;
    float wmCoverage = 1.0, effType = u_cloudType;
    if (u_useWeatherMap != 0) {
        vec4 wm = texture(u_weatherMap, weatherMapUV(p.xz));
        wmCoverage = wm.r; effType = wm.b;
        h = clamp(h / max(mix(0.45, 1.55, wm.a), 0.05), 0.0, 1.0);
    }
    float layerD = cloudLayerDensity(h, effType); if (layerD < 0.001) return 0.0;
    vec3 wind = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float covBase = (u_useWeatherMap != 0) ? 1.0 : ((u_sphereCount == 0) ? clamp(u_coverage,0.0,1.0) : clamp(sphereCoverage(p)*(1.0+u_coverage*0.6),0.0,1.0));
    float coverage = clamp(covBase * wmCoverage + ((u_useWeatherMap != 0) ? clamp(u_coverage*0.15,-0.5,0.5) : 0.0), 0.0, 1.0);
    if (coverage < 0.001) return 0.0;
    vec4 bn = texture(u_noiseBase, (p + wind) * u_noiseScale + uvwJitter(p));
    float base = remapC(bn.r, 1.0 - coverage, 1.0, 0.0, 1.0) * layerD;
    return remapC(base, bn.g*0.625 + bn.b*0.25 + bn.a*0.125, 1.0, 0.0, 1.0) * u_densityScale;
}
float boxEdgeFade(vec3 p) {
    vec3 t = (p - u_boxMin) / (u_boxMax - u_boxMin); vec3 d = min(t, 1.0 - t);
    return smoothstep(0.0, 0.15, min(d.x, d.z)) * smoothstep(0.0, 0.10, d.y);
}
float PhaseHG(float cosTheta, float g) {
    float g2 = g * g; return (1.0 - g2) / pow(max(1.0 + g2 - 2.0*g*cosTheta, 1e-4), 1.5) * 0.079577;
}
float lightMarch(vec3 p) {
    vec3 sd = normalize(vec3(cos(u_time * 0.05), 0.3, sin(u_time * 0.05)));
    float d = 0.0, step = 8.0;
    for (int i=0; i<6; i++) { p += sd * step; d += cloudDensityFast(p) * step; step *= 1.6; }
    return d;
}
bool rayAABB(vec3 ro, vec3 rd, vec3 bMin, vec3 bMax, out float tN, out float tF) {
    vec3 inv = 1.0 / rd; vec3 t0 = (bMin-ro)*inv, t1 = (bMax-ro)*inv;
    vec3 tMn = min(t0,t1), tMx = max(t0,t1);
    tN = max(max(tMn.x,tMn.y),tMn.z); tF = min(min(tMx.x,tMx.y),tMx.z);
    return tF > max(tN, 0.0);
}
vec4 raymarchCloud(vec3 rayOrigin, vec3 rayDir) {
    float tN, tF; if (!rayAABB(rayOrigin, rayDir, u_boxMin, u_boxMax, tN, tF)) return vec4(0.0);
    tN = max(tN, 0.001); float rayLen = min(tF - tN, 3000.0);
    vec3 sd = normalize(vec3(cos(u_time * 0.05), 0.3, sin(u_time * 0.05)));
    float phase = PhaseHG(dot(rayDir, sd), u_scatterG)*0.7 + PhaseHG(dot(rayDir, sd), -0.15)*0.3;
    float transmittance = 1.0; vec3 color = vec3(0.0); float dt = u_stepSize, t = stepJitter() * dt;
    int maxS = min(u_enableTAA != 0 ? u_maxSteps/2 : u_maxSteps, 256);
    float prevCoarseDt = 0.0;
    for (int i=0; i<maxS; i++) {
        if (t >= rayLen) break;
        vec3 pos = rayOrigin + rayDir * (tN + t); float density = cloudDensity(pos) * boxEdgeFade(pos);
        if (density > 0.001) {
            if (prevCoarseDt > 0.0) { t = max(t - prevCoarseDt*0.6, 0.0); prevCoarseDt = 0.0; continue; }
            float ext = density * u_absorption, stepT = exp(-ext * dt), lDen = lightMarch(pos);
            float lightReach = exp(-u_absorption*lDen) + exp(-u_absorption*lDen*0.25)*0.45 + exp(-u_absorption*lDen*0.06)*0.25;
            float powder = 1.0 - exp(-density * 4.0);
            float hFrac = (pos.y - u_boxMin.y) / max(u_boxMax.y - u_boxMin.y, 1.0);
            vec3 ambient = mix(u_cloudColorBottom*u_ambientStrength*0.3, u_cloudColorTop*u_ambientStrength*0.55 + vec3(0.25,0.45,0.75)*u_ambientStrength*0.45, hFrac);
            vec3 sunLight = mix(u_cloudColorBottom, u_cloudColorTop, lightReach) * mix(vec3(1.0), u_sunColor, 0.3) * u_sunIntensity * (lightReach + powder*0.12) * phase;
            vec3 silver = u_sunColor * u_silverLining * pow(max(0.0, dot(rayDir, sd)), 8.0) * (1.0 - transmittance) * lightReach * 0.3;
            color += (sunLight + silver + ambient) * density * transmittance * dt * u_absorption;
            transmittance *= stepT; if (transmittance < 0.005) break; t += dt;
        } else { prevCoarseDt = (dt * 2.0); t += prevCoarseDt; }
    }
    color = clamp((color*(2.51*color+0.03))/(color*(2.43*color+0.59)+0.14), 0.0, 1.0);
    return vec4(pow(max(color, vec3(0.0)), vec3(1.0/2.2)), clamp(1.0 - transmittance, 0.0, 1.0));
}
vec2 reprojectUV(vec3 worldPos) {
    vec4 clip = u_prevProj * (u_prevView * vec4(worldPos, 1.0));
    if (clip.w < 0.001) return vec2(-1.0);
    vec2 ndc = clip.xy / clip.w;
    if (any(greaterThan(abs(ndc), vec2(1.0)))) return vec2(-1.0);
    return ndc * 0.5 + 0.5;
}
void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy); ivec2 imgSize = imageSize(u_outputImage);
    if (pixel.x >= imgSize.x || pixel.y >= imgSize.y) return;
    if (gl_LocalInvocationIndex == 0) { gs_invProj = inverse(u_proj); gs_invView = inverse(u_view); gs_nodeScale = length(vec3(u_worldMatrix[0])); gs_tileHits = 0; }
    barrier();
    vec2 uv = (vec2(pixel)+0.5)/vec2(imgSize); vec2 ndc = uv*2.0-1.0;
    vec4 vD = gs_invProj * vec4(ndc, -1.0, 1.0); vec3 rayDir = normalize((gs_invView * vec4(vD.xyz/vD.w, 0.0)).xyz);
    float tN, tF; if (rayAABB(u_cameraPos, rayDir, u_boxMin, u_boxMax, tN, tF)) atomicOr(gs_tileHits, 1);
    memoryBarrierShared(); barrier();
    if (gs_tileHits == 0) { imageStore(u_outputImage, pixel, vec4(0.0)); return; }
    int slot = u_frameIndex % 4;
    bool isActive = (u_enableTAA == 0) || (u_frameIndex == 0) || (pixel.x % 2 == slot % 2 && pixel.y % 2 == slot / 2);
    vec4 result = isActive ? raymarchCloud(u_cameraPos, rayDir) : vec4(0.0);
    vec4 history = vec4(0.0); bool hasHist = false;
    if (u_enableTAA != 0 && u_frameIndex > 0) {
        if (rayAABB(u_cameraPos, rayDir, u_boxMin, u_boxMax, tN, tF)) {
            vec2 pUV = reprojectUV(u_cameraPos + rayDir * (tN + min(tF-tN, 3000.0)*0.5));
            if (pUV.x >= 0.0) { history = texture(u_historyTex, pUV); hasHist = (history.a > 0.001 || result.a > 0.001); }
        }
    }
    if (!isActive) result = hasHist ? history : vec4(0.0);
    else if (hasHist) result = mix(history, result, mix(u_taaBlend, min(u_taaBlend*5.0, 0.85), clamp(length(u_cameraPos-u_prevCameraPos)*0.08, 0.0, 1.0)));
    imageStore(u_outputImage, pixel, result);
}
)GLSL";

static const char* kQuadVertSrc = R"GLSL(
#version 460 core
out vec2 vTexCoord;
void main() {
    const vec2 verts[3] = vec2[](vec2(-1.0, -1.0), vec2( 3.0, -1.0), vec2(-1.0,  3.0));
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
    vTexCoord = verts[gl_VertexID] * 0.5 + 0.5;
}
)GLSL";

static const char* kQuadFragSrc = R"GLSL(
#version 460 core
in vec2 vTexCoord; out vec4 FragColor;
uniform sampler2D u_cloudTex;
void main() { vec4 c = texture(u_cloudTex, vTexCoord); if (c.a < 0.004) discard; FragColor = c; }
)GLSL";

static const char* kWeatherGenSrc = R"GLSL(
#version 460 core
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba8, binding = 0) writeonly uniform image2D uWeatherMap;
uniform int uWidth, uHeight; uniform uint uSeed; uniform int uPatternType;
uniform float uCenterX, uCenterY, uTightness, uFalloffRadius, uBandAngle, uBandWidth, uBandSpacing, uBandTurbulence, uNoiseFreq, uCoverageScale, uCoverageMin, uCoverageMax;
uniform int uArms;
float hash(int x, int y, int z, int period, int salt) {
    x = ((x % period) + period) % period; y = ((y % period) + period) % period; z = ((z % period) + period) % period;
    uint n = uint(x*1619 ^ y*31337 ^ z*6271 ^ salt*1013); n ^= n >> 13; n *= 0xb5297a4du; n ^= n >> 7; n *= 0x68e31da4u; n ^= n >> 11;
    return float(n & 0x00ffffffu) / 16777216.0;
}
float valueNoise(vec3 p, int period) {
    ivec3 p0 = ivec3(floor(p)); vec3 f = fract(p); f = f*f*(3.0 - 2.0*f);
    float h000 = hash(p0.x,p0.y,p0.z,period,0); float h100 = hash(p0.x+1,p0.y,p0.z,period,0);
    float h010 = hash(p0.x,p0.y+1,p0.z,period,0); float h110 = hash(p0.x+1,p0.y+1,p0.z,period,0);
    float h001 = hash(p0.x,p0.y,p0.z+1,period,0); float h101 = hash(p0.x+1,p0.y,p0.z+1,period,0);
    float h011 = hash(p0.x,p0.y+1,p0.z+1,period,0); float h111 = hash(p0.x+1,p0.y+1,p0.z+1,period,0);
    return mix(mix(mix(h000,h100,f.x),mix(h010,h110,f.x),f.y),mix(mix(h001,h101,f.x),mix(h011,h111,f.x),f.y),f.z);
}
float fbm(vec3 p, int baseFreq, int oct) {
    float v = 0.0, a = 0.5; for (int i=0; i<oct; i++) { int f = baseFreq << i; v += a * valueNoise(p * float(f), f); a *= 0.5; }
    return v;
}
float worley(vec3 p, int freq) {
    vec3 fp = p * float(freq); ivec3 cp = ivec3(floor(fp)); float md = 9.0;
    for (int dx=-1;dx<=1;dx++) for (int dy=-1;dy<=1;dy++) for (int dz=-1;dz<=1;dz++) {
        ivec3 np = cp + ivec3(dx,dy,dz);
        vec3 cell = vec3(np) + vec3(hash(np.x,np.y,np.z,freq,0),hash(np.x,np.y,np.z,freq,1),hash(np.x,np.y,np.z,freq,2));
        float d = dot(fp-cell,fp-cell); if (d<md) md=d;
    }
    return 1.0 - sqrt(md)/1.73205;
}
void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy); if (id.x >= uWidth || id.y >= uHeight) return;
    float ox = float((uSeed * 1619u) & 0xffffu) / 65535.0, oz = float((uSeed * 31337u) & 0xffffu) / 65535.0;
    float u = float(id.x)/float(uWidth) + ox, v = float(id.y)/float(uHeight) + oz;
    float cov = 0.0;
    if (uPatternType == 1) {
        vec2 d = vec2(u-ox-uCenterX, v-oz-uCenterY); float r = length(d), theta = atan(d.y, d.x); if (theta < 0.0) theta += 6.28318;
        float armP = 6.28318 / float(clamp(uArms,1,5)), sP = mod(theta - r * uTightness + 314.159, armP);
        cov = (1.0 - smoothstep(0.0, armP*0.28, min(sP, armP-sP))) * exp(-r*r/max(uFalloffRadius*uFalloffRadius, 0.001)) * (0.4 + 0.6 * fbm(vec3(u*7.0,0.5,v*7.0),2,2)) * uCoverageScale * 1.6;
    } else if (uPatternType == 2) {
        vec2 d = vec2(u-ox-uCenterX, v-oz-uCenterY); float r = length(d), theta = atan(d.y, d.x); if (theta < 0.0) theta += 6.28318;
        float armP = 6.28318 / float(clamp(uArms,1,5)), sP = mod(theta - r * uTightness * 8.0 + 314.159, armP);
        float eyeR = uFalloffRadius * 0.09, eyeW = exp(-pow(abs(r-eyeR)/max(eyeR*0.6,0.001), 2.0)), eyeC = smoothstep(0.0, eyeR*0.6, r);
        cov = max(eyeW*eyeC, (1.0 - smoothstep(0.0, armP*0.22, min(sP, armP-sP)))*exp(-r*r/max(uFalloffRadius*uFalloffRadius, 0.001))*eyeC) * (0.55 + 0.45*fbm(vec3(u*6.0,0.4,v*6.0),2,2)) * uCoverageScale * 1.3;
    } else if (uPatternType == 3) {
        float proj = (u-0.5)*cos(uBandAngle) + (v-0.5)*sin(uBandAngle) + (fbm(vec3(u*5.0,0.5,v*5.0),2,3)-0.5)*uBandTurbulence;
        cov = smoothstep(0.5-clamp(uBandWidth*0.5,0.01,0.49), 0.5, 1.0-abs(mod(proj/max(uBandSpacing,0.01)+100.0, 1.0)-0.5)*2.0) * uCoverageScale;
    } else if (uPatternType == 4) {
        float freq = clamp(uNoiseFreq, 0.3, 8.0), w = worley(vec3(u*freq, 0.5, v*freq), int(freq*3.5+0.5)), th = max(0.0, 1.0-uCoverageScale*0.9);
        cov = (w < th ? 0.0 : (w-th)/max(1.0-th,0.001)); cov = cov*cov*1.4 * (0.5+0.5*fbm(vec3(u*freq*1.3,0.5,v*freq*1.3),2,2));
    } else {
        float p = worley(vec3(u*uNoiseFreq,0.45,v*uNoiseFreq),3) + (fbm(vec3(u*uNoiseFreq,0.45,v*uNoiseFreq),int(uNoiseFreq),3)*1.5)*(1.0-worley(vec3(u*uNoiseFreq,0.45,v*uNoiseFreq),3));
        float th = max(0.0, 1.0-uCoverageScale*1.2); cov = p < th ? 0.0 : (p-th)/max(1.0-th, 0.001); cov = clamp(cov*cov*(3.0-2.0*cov), 0.0, 1.0);
    }
    cov = clamp(mix(uCoverageMin, uCoverageMax, cov), 0.0, 1.0);
    imageStore(uWeatherMap, id, vec4(cov, clamp((cov-0.72)*3.5, 0.0, 1.0), pow(worley(vec3(u*1.17,0.5,v*0.93),5), 2.0), clamp(fbm(vec3(u*0.61,0.5,v*0.73),2,3)*1.9-0.35, 0.0, 1.0)));
}
)GLSL";

// ============================================================================
// State
// ============================================================================

struct CloudState {
    GLuint outputTex=0, historyTex=0, noiseBase=0, noiseDetail=0, weatherMapTex=0, sphereSSBO=0, emptyVAO=0, computeProg=0, quadProg=0, weatherProg=0;
    float* sphereData=nullptr; int sphereCount=0; bool spheresDirty=false;
    int outputW=0, outputH=0; float renderScale=0.5f;
    mat4 worldMatrix;
    vec3 cloudColorTop=vec3(0.8f,0.8f,0.9f), cloudColorBottom=vec3(0.2f,0.25f,0.35f);
    float absorption=0.01f, coverage=0.5f, erosion=0.5f, silverLining=2.0f, cloudType=0.5f, noiseScale=0.002f, detailScale=0.01f;
    int weatherRes=512, weatherPattern=0; float weatherFreq=2.0f, weatherCenterX=0.5f, weatherCenterY=0.5f, weatherFalloff=0.5f; uint weatherSeed=12345;
    vec3 sunColor=vec3(1.0f,0.9f,0.8f); float sunIntensity=5.0f, ambientStrength=0.5f, scatterG=0.2f;
    float densityScale=25.0f; int maxSteps=192; float stepSize=1.5f, turbulence=1.0f, windSpeed=5.0f;
    vec3 boxSize=vec3(15000.0f, 400.0f, 15000.0f); int gridX=3, gridZ=3; float gridSpacing=2000.0f, gridScale=1500.0f;
    bool enableTAA=true; float taaBlend=0.15f; int frameIndex=0;
    vec3 prevCamPos; mat4 prevView, prevProj;
    struct Locs {
        GLint cameraPos, view, proj, worldMatrix, time, sphereCount, densityScale, maxSteps, stepSize, turbulence, windSpeed, boxMin, boxMax, sunColor, sunIntensity, ambientStrength, scatterG;
        GLint cloudColorTop, cloudColorBottom, absorption, coverage, erosion, silverLining, cloudType, noiseScale, detailScale, noiseBase, noiseDetail, weatherMap, weatherMapAnchor, weatherMapGridExtent, useWeatherMap;
        GLint enableTAA, frameIndex, taaBlend, prevCameraPos, prevView, prevProj, historyTex, quadCloudTex;
        struct { GLint width, height, seed, pattern, centerX, centerY, arms, tightness, falloff, bandAngle, bandWidth, bandSpacing, bandTurbulence, noiseFreq, coverageScale, coverageMin, coverageMax; } w;
    } loc;
};
CloudState gCloud;

// ============================================================================
// Helpers
// ============================================================================

void checkShader(GLuint sh, const char* tag) {
    GLint ok=0; glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(sh,2048,NULL,log); fprintf(stderr,"[%s error] %s\n",tag,log); }
}
void checkProgram(GLuint prog, const char* tag) {
    GLint ok=0; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(prog,2048,NULL,log); fprintf(stderr,"[%s error] %s\n",tag,log); }
}
GLuint buildCompute(const char* src) {
    GLuint sh=glCreateShader(GL_COMPUTE_SHADER); glShaderSource(sh,1,&src,NULL); glCompileShader(sh); checkShader(sh,"comp");
    GLuint p=glCreateProgram(); glAttachShader(p,sh); glLinkProgram(p); checkProgram(p,"comp"); glDeleteShader(sh); return p;
}
GLuint buildRaster(const char* v, const char* f) {
    GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&v,NULL); glCompileShader(vs);
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&f,NULL); glCompileShader(fs);
    GLuint p=glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p); glDeleteShader(vs); glDeleteShader(fs); return p;
}
GLuint genBaseNoise() {
    const int N=64; unsigned char* d=(unsigned char*)malloc(N*N*N*4);
    for(int z=0;z<N;z++)for(int y=0;y<N;y++)for(int x=0;x<N;x++){
        float fx=(x+0.5f)/N, fy=(y+0.5f)/N, fz=(z+0.5f)/N; int i=(z*N*N+y*N+x)*4;
        d[i+0]=(unsigned char)(vcn_c01(vcn_perlinWorley(fx,fy,fz,4))*255);
        d[i+1]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,4))*255);
        d[i+2]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,8))*255);
        d[i+3]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,16))*255);
    }
    GLuint id; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_3D,id);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,N,N,N,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
    glGenerateMipmap(GL_TEXTURE_3D); free(d); return id;
}
GLuint genDetailNoise() {
    const int N=32; unsigned char* d=(unsigned char*)malloc(N*N*N*3);
    for(int z=0;z<N;z++)for(int y=0;y<N;y++)for(int x=0;x<N;x++){
        float fx=(x+0.5f)/N, fy=(y+0.5f)/N, fz=(z+0.5f)/N; int i=(z*N*N+y*N+x)*3;
        d[i+0]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,4))*255);
        d[i+1]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,8))*255);
        d[i+2]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,16))*255);
    }
    GLuint id; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_3D,id);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGB8,N,N,N,0,GL_RGB,GL_UNSIGNED_BYTE,d);
    glGenerateMipmap(GL_TEXTURE_3D); free(d); return id;
}
void generateWeatherMapGPU(int w, int h) {
    glUseProgram(gCloud.weatherProg); const auto& lw = gCloud.loc.w;
    glUniform1i(lw.width,w); glUniform1i(lw.height,h); glUniform1ui(lw.seed,gCloud.weatherSeed);
    glUniform1i(lw.pattern,gCloud.weatherPattern); glUniform1f(lw.centerX,gCloud.weatherCenterX); glUniform1f(lw.centerY,gCloud.weatherCenterY);
    glUniform1i(lw.arms,3); glUniform1f(lw.tightness,5.0f); glUniform1f(lw.falloff,gCloud.weatherFalloff);
    glUniform1f(lw.bandAngle,0.0f); glUniform1f(lw.bandWidth,0.5f); glUniform1f(lw.bandSpacing,0.2f); glUniform1f(lw.bandTurbulence,0.1f);
    glUniform1f(lw.noiseFreq,gCloud.weatherFreq); glUniform1f(lw.coverageScale,1.0f); glUniform1f(lw.coverageMin,0.0f); glUniform1f(lw.coverageMax,1.0f);
    glBindImageTexture(0, gCloud.weatherMapTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute((w+7)/8, (h+7)/8, 1); glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); glUseProgram(0);
}
void toggleFullScreen() {
    Atom s = XInternAtom(gDisplay,"_NET_WM_STATE",False), f = XInternAtom(gDisplay,"_NET_WM_STATE_FULLSCREEN",False);
    XEvent e; memset(&e,0,sizeof(e)); e.type=ClientMessage; e.xclient.window=gWindow; e.xclient.message_type=s; e.xclient.format=32;
    e.xclient.data.l[0]=gFullscreen?1:0; e.xclient.data.l[1]=(long)f;
    XSendEvent(gDisplay,XRootWindow(gDisplay,gVisInfo->screen),False,SubstructureNotifyMask,&e);
}

int initialize() {
    gCloud.worldMatrix = mat4::identity(); gCloud.prevCamPos = gCam.pos; gCloud.prevView = cameraView(); gCloud.prevProj = gProjMatrix;
    gCloud.computeProg = buildCompute(kComputeSrc); gCloud.quadProg = buildRaster(kQuadVertSrc, kQuadFragSrc); gCloud.weatherProg = buildCompute(kWeatherGenSrc);
    if(!gCloud.computeProg || !gCloud.quadProg || !gCloud.weatherProg) return -1;
    glGenVertexArrays(1, &gCloud.emptyVAO); gCloud.noiseBase = genBaseNoise(); gCloud.noiseDetail = genDetailNoise();
    glGenTextures(1, &gCloud.weatherMapTex); glBindTexture(GL_TEXTURE_2D, gCloud.weatherMapTex);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,gCloud.weatherRes,gCloud.weatherRes);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    auto& L = gCloud.loc; GLuint cp = gCloud.computeProg;
    L.cameraPos=glGetUniformLocation(cp,"u_cameraPos"); L.view=glGetUniformLocation(cp,"u_view"); L.proj=glGetUniformLocation(cp,"u_proj");
    L.worldMatrix=glGetUniformLocation(cp,"u_worldMatrix"); L.time=glGetUniformLocation(cp,"u_time");
    L.sphereCount=glGetUniformLocation(cp,"u_sphereCount"); L.densityScale=glGetUniformLocation(cp,"u_densityScale");
    L.maxSteps=glGetUniformLocation(cp,"u_maxSteps"); L.stepSize=glGetUniformLocation(cp,"u_stepSize");
    L.turbulence=glGetUniformLocation(cp,"u_turbulence"); L.windSpeed=glGetUniformLocation(cp,"u_windSpeed");
    L.boxMin=glGetUniformLocation(cp,"u_boxMin"); L.boxMax=glGetUniformLocation(cp,"u_boxMax");
    L.sunColor=glGetUniformLocation(cp,"u_sunColor"); L.sunIntensity=glGetUniformLocation(cp,"u_sunIntensity");
    L.ambientStrength=glGetUniformLocation(cp,"u_ambientStrength"); L.scatterG=glGetUniformLocation(cp,"u_scatterG");
    L.cloudColorTop=glGetUniformLocation(cp,"u_cloudColorTop"); L.cloudColorBottom=glGetUniformLocation(cp,"u_cloudColorBottom");
    L.absorption=glGetUniformLocation(cp,"u_absorption"); L.coverage=glGetUniformLocation(cp,"u_coverage");
    L.erosion=glGetUniformLocation(cp,"u_erosion"); L.silverLining=glGetUniformLocation(cp,"u_silverLining");
    L.cloudType=glGetUniformLocation(cp,"u_cloudType"); L.noiseScale=glGetUniformLocation(cp,"u_noiseScale");
    L.detailScale=glGetUniformLocation(cp,"u_detailScale"); L.noiseBase=glGetUniformLocation(cp,"u_noiseBase");
    L.noiseDetail=glGetUniformLocation(cp,"u_noiseDetail"); L.weatherMap=glGetUniformLocation(cp,"u_weatherMap");
    L.weatherMapAnchor=glGetUniformLocation(cp,"u_weatherMapAnchor"); L.weatherMapGridExtent=glGetUniformLocation(cp,"u_weatherMapGridExtent");
    L.useWeatherMap=glGetUniformLocation(cp,"u_useWeatherMap"); L.enableTAA=glGetUniformLocation(cp,"u_enableTAA");
    L.frameIndex=glGetUniformLocation(cp,"u_frameIndex"); L.taaBlend=glGetUniformLocation(cp,"u_taaBlend");
    L.prevCameraPos=glGetUniformLocation(cp,"u_prevCameraPos"); L.prevView=glGetUniformLocation(cp,"u_prevView");
    L.prevProj=glGetUniformLocation(cp,"u_prevProj"); L.historyTex=glGetUniformLocation(cp,"u_historyTex");
    L.quadCloudTex=glGetUniformLocation(gCloud.quadProg,"u_cloudTex");
    GLuint wp = gCloud.weatherProg;
    L.w.width=glGetUniformLocation(wp,"uWidth"); L.w.height=glGetUniformLocation(wp,"uHeight"); L.w.seed=glGetUniformLocation(wp,"uSeed");
    L.w.pattern=glGetUniformLocation(wp,"uPatternType"); L.w.centerX=glGetUniformLocation(wp,"uCenterX"); L.w.centerY=glGetUniformLocation(wp,"uCenterY");
    L.w.arms=glGetUniformLocation(wp,"uArms"); L.w.tightness=glGetUniformLocation(wp,"uTightness"); L.w.falloff=glGetUniformLocation(wp,"uFalloffRadius");
    L.w.bandAngle=glGetUniformLocation(wp,"uBandAngle"); L.w.bandWidth=glGetUniformLocation(wp,"uBandWidth");
    L.w.bandSpacing=glGetUniformLocation(wp,"uBandSpacing"); L.w.bandTurbulence=glGetUniformLocation(wp,"uBandTurbulence");
    L.w.noiseFreq=glGetUniformLocation(wp,"uNoiseFreq"); L.w.coverageScale=glGetUniformLocation(wp,"uCoverageScale");
    L.w.coverageMin=glGetUniformLocation(wp,"uCoverageMin"); L.w.coverageMax=glGetUniformLocation(wp,"uCoverageMax");
    generateWeatherMapGPU(gCloud.weatherRes, gCloud.weatherRes);
    return 0;
}
void resize(int w, int h) {
    if(h<=0) h=1; glViewport(0,0,w,h); gProjMatrix = vmath::perspective(45.0f,(float)w/(float)h, 0.1f, 10000.0f);
    gCloud.outputW = (int)(w*gCloud.renderScale); gCloud.outputH = (int)(h*gCloud.renderScale);
    if(gCloud.outputTex) glDeleteTextures(1,&gCloud.outputTex); if(gCloud.historyTex) glDeleteTextures(1,&gCloud.historyTex);
    auto mk = [](int w, int h){ GLuint id; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id); glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA16F,w,h); return id; };
    gCloud.outputTex = mk(gCloud.outputW, gCloud.outputH); gCloud.historyTex = mk(gCloud.outputW, gCloud.outputH); gCloud.frameIndex=0;
}
void update(float dt) {
    const float ls=1.2f; if(gKeyLeft) gCam.yaw-=ls*dt; if(gKeyRight) gCam.yaw+=ls*dt; if(gKeyUp) gCam.pitch+=ls*dt; if(gKeyDown) gCam.pitch-=ls*dt;
    gCam.pitch=std::clamp(gCam.pitch,-1.4f,1.4f); float s=gCam.speed*dt; vec3 f=cameraForward(), r=cameraRight();
    if(gKey['w']) gCam.pos=gCam.pos+f*s; if(gKey['s']) gCam.pos=gCam.pos-f*s; if(gKey['a']) gCam.pos=gCam.pos-r*s; if(gKey['d']) gCam.pos=gCam.pos+r*s;
    if(gKey['q']) gCam.pos[1]-=s; if(gKey['e']) gCam.pos[1]+=s;
}
void display() {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); auto now=std::chrono::high_resolution_clock::now(); float t=std::chrono::duration<float>(now-gStartTime).count();
    mat4 view=cameraView(), proj=gProjMatrix; glUseProgram(gCloud.computeProg);
    glBindImageTexture(0,gCloud.outputTex,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D,gCloud.noiseBase);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D,gCloud.noiseDetail);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,gCloud.historyTex);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,gCloud.weatherMapTex);
    const auto& L=gCloud.loc; glUniform3fv(L.cameraPos,1,(float*)&gCam.pos); glUniform3fv(L.prevCameraPos,1,(float*)&gCloud.prevCamPos);
    glUniformMatrix4fv(L.view,1,GL_FALSE,view); glUniformMatrix4fv(L.proj,1,GL_FALSE,proj);
    glUniformMatrix4fv(L.prevView,1,GL_FALSE,gCloud.prevView); glUniformMatrix4fv(L.prevProj,1,GL_FALSE,gCloud.prevProj);
    glUniformMatrix4fv(L.worldMatrix,1,GL_FALSE,gCloud.worldMatrix); glUniform1f(L.time,t);
    glUniform1i(L.sphereCount,0); glUniform1f(L.densityScale,gCloud.densityScale); glUniform1i(L.maxSteps,gCloud.maxSteps); glUniform1f(L.stepSize,gCloud.stepSize);
    glUniform1f(L.turbulence,gCloud.turbulence); glUniform1f(L.windSpeed,gCloud.windSpeed);
    vec3 minB=vec3(-gCloud.boxSize[0]*0.5f,0, -gCloud.boxSize[2]*0.5f), maxB=vec3(gCloud.boxSize[0]*0.5f,gCloud.boxSize[1],gCloud.boxSize[2]*0.5f);
    glUniform3fv(L.boxMin,1,(float*)&minB); glUniform3fv(L.boxMax,1,(float*)&maxB);
    glUniform3fv(L.sunColor,1,(float*)&gCloud.sunColor); glUniform1f(L.sunIntensity,gCloud.sunIntensity); glUniform1f(L.ambientStrength,gCloud.ambientStrength); glUniform1f(L.scatterG,gCloud.scatterG);
    glUniform3fv(L.cloudColorTop,1,(float*)&gCloud.cloudColorTop); glUniform3fv(L.cloudColorBottom,1,(float*)&gCloud.cloudColorBottom);
    glUniform1f(L.absorption,gCloud.absorption); glUniform1f(L.coverage,gCloud.coverage); glUniform1f(L.erosion,gCloud.erosion); glUniform1f(L.silverLining,gCloud.silverLining);
    glUniform1f(L.cloudType,gCloud.cloudType); glUniform1f(L.noiseScale,gCloud.noiseScale); glUniform1f(L.detailScale,gCloud.detailScale);
    glUniform1i(L.noiseBase,0); glUniform1i(L.noiseDetail,1); glUniform1i(L.historyTex,2); glUniform1i(L.weatherMap,3); glUniform1i(L.useWeatherMap,1);
    glUniform2f(L.weatherMapAnchor,0,0); glUniform1f(L.weatherMapGridExtent,gCloud.boxSize[0]);
    glUniform1i(L.enableTAA,gCloud.enableTAA?1:0); glUniform1i(L.frameIndex,gCloud.frameIndex); glUniform1f(L.taaBlend,gCloud.taaBlend);
    glDispatchCompute(((GLuint)gCloud.outputW+7)/8,((GLuint)gCloud.outputH+7)/8,1); glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
    glUseProgram(gCloud.quadProg); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,gCloud.outputTex); glUniform1i(L.quadCloudTex,0);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    glBindVertexArray(gCloud.emptyVAO); glDrawArrays(GL_TRIANGLES,0,3);
    glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    if(gCloud.enableTAA){ GLuint tmp=gCloud.outputTex; gCloud.outputTex=gCloud.historyTex; gCloud.historyTex=tmp; }
    gCloud.prevCamPos=gCam.pos; gCloud.prevView=view; gCloud.prevProj=proj; gCloud.frameIndex++;
    glXSwapBuffers(gDisplay,gWindow);
}
void uninitialize() {
    if(gCloud.outputTex)glDeleteTextures(1,&gCloud.outputTex); if(gCloud.historyTex)glDeleteTextures(1,&gCloud.historyTex);
    if(gCloud.noiseBase)glDeleteTextures(1,&gCloud.noiseBase); if(gCloud.noiseDetail)glDeleteTextures(1,&gCloud.noiseDetail);
    if(gCloud.weatherMapTex)glDeleteTextures(1,&gCloud.weatherMapTex);
    if(gCloud.computeProg)glDeleteProgram(gCloud.computeProg); if(gCloud.quadProg)glDeleteProgram(gCloud.quadProg); if(gCloud.weatherProg)glDeleteProgram(gCloud.weatherProg);
    if(gWindow)XDestroyWindow(gDisplay,gWindow); if(gColormap)XFreeColormap(gDisplay,gColormap); if(gGLXCtx){glXMakeCurrent(gDisplay,0,0);glXDestroyContext(gDisplay,gGLXCtx);}
    if(gVisInfo)XFree(gVisInfo); if(gDisplay)XCloseDisplay(gDisplay);
}

int main() {
    gpFile=fopen("vcloud_log.txt","w"); if(!gpFile)gpFile=stderr; Display* dpy=XOpenDisplay(NULL); if(!dpy)return 1; gDisplay=dpy;
    int defS=XDefaultScreen(dpy); int fbA[]={GLX_X_RENDERABLE,True,GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,GLX_RENDER_TYPE,GLX_RGBA_BIT,GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,GLX_DOUBLEBUFFER,True,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_ALPHA_SIZE,8,GLX_DEPTH_SIZE,24,None};
    int nFB=0; GLXFBConfig* fbs=glXChooseFBConfig(dpy,defS,fbA,&nFB); gFBCfg=fbs[0]; gVisInfo=glXGetVisualFromFBConfig(dpy,gFBCfg); XFree(fbs);
    XSetWindowAttributes wa; memset(&wa,0,sizeof(wa)); wa.event_mask=KeyPressMask|KeyReleaseMask|FocusChangeMask|StructureNotifyMask|ExposureMask;
    Window root=XRootWindow(dpy,gVisInfo->screen); wa.colormap=XCreateColormap(dpy,root,gVisInfo->visual,AllocNone); gColormap=wa.colormap;
    gWindow=XCreateWindow(dpy,root,0,0,WIN_W,WIN_H,0,gVisInfo->depth,InputOutput,gVisInfo->visual,CWEventMask|CWColormap,&wa);
    XMapWindow(dpy,gWindow); glXCreateContextAttribsARB=(glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
    int ca[]={GLX_CONTEXT_MAJOR_VERSION_ARB,4,GLX_CONTEXT_MINOR_VERSION_ARB,6,GLX_CONTEXT_PROFILE_MASK_ARB,GLX_CONTEXT_CORE_PROFILE_BIT_ARB,0};
    gGLXCtx=glXCreateContextAttribsARB(dpy,gFBCfg,0,True,ca); glXMakeCurrent(dpy,gWindow,gGLXCtx); glewInit();
    if(initialize()!=0)return 1; auto prevT=std::chrono::high_resolution_clock::now(); bool done=false; XEvent ev;
    while(!done){
        while(XPending(dpy)){
            XNextEvent(dpy,&ev);
            if(ev.type==ConfigureNotify)resize(ev.xconfigure.width,ev.xconfigure.height);
            if(ev.type==KeyPress){ KeySym ks=XkbKeycodeToKeysym(dpy,ev.xkey.keycode,0,0); if(ks==XK_Escape)done=true; char buf[8]; XLookupString(&ev.xkey,buf,8,NULL,NULL); if(buf[0]=='f'||buf[0]=='F'){gFullscreen=!gFullscreen;toggleFullScreen();} if(buf[0]>='a'&&buf[0]<='z')gKey[(unsigned char)buf[0]]=true; }
            if(ev.type==KeyRelease){ char buf[8]; XLookupString(&ev.xkey,buf,8,NULL,NULL); if(buf[0]>='a'&&buf[0]<='z')gKey[(unsigned char)buf[0]]=false; }
            if(ev.type==ClientMessage)done=true;
        }
        auto now=std::chrono::high_resolution_clock::now(); float dt=std::chrono::duration<float>(now-prevT).count(); prevT=now; update(dt>0.1f?0.1f:dt); display();
    }
    uninitialize(); return 0;
}
