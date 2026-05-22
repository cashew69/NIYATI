// Jai Shree Ram!!!
// Volumetric Cloud — Standalone Export Version
//
// Single self-contained C++ file. All shaders are embedded as string literals.
// Incorporates the full Nubis density model, Perlin-Worley 3D noise baked on CPU,
// checkerboard TAA with reprojection, banding / slab / jitter fixes.
//
// Build:
//   g++ volumetricExportVersion.cpp -std=c++17 -O2 -lX11 -lGL -lGLEW -o vcloud
//   ./vcloud
//
// Controls:
//   W / S       — fly forward / back
//   A / D       — strafe left / right
//   Q / E       — fly up / down
//   Arrow keys  — look around (yaw / pitch)
//   T           — toggle TAA + checkerboard (4x perf win)
//   F           — toggle fullscreen
//   Esc         — quit

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <chrono>

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
    float speed = 40.0f;
};
Camera gCam;

// Raw key state (ASCII + arrow sentinels)
bool gKey[256]     = {};
bool gKeyLeft      = false;
bool gKeyRight     = false;
bool gKeyUp        = false;   // arrow up
bool gKeyDown      = false;   // arrow down

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
// CPU-side Perlin-Worley noise (Nubis / HZD style pre-baked 3D textures)
// Identical to the engine version — generates once at startup.
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
// Embedded shaders
// ============================================================================

// ---- Compute shader (Nubis volumetric cloud raymarch) -----------------------
// Incorporates all fixes: Perlin-Worley Nubis density, Nubis height profiles,
// smooth UVW jitter (no voxel artifacts), coarse-step boundary refinement,
// checkerboard 2x2 interleave, proper reprojection via prevView/prevProj.

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
uniform int   u_flatBottom;

uniform float u_cloudType;
uniform float u_noiseScale;
uniform float u_detailScale;
uniform sampler3D u_noiseBase;
uniform sampler3D u_noiseDetail;

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

// ---- Remap ------------------------------------------------------------------

float remap(float v, float oMin, float oMax, float nMin, float nMax) {
    return nMin + ((v - oMin) / (oMax - oMin)) * (nMax - nMin);
}

float remapC(float v, float oMin, float oMax, float nMin, float nMax) {
    return clamp(remap(v,oMin,oMax,nMin,nMax), min(nMin,nMax), max(nMin,nMax));
}

// ---- Per-pixel step jitter --------------------------------------------------

float stepJitter() {
    float ign = fract(52.9829189 * fract(
        0.06711056 * float(gl_GlobalInvocationID.x) +
        0.00583715 * float(gl_GlobalInvocationID.y)));
    return fract(ign + float(u_frameIndex) * 0.6180339887) * 0.5;
}

// ---- Smooth UVW jitter — breaks 3D texture slab visibility ------------------
// Sinusoids with mutually irrational periods → no grid pattern, no cell
// boundaries, no voxel / checkerboard cube artifacts (unlike floor-hash).

vec3 uvwJitter(vec3 p) {
    return vec3(
        sin(p.x * 0.073 + p.z * 0.051),
        sin(p.y * 0.079 + p.x * 0.063),
        sin(p.z * 0.067 + p.y * 0.057)
    ) * 0.018;
}

// ---- Nubis height-layer density profiles ------------------------------------
// Source: Schneider, "The Real-time Volumetric Cloudscapes of HZD", SIGGRAPH 2015

float cloudLayerDensity(float h, float cloudType) {
    h = clamp(h, 0.0, 1.0);
    float stratus       = max(0.0, remapC(h,0.00,0.10,0.0,1.0) * remapC(h,0.20,0.30,1.0,0.0));
    float stratocumulus = max(0.0, remapC(h,0.00,0.20,0.0,1.0) * remapC(h,0.40,0.70,1.0,0.0));
    float cumulus       = max(0.0, remapC(h,0.00,0.15,0.0,1.0) * remapC(h,0.70,0.95,1.0,0.0));
    float d1 = mix(stratus,       stratocumulus, clamp(cloudType * 2.0,       0.0,1.0));
    float d2 = mix(stratocumulus, cumulus,       clamp((cloudType-0.5)*2.0,   0.0,1.0));
    return mix(d1, d2, cloudType);
}

// ---- 2D XZ sphere-footprint coverage ----------------------------------------

float sphereCoverage(vec3 p) {
    if (u_sphereCount == 0) return 1.0;
    float sc  = gs_nodeScale;
    float cov = 0.0;
    for (int i = 0; i < u_sphereCount; i++) {
        vec3  c = (u_worldMatrix * vec4(spheres[i].xyz, 1.0)).xyz;
        float r = spheres[i].w * sc * 8.0;
        float d = length(p.xz - c.xz) / max(r, 0.001);
        cov = max(cov, 1.0 - smoothstep(0.0, 1.0, d));
    }
    return cov;
}

// ---- Full cloud density (Nubis) ---------------------------------------------

float cloudDensity(vec3 p) {
    float boxH = max(u_boxMax.y - u_boxMin.y, 1.0);
    float h    = (p.y - u_boxMin.y) / boxH;
    if (h < 0.0 || h > 1.0) return 0.0;

    float layerDensity = cloudLayerDensity(h, u_cloudType);
    if (layerDensity < 0.001) return 0.0;

    vec3  wind    = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float spCov   = sphereCoverage(p);
    float coverage = (u_sphereCount == 0)
        ? clamp(u_coverage, 0.0, 1.0)
        : clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    if (coverage < 0.001) return 0.0;

    vec4  bn   = texture(u_noiseBase, (p + wind) * u_noiseScale + uvwJitter(p));
    float base = remapC(bn.r, 1.0 - coverage, 1.0, 0.0, 1.0) * layerDensity;
    if (base < 0.001) return 0.0;

    float erosion = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    base = remapC(base, erosion, 1.0, 0.0, 1.0);
    if (base < 0.001) return 0.0;

    vec4  dn  = texture(u_noiseDetail, (p + wind * 1.6) * u_detailScale + uvwJitter(p) * 0.5);
    float det = dn.r * 0.625 + dn.g * 0.25 + dn.b * 0.125;
    det       = mix(1.0 - det, det, clamp(h * 6.0, 0.0, 1.0));
    base      = remapC(base, det * u_erosion, 1.0, 0.0, 1.0);

    return base * u_densityScale;
}

// ---- Fast variant (light march — skip detail texture) -----------------------

float cloudDensityFast(vec3 p) {
    float boxH = max(u_boxMax.y - u_boxMin.y, 1.0);
    float h    = (p.y - u_boxMin.y) / boxH;
    if (h < 0.0 || h > 1.0) return 0.0;

    float layerDensity = cloudLayerDensity(h, u_cloudType);
    if (layerDensity < 0.001) return 0.0;

    vec3  wind    = vec3(u_windSpeed, 0.0, u_windSpeed * 0.4) * u_time;
    float spCov   = sphereCoverage(p);
    float coverage = (u_sphereCount == 0)
        ? clamp(u_coverage, 0.0, 1.0)
        : clamp(spCov * (1.0 + u_coverage * 0.6), 0.0, 1.0);
    if (coverage < 0.001) return 0.0;

    vec4  bn      = texture(u_noiseBase, (p + wind) * u_noiseScale + uvwJitter(p));
    float base    = remapC(bn.r, 1.0 - coverage, 1.0, 0.0, 1.0) * layerDensity;
    float erosion = bn.g * 0.625 + bn.b * 0.25 + bn.a * 0.125;
    return remapC(base, erosion, 1.0, 0.0, 1.0) * u_densityScale;
}

// ---- Box edge fade ----------------------------------------------------------

float boxEdgeFade(vec3 p) {
    vec3 t = (p - u_boxMin) / (u_boxMax - u_boxMin);
    vec3 d = min(t, 1.0 - t);
    return smoothstep(0.0, 0.15, min(d.x, d.z)) * smoothstep(0.0, 0.10, d.y);
}

// ---- Lighting helpers -------------------------------------------------------

vec3 sunDir() {
    return normalize(vec3(cos(u_time * 0.05), 0.3, sin(u_time * 0.05)));
}

float PhaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(max(1.0 + g2 - 2.0*g*cosTheta, 1e-4), 1.5) * 0.079577;
}

float lightMarch(vec3 p) {
    vec3  sd   = sunDir();
    float d    = 0.0;
    float step = 8.0;
    for (int i = 0; i < 6; i++) {
        p += sd * step;
        d += cloudDensityFast(p) * step;
        step *= 1.6;
    }
    return d;
}

float multiScatter(float absorption, float lightDensity) {
    return exp(-absorption * lightDensity)
         + exp(-absorption * lightDensity * 0.25) * 0.45
         + exp(-absorption * lightDensity * 0.06) * 0.25;
}

float powderEffect(float density) {
    return 1.0 - exp(-density * 4.0);
}

// ---- Ray-AABB ---------------------------------------------------------------

bool rayAABB(vec3 ro, vec3 rd, vec3 bMin, vec3 bMax, out float tNear, out float tFar) {
    vec3 inv = 1.0 / rd;
    vec3 t0  = (bMin - ro) * inv;
    vec3 t1  = (bMax - ro) * inv;
    vec3 tMn = min(t0,t1), tMx = max(t0,t1);
    tNear = max(max(tMn.x,tMn.y),tMn.z);
    tFar  = min(min(tMx.x,tMx.y),tMx.z);
    return tFar > max(tNear, 0.0);
}

// ---- Full ray march ---------------------------------------------------------

vec4 raymarchCloud(vec3 rayOrigin, vec3 rayDir) {
    float tNear, tFar;
    if (!rayAABB(rayOrigin, rayDir, u_boxMin, u_boxMax, tNear, tFar))
        return vec4(0.0);
    tNear = max(tNear, 0.001);
    float rayLen = min(tFar - tNear, 3000.0);

    vec3  sd       = sunDir();
    float cosTheta = dot(rayDir, sd);
    float phase    = PhaseHG(cosTheta,  u_scatterG) * 0.7
                   + PhaseHG(cosTheta, -0.15)        * 0.3;

    float transmittance = 1.0;
    vec3  color         = vec3(0.0);
    float dt            = u_stepSize;
    float t             = stepJitter() * dt;

    int maxS = min(u_enableTAA != 0 ? u_maxSteps / 2 : u_maxSteps, 256);

    int   emptyRun   = 0;
    float prevCoarseDt = 0.0;

    for (int i = 0; i < maxS; i++) {
        if (t >= rayLen) break;

        vec3  pos     = rayOrigin + rayDir * (tNear + t);
        float density = cloudDensity(pos) * boxEdgeFade(pos);

        if (density > 0.001) {
            if (prevCoarseDt > 0.0) {
                t = max(t - prevCoarseDt * 0.6, 0.0);
                prevCoarseDt = 0.0;
                emptyRun     = 0;
                continue;
            }
            emptyRun     = 0;
            prevCoarseDt = 0.0;

            float ext   = density * u_absorption;
            float stepT = exp(-ext * dt);
            float lDen  = lightMarch(pos);

            float lightReach = multiScatter(u_absorption, lDen);
            float powder     = powderEffect(density);

            float hFrac = (pos.y - u_boxMin.y) / max(u_boxMax.y - u_boxMin.y, 1.0);
            vec3  ambTop = u_cloudColorTop    * u_ambientStrength * 0.55
                         + vec3(0.25,0.45,0.75) * u_ambientStrength * 0.45;
            vec3  ambBot = u_cloudColorBottom * u_ambientStrength * 0.3;
            vec3  ambient = mix(ambBot, ambTop, hFrac);

            vec3 cloudColor = mix(u_cloudColorBottom, u_cloudColorTop, lightReach);
            vec3 sunTint    = mix(vec3(1.0), u_sunColor, 0.3);
            vec3 sunLight   = cloudColor * sunTint * u_sunIntensity
                            * (lightReach + powder * 0.12) * phase;

            float silverG = pow(max(0.0, dot(rayDir, sd)), 8.0);
            vec3  silver  = u_sunColor * u_silverLining * silverG
                          * (1.0 - transmittance) * lightReach * 0.3;

            color         += (sunLight + silver + ambient) * density * transmittance * dt * u_absorption;
            transmittance *= stepT;

            if (transmittance < 0.005) break;
            t += dt;
        } else {
            emptyRun++;
            prevCoarseDt = (emptyRun >= 2) ? dt * 4.0 : dt * 2.0;
            t += prevCoarseDt;
        }
    }

    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    color = clamp((color*(a*color+b))/(color*(c*color+d)+e), 0.0, 1.0);
    color = pow(max(color, vec3(0.0)), vec3(1.0/2.2));
    return vec4(color, clamp(1.0 - transmittance, 0.0, 1.0));
}

// ---- Reprojection -----------------------------------------------------------

vec2 reprojectUV(vec3 worldPos) {
    vec4 clip = u_prevProj * (u_prevView * vec4(worldPos, 1.0));
    if (clip.w < 0.001) return vec2(-1.0);
    vec2 ndc = clip.xy / clip.w;
    if (any(greaterThan(abs(ndc), vec2(1.0)))) return vec2(-1.0);
    return ndc * 0.5 + 0.5;
}

// ---- Main -------------------------------------------------------------------

void main() {
    ivec2 pixel   = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(u_outputImage);
    bool  inBounds = (pixel.x < imgSize.x && pixel.y < imgSize.y);

    if (gl_LocalInvocationIndex == 0) {
        gs_invProj   = inverse(u_proj);
        gs_invView   = inverse(u_view);
        gs_nodeScale = length(vec3(u_worldMatrix[0]));
        gs_tileHits  = 0;
    }
    barrier();

    vec3 rayDir = vec3(0.0, 0.0, -1.0);
    if (inBounds) {
        vec2 uv  = (vec2(pixel) + 0.5) / vec2(imgSize);
        vec2 ndc = uv * 2.0 - 1.0;
        vec4 vD  = gs_invProj * vec4(ndc, -1.0, 1.0);
        vD.xyz  /= vD.w;
        rayDir   = normalize((gs_invView * vec4(vD.xyz, 0.0)).xyz);

        float tN, tF;
        if (rayAABB(u_cameraPos, rayDir, u_boxMin, u_boxMax, tN, tF))
            atomicOr(gs_tileHits, 1);
    }
    memoryBarrierShared();
    barrier();

    if (gs_tileHits == 0) {
        if (inBounds) imageStore(u_outputImage, pixel, vec4(0.0));
        return;
    }
    if (!inBounds) return;

    // Checkerboard 2x2: only 1/4 pixels raymarched per frame when TAA on.
    int  slot     = u_frameIndex % 4;
    bool isActive = (u_enableTAA == 0)
                  || (u_frameIndex == 0)
                  || (pixel.x % 2 == slot % 2 && pixel.y % 2 == slot / 2);

    vec4 result = vec4(0.0);
    if (isActive)
        result = raymarchCloud(u_cameraPos, rayDir);

    // Reprojected history via previous-frame camera matrices.
    vec4 history = vec4(0.0);
    bool hasHist = false;
    if (u_enableTAA != 0 && u_frameIndex > 0) {
        float tN, tF;
        if (rayAABB(u_cameraPos, rayDir, u_boxMin, u_boxMax, tN, tF)) {
            float tMid  = tN + min(tF - tN, 3000.0) * 0.5;
            vec2  prevUV = reprojectUV(u_cameraPos + rayDir * tMid);
            if (prevUV.x >= 0.0) {
                history  = texture(u_historyTex, prevUV);
                hasHist  = (history.a > 0.001 || result.a > 0.001);
            }
        }
    }

    if (!isActive) {
        result = hasHist ? history : vec4(0.0);
    } else if (hasHist) {
        float speed = length(u_cameraPos - u_prevCameraPos);
        float mBias = clamp(speed * 0.08, 0.0, 1.0);
        float blend = mix(u_taaBlend, min(u_taaBlend * 5.0, 0.85), mBias);
        result = mix(history, result, blend);
    }

    imageStore(u_outputImage, pixel, result);
}
)GLSL";

// ---- Quad vertex shader -----------------------------------------------------
static const char* kQuadVertSrc = R"GLSL(
#version 460 core
out vec2 vTexCoord;

void main() {
    const vec2 verts[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
    vTexCoord   = verts[gl_VertexID] * 0.5 + 0.5;
}
)GLSL";

// ---- Quad fragment shader ---------------------------------------------------
static const char* kQuadFragSrc = R"GLSL(
#version 460 core
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_cloudTex;

void main() {
    vec4 cloud = texture(u_cloudTex, vTexCoord);
    if (cloud.a < 0.004) discard;
    FragColor = cloud;
}
)GLSL";

// ============================================================================
// Cloud state
// ============================================================================

struct CloudState {
    // GPU resources
    GLuint outputTex  = 0;
    GLuint historyTex = 0;
    GLuint noiseBase  = 0;   // 64^3 RGBA8
    GLuint noiseDetail= 0;   // 32^3 RGB8
    GLuint sphereSSBO = 0;
    GLuint emptyVAO   = 0;
    GLuint computeProg= 0;
    GLuint quadProg   = 0;

    // Sphere data (CPU)
    float* sphereData  = nullptr;
    int    sphereCount = 0;
    bool   spheresDirty= false;

    // Output resolution
    int   outputW = 0, outputH = 0;
    float renderScale = 0.39f;

    // World
    mat4 worldMatrix;

    // Appearance
    vec3  cloudColorTop    = vec3(0.235294f, 0.235294f, 0.235294f);
    vec3  cloudColorBottom = vec3(0.0179979f, 0.0256759f, 0.0343137f);
    float absorption   = 0.01f;
    float coverage     = -0.5f;
    float erosion      = 0.54f;
    float silverLining = 3.0f;
    float cloudType    = 1.0f;
    float noiseScale   = 0.0026f;
    float detailScale  = 0.015f;

    // Lighting
    vec3  sunColor        = vec3(1.0f, 1.0f, 1.0f);
    float sunIntensity    = 3.2f;
    float ambientStrength = 0.42f;
    float scatterG        = 0.14f;

    // Raymarching
    float densityScale = 20.0f;
    int   maxSteps     = 256;
    float stepSize     = 1.13f;
    float turbulence   = 4.0f;
    float windSpeed    = 10.0f;

    // Volume
    vec3  boxSize          = vec3(10000.0f, 250.0f, 10000.0f);
    int   gridX            = 3,  gridZ = 3;
    float gridSpacing      = 200.0f;
    float gridScale        = 800.0f;
    int   spheresPerCloudMin = 3, spheresPerCloudMax = 8;

    // TAA
    bool  enableTAA = false;
    float taaBlend  = 1.0f;
    int   frameIndex= 0;

    // Previous-frame camera (for reprojection)
    vec3 prevCamPos;
    mat4 prevView;
    mat4 prevProj;

    // Cached uniform locations
    struct Locs {
        GLint cameraPos, view, proj, worldMatrix, time;
        GLint sphereCount, densityScale, maxSteps, stepSize, turbulence, windSpeed;
        GLint boxMin, boxMax;
        GLint sunColor, sunIntensity, ambientStrength, scatterG;
        GLint cloudColorTop, cloudColorBottom, absorption, coverage, erosion;
        GLint silverLining, flatBottom, cloudType, noiseScale, detailScale;
        GLint noiseBase, noiseDetail;
        GLint enableTAA, frameIndex, taaBlend;
        GLint prevCameraPos, prevView, prevProj, historyTex;
        GLint quadCloudTex;
    } loc;
};

CloudState gCloud;

// ============================================================================
// Function prototypes
// ============================================================================

int  initialize();
void resize(int w, int h);
void display();
void update(float dt);
void uninitialize();
void toggleFullScreen();
void checkShader(GLuint sh, const char* tag);
void checkProgram(GLuint prog, const char* tag);

// ============================================================================
// Noise texture generation (CPU → GPU, done once at startup)
// ============================================================================

static GLuint genBaseNoise() {
    const int N = 64;
    unsigned char* data = (unsigned char*)malloc((size_t)N*N*N*4);
    for (int z=0;z<N;z++) for (int y=0;y<N;y++) for (int x=0;x<N;x++) {
        float fx=(x+0.5f)/N, fy=(y+0.5f)/N, fz=(z+0.5f)/N;
        int idx=(z*N*N+y*N+x)*4;
        data[idx+0]=(unsigned char)(vcn_c01(vcn_perlinWorley(fx,fy,fz,4))*255+0.5f);
        data[idx+1]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz, 4))*255+0.5f);
        data[idx+2]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz, 8))*255+0.5f);
        data[idx+3]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,16))*255+0.5f);
    }
    GLuint id;
    glGenTextures(1,&id); glBindTexture(GL_TEXTURE_3D,id);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,N,N,N,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D,0); free(data);
    fprintf(gpFile,"vcloud: base noise tex %dx%dx%d id=%u\n",N,N,N,id);
    return id;
}

static GLuint genDetailNoise() {
    const int N = 32;
    unsigned char* data = (unsigned char*)malloc((size_t)N*N*N*3);
    for (int z=0;z<N;z++) for (int y=0;y<N;y++) for (int x=0;x<N;x++) {
        float fx=(x+0.5f)/N, fy=(y+0.5f)/N, fz=(z+0.5f)/N;
        int idx=(z*N*N+y*N+x)*3;
        data[idx+0]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz, 4))*255+0.5f);
        data[idx+1]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz, 8))*255+0.5f);
        data[idx+2]=(unsigned char)(vcn_c01(vcn_worley(fx,fy,fz,16))*255+0.5f);
    }
    GLuint id;
    glGenTextures(1,&id); glBindTexture(GL_TEXTURE_3D,id);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGB8,N,N,N,0,GL_RGB,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D,0); free(data);
    fprintf(gpFile,"vcloud: detail noise tex %dx%dx%d id=%u\n",N,N,N,id);
    return id;
}

// ============================================================================
// Texture helpers
// ============================================================================

static GLuint makeRGBA16F(int w, int h) {
    GLuint id;
    glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA16F,w,h);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    static const float zero[4]={};
    glClearTexImage(id,0,GL_RGBA,GL_FLOAT,zero);
    glBindTexture(GL_TEXTURE_2D,0);
    return id;
}

static void reallocOutputTex() {
    if (gCloud.outputTex)  glDeleteTextures(1,&gCloud.outputTex);
    if (gCloud.historyTex) glDeleteTextures(1,&gCloud.historyTex);
    gCloud.outputTex  = makeRGBA16F(gCloud.outputW, gCloud.outputH);
    gCloud.historyTex = makeRGBA16F(gCloud.outputW, gCloud.outputH);
    gCloud.frameIndex = 0;
    fprintf(gpFile,"vcloud: output textures %dx%d\n", gCloud.outputW, gCloud.outputH);
}

// ============================================================================
// Sphere grid generation (local space, transformed by worldMatrix in shader)
// ============================================================================

static void generateSpheres() {
    srand((unsigned int)time(NULL));
    if (!gCloud.sphereData)
        gCloud.sphereData = (float*)malloc(256*4*sizeof(float));
    gCloud.sphereCount = 0;

    float startX = -(gCloud.gridX-1)*gCloud.gridSpacing*0.5f;
    float startZ = -(gCloud.gridZ-1)*gCloud.gridSpacing*0.5f;
    int perMin = gCloud.spheresPerCloudMin, perMax = gCloud.spheresPerCloudMax;
    if (perMin > perMax) perMin = perMax;
    auto rnd=[](float lo,float hi){ return lo+((float)rand()/(float)RAND_MAX)*(hi-lo); };

    for (int gx=0; gx<gCloud.gridX && gCloud.sphereCount<255; gx++) {
        for (int gz=0; gz<gCloud.gridZ && gCloud.sphereCount<255; gz++) {
            float cx = startX + gx*gCloud.gridSpacing;
            float cy = 4.0f;
            float cz = startZ + gz*gCloud.gridSpacing;
            int cnt = perMin + (rand()%(perMax-perMin+1));
            if (gCloud.sphereCount+cnt>256) cnt=256-gCloud.sphereCount;
            if (cnt<=0) break;
            int numLow=(int)(cnt*0.6f);
            if (numLow<2&&cnt>=3) numLow=2;
            int numUp=cnt-numLow;
            for (int k=0;k<numLow&&gCloud.sphereCount<256;k++) {
                float* s=&gCloud.sphereData[gCloud.sphereCount*4];
                s[0]=cx+rnd(-7.5f, 7.5f)*gCloud.gridScale;
                s[1]=cy+rnd(-0.25f,0.25f)*gCloud.gridScale;
                s[2]=cz+rnd(-1.5f, 1.5f)*gCloud.gridScale;
                s[3]=(2.0f+rnd(0.0f,1.0f))*gCloud.gridScale;
                gCloud.sphereCount++;
            }
            for (int k=0;k<numUp&&gCloud.sphereCount<256;k++) {
                float* s=&gCloud.sphereData[gCloud.sphereCount*4];
                s[0]=cx+rnd(-6.5f,6.5f)*gCloud.gridScale;
                s[1]=cy+rnd(2.0f, 3.0f)*gCloud.gridScale;
                s[2]=cz+rnd(-1.0f,1.0f)*gCloud.gridScale;
                s[3]=(1.0f+rnd(0.0f,0.6f))*gCloud.gridScale;
                gCloud.sphereCount++;
            }
        }
    }
    gCloud.spheresDirty=true;
    fprintf(gpFile,"vcloud: generated %d spheres (%dx%d grid)\n",
            gCloud.sphereCount, gCloud.gridX, gCloud.gridZ);
}

static void uploadSpheres() {
    if (!gCloud.spheresDirty || gCloud.sphereCount==0) return;
    size_t bytes=(size_t)gCloud.sphereCount*4*sizeof(float);
    if (!gCloud.sphereSSBO) glGenBuffers(1,&gCloud.sphereSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,gCloud.sphereSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,bytes,gCloud.sphereData,GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    gCloud.spheresDirty=false;
}

// ============================================================================
// Shader compilation helpers
// ============================================================================

void checkShader(GLuint sh, const char* tag) {
    GLint ok=0; glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(sh,sizeof(log),NULL,log);
        fprintf(gpFile,"[%s compile error]\n%s\n",tag,log);
        fprintf(stderr,"[%s compile error]\n%s\n",tag,log);
    }
}

void checkProgram(GLuint prog, const char* tag) {
    GLint ok=0; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(prog,sizeof(log),NULL,log);
        fprintf(gpFile,"[%s link error]\n%s\n",tag,log);
        fprintf(stderr,"[%s link error]\n%s\n",tag,log);
    }
}

static GLuint buildCompute(const char* src) {
    GLuint sh=glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);
    checkShader(sh,"compute");
    GLuint prog=glCreateProgram();
    glAttachShader(prog,sh); glLinkProgram(prog); checkProgram(prog,"compute");
    glDetachShader(prog,sh); glDeleteShader(sh);
    return prog;
}

static GLuint buildRaster(const char* vSrc, const char* fSrc) {
    GLuint vs=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs,1,&vSrc,NULL); glCompileShader(vs); checkShader(vs,"quad-vert");
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&fSrc,NULL); glCompileShader(fs); checkShader(fs,"quad-frag");
    GLuint prog=glCreateProgram();
    glAttachShader(prog,vs); glAttachShader(prog,fs);
    glLinkProgram(prog); checkProgram(prog,"quad");
    glDetachShader(prog,vs); glDetachShader(prog,fs);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    gpFile = fopen("vcloud_log.txt","w");
    if (!gpFile) gpFile = stderr;
    setvbuf(gpFile, NULL, _IONBF, 0);
    fprintf(gpFile, "Jai Shree Ram! vcloud started.\n");

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr,"XOpenDisplay failed\n"); return 1; }
    gDisplay = dpy;

    int defScreen = XDefaultScreen(dpy);

    int fbAttribs[] = {
        GLX_X_RENDERABLE,True, GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,GLX_RGBA_BIT, GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
        GLX_DOUBLEBUFFER,True, GLX_RED_SIZE,8, GLX_GREEN_SIZE,8,
        GLX_BLUE_SIZE,8, GLX_ALPHA_SIZE,8, GLX_DEPTH_SIZE,24, None
    };
    int nFB=0;
    GLXFBConfig* fbs = glXChooseFBConfig(dpy,defScreen,fbAttribs,&nFB);
    if (!fbs||nFB==0) { fprintf(stderr,"glXChooseFBConfig failed\n"); return 1; }

    int bestIdx=0, bestSamples=-1;
    for (int i=0;i<nFB;i++) {
        XVisualInfo* vi=glXGetVisualFromFBConfig(dpy,fbs[i]);
        if (vi) {
            int s; glXGetFBConfigAttrib(dpy,fbs[i],GLX_SAMPLES,&s);
            if (s>bestSamples) { bestSamples=s; bestIdx=i; }
            XFree(vi);
        }
    }
    gFBCfg  = fbs[bestIdx];
    gVisInfo= glXGetVisualFromFBConfig(dpy,gFBCfg);
    XFree(fbs);

    XSetWindowAttributes wa; memset(&wa,0,sizeof(wa));
    wa.border_pixel=0; wa.background_pixel=0;
    wa.event_mask=KeyPressMask|KeyReleaseMask|FocusChangeMask|StructureNotifyMask|ExposureMask;
    Window root=XRootWindow(dpy,gVisInfo->screen);
    wa.colormap=XCreateColormap(dpy,root,gVisInfo->visual,AllocNone);
    gColormap=wa.colormap;

    gWindow=XCreateWindow(dpy,root,0,0,WIN_W,WIN_H,0,gVisInfo->depth,InputOutput,
        gVisInfo->visual, CWBorderPixel|CWBackPixel|CWEventMask|CWColormap, &wa);
    Atom wmDel=XInternAtom(dpy,"WM_DELETE_WINDOW",True);
    XSetWMProtocols(dpy,gWindow,&wmDel,1);
    XStoreName(dpy,gWindow,"Nikhil Sathe — Volumetric Cloud (Export)");
    XMapWindow(dpy,gWindow);
    {
        Screen* scr=XScreenOfDisplay(dpy,gVisInfo->screen);
        XMoveWindow(dpy,gWindow,(XWidthOfScreen(scr)-WIN_W)/2,(XHeightOfScreen(scr)-WIN_H)/2);
    }

    glXCreateContextAttribsARB=(glXCreateContextAttribsARBProc)
        glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
    if (glXCreateContextAttribsARB) {
        int ca[]={GLX_CONTEXT_MAJOR_VERSION_ARB,4,GLX_CONTEXT_MINOR_VERSION_ARB,6,
                  GLX_CONTEXT_PROFILE_MASK_ARB,GLX_CONTEXT_CORE_PROFILE_BIT_ARB,0};
        gGLXCtx=glXCreateContextAttribsARB(dpy,gFBCfg,0,True,ca);
    }
    if (!gGLXCtx) gGLXCtx=glXCreateNewContext(dpy,gFBCfg,GLX_RGBA_TYPE,0,True);
    if (!gGLXCtx) { fprintf(stderr,"GLX context creation failed\n"); return 1; }
    glXMakeCurrent(dpy,gWindow,gGLXCtx);

    if (glewInit()!=GLEW_OK) { fprintf(stderr,"glewInit failed\n"); return 1; }

    fprintf(gpFile,"GL %s  GLSL %s\n",glGetString(GL_VERSION),glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (initialize()!=0) { fprintf(stderr,"initialize() failed\n"); return 1; }

    auto prevTime = std::chrono::high_resolution_clock::now();
    Bool bDone=False;
    XEvent ev;

    while (!bDone) {
        while (XPending(dpy)) {
            XNextEvent(dpy,&ev);
            switch (ev.type) {
            case FocusIn:  gActive=true;  break;
            case FocusOut: gActive=false; break;
            case ConfigureNotify:
                resize(ev.xconfigure.width, ev.xconfigure.height);
                break;
            case KeyPress: {
                KeySym ks=XkbKeycodeToKeysym(dpy,ev.xkey.keycode,0,0);
                if (ks==XK_Escape) bDone=True;
                if (ks==XK_Left)  gKeyLeft =true;
                if (ks==XK_Right) gKeyRight=true;
                if (ks==XK_Up)    gKeyUp   =true;
                if (ks==XK_Down)  gKeyDown =true;
                char buf[8]={};
                XLookupString(&ev.xkey,buf,sizeof(buf),NULL,NULL);
                char c=buf[0];
                if (c>='a'&&c<='z') gKey[(unsigned char)c]=true;
                if (c>='A'&&c<='Z') gKey[(unsigned char)(c+32)]=true;
                if (c=='f'||c=='F') { gFullscreen=!gFullscreen; toggleFullScreen(); }
                if (c=='t'||c=='T') {
                    gCloud.enableTAA=!gCloud.enableTAA;
                    if (gCloud.enableTAA) {
                        static const float z[4]={};
                        glClearTexImage(gCloud.historyTex,0,GL_RGBA,GL_FLOAT,z);
                        gCloud.frameIndex=0;
                    }
                    fprintf(gpFile,"TAA %s\n", gCloud.enableTAA?"ON":"OFF");
                }
                break;
            }
            case KeyRelease: {
                KeySym ks=XkbKeycodeToKeysym(dpy,ev.xkey.keycode,0,0);
                if (ks==XK_Left)  gKeyLeft =false;
                if (ks==XK_Right) gKeyRight=false;
                if (ks==XK_Up)    gKeyUp   =false;
                if (ks==XK_Down)  gKeyDown =false;
                char buf[8]={};
                XLookupString(&ev.xkey,buf,sizeof(buf),NULL,NULL);
                char c=buf[0];
                if (c>='a'&&c<='z') gKey[(unsigned char)c]=false;
                if (c>='A'&&c<='Z') gKey[(unsigned char)(c+32)]=false;
                break;
            }
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0]==wmDel) bDone=True;
                break;
            }
        }

        auto now=std::chrono::high_resolution_clock::now();
        float dt=std::chrono::duration<float>(now-prevTime).count();
        prevTime=now;
        if (dt>0.1f) dt=0.1f;

        update(dt);
        display();
    }

    uninitialize();
    return 0;
}

// ============================================================================
// initialize
// ============================================================================

int initialize() {
    gCloud.worldMatrix = mat4::identity();
    gCloud.prevCamPos  = gCam.pos;
    gCloud.prevView    = cameraView();
    gCloud.prevProj    = gProjMatrix;

    fprintf(gpFile,"vcloud: compiling shaders...\n");
    gCloud.computeProg = buildCompute(kComputeSrc);
    gCloud.quadProg    = buildRaster(kQuadVertSrc, kQuadFragSrc);
    if (!gCloud.computeProg || !gCloud.quadProg) return -1;

    glGenVertexArrays(1, &gCloud.emptyVAO);

    fprintf(gpFile,"vcloud: baking noise textures (may take 1-2 s)...\n");
    gCloud.noiseBase   = genBaseNoise();
    gCloud.noiseDetail = genDetailNoise();
    if (!gCloud.noiseBase || !gCloud.noiseDetail) return -1;

    // Cache all uniform locations
    auto& L = gCloud.loc;
    GLuint cp = gCloud.computeProg;
    L.cameraPos       = glGetUniformLocation(cp,"u_cameraPos");
    L.view            = glGetUniformLocation(cp,"u_view");
    L.proj            = glGetUniformLocation(cp,"u_proj");
    L.worldMatrix     = glGetUniformLocation(cp,"u_worldMatrix");
    L.time            = glGetUniformLocation(cp,"u_time");
    L.sphereCount     = glGetUniformLocation(cp,"u_sphereCount");
    L.densityScale    = glGetUniformLocation(cp,"u_densityScale");
    L.maxSteps        = glGetUniformLocation(cp,"u_maxSteps");
    L.stepSize        = glGetUniformLocation(cp,"u_stepSize");
    L.turbulence      = glGetUniformLocation(cp,"u_turbulence");
    L.windSpeed       = glGetUniformLocation(cp,"u_windSpeed");
    L.boxMin          = glGetUniformLocation(cp,"u_boxMin");
    L.boxMax          = glGetUniformLocation(cp,"u_boxMax");
    L.sunColor        = glGetUniformLocation(cp,"u_sunColor");
    L.sunIntensity    = glGetUniformLocation(cp,"u_sunIntensity");
    L.ambientStrength = glGetUniformLocation(cp,"u_ambientStrength");
    L.scatterG        = glGetUniformLocation(cp,"u_scatterG");
    L.cloudColorTop   = glGetUniformLocation(cp,"u_cloudColorTop");
    L.cloudColorBottom= glGetUniformLocation(cp,"u_cloudColorBottom");
    L.absorption      = glGetUniformLocation(cp,"u_absorption");
    L.coverage        = glGetUniformLocation(cp,"u_coverage");
    L.erosion         = glGetUniformLocation(cp,"u_erosion");
    L.silverLining    = glGetUniformLocation(cp,"u_silverLining");
    L.flatBottom      = glGetUniformLocation(cp,"u_flatBottom");
    L.cloudType       = glGetUniformLocation(cp,"u_cloudType");
    L.noiseScale      = glGetUniformLocation(cp,"u_noiseScale");
    L.detailScale     = glGetUniformLocation(cp,"u_detailScale");
    L.noiseBase       = glGetUniformLocation(cp,"u_noiseBase");
    L.noiseDetail     = glGetUniformLocation(cp,"u_noiseDetail");
    L.enableTAA       = glGetUniformLocation(cp,"u_enableTAA");
    L.frameIndex      = glGetUniformLocation(cp,"u_frameIndex");
    L.taaBlend        = glGetUniformLocation(cp,"u_taaBlend");
    L.prevCameraPos   = glGetUniformLocation(cp,"u_prevCameraPos");
    L.prevView        = glGetUniformLocation(cp,"u_prevView");
    L.prevProj        = glGetUniformLocation(cp,"u_prevProj");
    L.historyTex      = glGetUniformLocation(cp,"u_historyTex");
    L.quadCloudTex    = glGetUniformLocation(gCloud.quadProg,"u_cloudTex");

    generateSpheres();
    uploadSpheres();

    glClearColor(0.05f,0.08f,0.12f,1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    resize(WIN_W, WIN_H);
    return 0;
}

// ============================================================================
// resize
// ============================================================================

void resize(int w, int h) {
    if (h<=0) h=1;
    glViewport(0,0,w,h);
    gProjMatrix = vmath::perspective(45.0f,(float)w/(float)h, 0.1f, 5000.0f);
    gCloud.outputW = (int)(w * gCloud.renderScale);
    gCloud.outputH = (int)(h * gCloud.renderScale);
    reallocOutputTex();
}

// ============================================================================
// update — camera movement
// ============================================================================

void update(float dt) {
    const float lookSpeed = 1.2f;
    if (gKeyLeft)  gCam.yaw   -= lookSpeed * dt;
    if (gKeyRight) gCam.yaw   += lookSpeed * dt;
    if (gKeyUp)    gCam.pitch += lookSpeed * dt;
    if (gKeyDown)  gCam.pitch -= lookSpeed * dt;
    gCam.pitch = gCam.pitch >  1.4f ?  1.4f : gCam.pitch;
    gCam.pitch = gCam.pitch < -1.4f ? -1.4f : gCam.pitch;

    vec3 fwd   = cameraForward();
    vec3 right = cameraRight();
    float s = gCam.speed * dt;

    if (gKey['w']) gCam.pos = gCam.pos + fwd   *  s;
    if (gKey['s']) gCam.pos = gCam.pos + fwd   * -s;
    if (gKey['a']) gCam.pos = gCam.pos + right * -s;
    if (gKey['d']) gCam.pos = gCam.pos + right *  s;
    if (gKey['q']) gCam.pos[1] -= s;
    if (gKey['e']) gCam.pos[1] += s;
}

// ============================================================================
// display — compute pass + quad composite
// ============================================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadSpheres();

    auto now = std::chrono::high_resolution_clock::now();
    float t  = std::chrono::duration<float>(now - gStartTime).count();

    mat4 view = cameraView();
    mat4 proj = gProjMatrix;

    // ---- Compute pass -------------------------------------------------------
    glUseProgram(gCloud.computeProg);

    // Image unit 0 = write target
    glBindImageTexture(0, gCloud.outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Texture unit 0 = base noise 3D
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, gCloud.noiseBase);
    // Texture unit 1 = detail noise 3D
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, gCloud.noiseDetail);
    // Texture unit 2 = history (sampler2D for bilinear reprojection)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gCloud.historyTex);
    glActiveTexture(GL_TEXTURE0);

    // SSBO binding 1 = sphere data
    if (gCloud.sphereSSBO)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gCloud.sphereSSBO);

    const auto& L = gCloud.loc;

    glUniform3fv(L.cameraPos,    1, (float*)&gCam.pos);
    glUniform3fv(L.prevCameraPos,1, (float*)&gCloud.prevCamPos);
    glUniformMatrix4fv(L.view,        1,GL_FALSE,view);
    glUniformMatrix4fv(L.proj,        1,GL_FALSE,proj);
    glUniformMatrix4fv(L.prevView,    1,GL_FALSE,gCloud.prevView);
    glUniformMatrix4fv(L.prevProj,    1,GL_FALSE,gCloud.prevProj);
    glUniformMatrix4fv(L.worldMatrix, 1,GL_FALSE,gCloud.worldMatrix);
    glUniform1f(L.time, t);

    glUniform1i(L.sphereCount,   gCloud.sphereCount);
    glUniform1f(L.densityScale,  gCloud.densityScale);
    glUniform1i(L.maxSteps,      gCloud.maxSteps);
    glUniform1f(L.stepSize,      gCloud.stepSize);
    glUniform1f(L.turbulence,    gCloud.turbulence);
    glUniform1f(L.windSpeed,     gCloud.windSpeed);

    vec3 halfBox = gCloud.boxSize * 0.5f;
    vec3 boxMin  = vec3(-halfBox[0], -halfBox[1], -halfBox[2]);
    vec3 boxMax  = vec3( halfBox[0],  halfBox[1],  halfBox[2]);
    glUniform3fv(L.boxMin, 1, (float*)&boxMin);
    glUniform3fv(L.boxMax, 1, (float*)&boxMax);

    glUniform3fv(L.sunColor,         1,(float*)&gCloud.sunColor);
    glUniform1f(L.sunIntensity,      gCloud.sunIntensity);
    glUniform1f(L.ambientStrength,   gCloud.ambientStrength);
    glUniform1f(L.scatterG,          gCloud.scatterG);
    glUniform3fv(L.cloudColorTop,    1,(float*)&gCloud.cloudColorTop);
    glUniform3fv(L.cloudColorBottom, 1,(float*)&gCloud.cloudColorBottom);
    glUniform1f(L.absorption,        gCloud.absorption);
    glUniform1f(L.coverage,          gCloud.coverage);
    glUniform1f(L.erosion,           gCloud.erosion);
    glUniform1f(L.silverLining,      gCloud.silverLining);
    glUniform1i(L.flatBottom,        0);
    glUniform1f(L.cloudType,         gCloud.cloudType);
    glUniform1f(L.noiseScale,        gCloud.noiseScale);
    glUniform1f(L.detailScale,       gCloud.detailScale);
    glUniform1i(L.noiseBase,   0);
    glUniform1i(L.noiseDetail, 1);
    glUniform1i(L.historyTex,  2);

    glUniform1i(L.enableTAA,  gCloud.enableTAA ? 1 : 0);
    glUniform1i(L.frameIndex, gCloud.frameIndex);
    glUniform1f(L.taaBlend,   gCloud.taaBlend);

    GLuint gx = ((GLuint)gCloud.outputW+7)/8;
    GLuint gy = ((GLuint)gCloud.outputH+7)/8;
    glDispatchCompute(gx, gy, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // ---- Quad composite pass ------------------------------------------------
    glUseProgram(gCloud.quadProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCloud.outputTex);
    glUniform1i(L.quadCloudTex, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(gCloud.emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // TAA ping-pong and prev-frame state update
    if (gCloud.enableTAA) {
        GLuint tmp        = gCloud.outputTex;
        gCloud.outputTex  = gCloud.historyTex;
        gCloud.historyTex = tmp;
    }
    gCloud.prevCamPos = gCam.pos;
    gCloud.prevView   = view;
    gCloud.prevProj   = proj;
    gCloud.frameIndex++;

    glXSwapBuffers(gDisplay, gWindow);
}

// ============================================================================
// toggleFullScreen
// ============================================================================

void toggleFullScreen() {
    Atom state = XInternAtom(gDisplay,"_NET_WM_STATE",False);
    Atom fs    = XInternAtom(gDisplay,"_NET_WM_STATE_FULLSCREEN",False);
    XEvent e; memset(&e,0,sizeof(e));
    e.type=ClientMessage; e.xclient.window=gWindow;
    e.xclient.message_type=state; e.xclient.format=32;
    e.xclient.data.l[0]=gFullscreen?1:0; e.xclient.data.l[1]=(long)fs;
    XSendEvent(gDisplay,XRootWindow(gDisplay,gVisInfo->screen),
               False,SubstructureNotifyMask,&e);
}

// ============================================================================
// uninitialize
// ============================================================================

void uninitialize() {
    if (gCloud.outputTex)   glDeleteTextures(1,&gCloud.outputTex);
    if (gCloud.historyTex)  glDeleteTextures(1,&gCloud.historyTex);
    if (gCloud.noiseBase)   glDeleteTextures(1,&gCloud.noiseBase);
    if (gCloud.noiseDetail) glDeleteTextures(1,&gCloud.noiseDetail);
    if (gCloud.sphereSSBO)  glDeleteBuffers(1,&gCloud.sphereSSBO);
    if (gCloud.emptyVAO)    glDeleteVertexArrays(1,&gCloud.emptyVAO);
    if (gCloud.computeProg) glDeleteProgram(gCloud.computeProg);
    if (gCloud.quadProg)    glDeleteProgram(gCloud.quadProg);
    if (gCloud.sphereData)  free(gCloud.sphereData);

    if (gWindow)   XDestroyWindow(gDisplay,gWindow);
    if (gColormap) XFreeColormap(gDisplay,gColormap);
    if (gGLXCtx) {
        glXMakeCurrent(gDisplay,0,0);
        glXDestroyContext(gDisplay,gGLXCtx);
    }
    if (gVisInfo)  { XFree(gVisInfo); gVisInfo=NULL; }
    if (gDisplay)  { XCloseDisplay(gDisplay); gDisplay=NULL; }
    if (gpFile && gpFile!=stderr) {
        fprintf(gpFile,"vcloud: clean exit.\n");
        fclose(gpFile);
    }
}
