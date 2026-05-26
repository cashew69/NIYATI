#include "terrain_overlay.h"
#include "engine/core/gl/structs.h"
#include <GL/glew.h>
#include <cmath>
#include <cstdio>
#include <vector>

extern int   g_terrainMeshWidth;
extern int   g_terrainMeshDepth;
extern float g_terrainScale;

static GLuint g_overlayTex     = 0;
static GLuint g_stampProg      = 0;
static GLuint g_defaultFootTex = 0;
static GLuint g_dummyVAO       = 0;
static GLint  g_locCenter      = -1;
static GLint  g_locHalfSize    = -1;
static GLint  g_locYaw         = -1;

GLuint terrain_overlay_GetTexture() { return g_overlayTex; }
void   terrain_overlay_Detach()     { g_overlayTex = 0; }

// ---------------------------------------------------------------------------
// Inline stamp shader — fullscreen quad, footprint clipped in UV space
// ---------------------------------------------------------------------------

static const char* STAMP_VERT = R"glsl(
#version 460 core
out vec2 vTexCoord;
void main() {
    vec2 pos = vec2((gl_VertexID & 1) * 2 - 1, (gl_VertexID >> 1) * 2 - 1);
    vTexCoord = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

static const char* STAMP_FRAG = R"glsl(
#version 460 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform vec2      uCenter;
uniform vec2      uHalfSize;
uniform float     uYaw;
uniform sampler2D uFootTex;
void main() {
    vec2  d  = vTexCoord - uCenter;
    float c  = cos(-uYaw), s = sin(-uYaw);
    vec2  lc = vec2(c * d.x - s * d.y, s * d.x + c * d.y);
    if (abs(lc.x) > uHalfSize.x || abs(lc.y) > uHalfSize.y) discard;
    vec2 uv = lc / uHalfSize * 0.5 + 0.5;
    FragColor = texture(uFootTex, uv);
    if (FragColor.a < 0.05) discard;
}
)glsl";

// ---------------------------------------------------------------------------
// GL helpers
// ---------------------------------------------------------------------------

static GLuint compileGLSL(const char* src, GLenum type) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        fprintf(stderr, "[terrain_overlay] shader compile: %s\n", log);
    }
    return s;
}

// Procedural ellipse footprint: rim normals tilt outward, alpha fades to 0 at edge
static GLuint generateFootprintTex() {
    const int SZ = 64;
    std::vector<unsigned char> data(SZ * SZ * 4);
    for (int y = 0; y < SZ; y++) {
        for (int x = 0; x < SZ; x++) {
            float lx  = (x + 0.5f) / SZ * 2.0f - 1.0f;
            float ly  = (y + 0.5f) / SZ * 2.0f - 1.0f;
            float d   = sqrtf(lx * lx + ly * ly);
            unsigned char* px = &data[(y * SZ + x) * 4];
            if (d >= 1.0f) {
                px[0] = 128; px[1] = 128; px[2] = 255; px[3] = 0;
            } else {
                float rim  = sinf(d * 3.14159f);
                float nx   = -lx * rim * 0.6f;
                float ny_  = -ly * rim * 0.6f;
                float nz   = 1.0f;
                float inv  = 1.0f / sqrtf(nx * nx + ny_ * ny_ + nz * nz);
                float alpha = 1.0f - d * d;
                px[0] = (unsigned char)((nx  * inv * 0.5f + 0.5f) * 255.0f);
                px[1] = (unsigned char)((ny_ * inv * 0.5f + 0.5f) * 255.0f);
                px[2] = (unsigned char)((nz  * inv * 0.5f + 0.5f) * 255.0f);
                px[3] = (unsigned char)(alpha * 255.0f);
            }
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SZ, SZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {0.5f, 0.5f, 1.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    return tex;
}

static void lazyInit() {
    if (g_stampProg) return;

    GLuint vs = compileGLSL(STAMP_VERT, GL_VERTEX_SHADER);
    GLuint fs = compileGLSL(STAMP_FRAG, GL_FRAGMENT_SHADER);
    g_stampProg = glCreateProgram();
    glAttachShader(g_stampProg, vs);
    glAttachShader(g_stampProg, fs);
    glLinkProgram(g_stampProg);
    glDeleteShader(vs);
    glDeleteShader(fs);

    g_locCenter   = glGetUniformLocation(g_stampProg, "uCenter");
    g_locHalfSize = glGetUniformLocation(g_stampProg, "uHalfSize");
    g_locYaw      = glGetUniformLocation(g_stampProg, "uYaw");
    glUseProgram(g_stampProg);
    glUniform1i(glGetUniformLocation(g_stampProg, "uFootTex"), 0);
    glUseProgram(0);

    g_defaultFootTex = generateFootprintTex();
    glGenVertexArrays(1, &g_dummyVAO);
}

// ---------------------------------------------------------------------------
// Coordinate helper: world XZ -> terrain UV [0,1]
// ---------------------------------------------------------------------------
static inline void worldToTerrainUV(float wx, float wz, float& outU, float& outV) {
    float halfW = (g_terrainMeshWidth  * g_terrainScale) / 2.0f;
    float halfD = (g_terrainMeshDepth  * g_terrainScale) / 2.0f;
    outU = (wx + halfW) / (g_terrainScale * (g_terrainMeshWidth  - 1));
    outV = (wz + halfD) / (g_terrainScale * (g_terrainMeshDepth - 1));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void terrain_overlay_ApplySpline(GLuint overlayTex, SceneNode* splineNode) {
    if (!overlayTex || !splineNode) return;
    if (splineNode->type != ENTITY_CATMULLROMSPLINE) return;

    CatmullRomNodeData* data = &splineNode->data.catmullrom;
    if (!data->curvePoints || data->curvePointCount < 2) return;

    lazyInit();

    // Query texture dims directly from GL
    glBindTexture(GL_TEXTURE_2D, overlayTex);
    GLint fbW = 0, fbH = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &fbW);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &fbH);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (fbW == 0 || fbH == 0) return;

    // Save GL state before touching anything
    GLint     savedFBO;
    GLint     savedVP[4];
    GLboolean savedBlend, savedScissor;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFBO);
    glGetIntegerv(GL_VIEWPORT, savedVP);
    glGetBooleanv(GL_BLEND,        &savedBlend);
    glGetBooleanv(GL_SCISSOR_TEST, &savedScissor);

    // Wrap overlayTex in a temporary FBO
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, overlayTex, 0);

    glViewport(0, 0, fbW, fbH);
    glClearColor(0.5f, 0.5f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_stampProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_defaultFootTex);
    glBindVertexArray(g_dummyVAO);

    // Footprint size: 0.2% of terrain width, 1.6:1 aspect, stride 1.4× length
    float terrainW = g_terrainScale * (g_terrainMeshWidth  - 1);
    float terrainD = g_terrainScale * (g_terrainMeshDepth  - 1);
    float footW    = terrainW * 0.002f;
    float footL    = footW * 1.6f;
    float halfU    = footW * 0.5f / terrainW;   // UV half-extents
    float halfV    = footL * 0.5f / terrainD;
    float stride   = footL * 1.4f;
    float sideOff  = footW * 0.7f;

    float accumulated = stride;  // start stamping at the very first stride
    int   foot = 0;

    for (int i = 1; i < data->curvePointCount; i++) {
        vec3  prev   = data->curvePoints[i - 1];
        vec3  curr   = data->curvePoints[i];
        vec3  seg    = curr - prev;
        float segLen = vmath::length(seg);
        if (segLen < 0.0001f) continue;

        accumulated += segLen;
        while (accumulated >= stride) {
            accumulated -= stride;

            float t   = 1.0f - accumulated / segLen;
            vec3  pos = prev + seg * t;

            vec3 tang  = vmath::normalize(seg);
            vec3 up    = vec3(0.0f, 1.0f, 0.0f);
            vec3 right = vmath::cross(tang, up);
            float rLen = vmath::length(right);
            right = (rLen > 0.001f) ? right * (1.0f / rLen) : vec3(1.0f, 0.0f, 0.0f);

            float side = (foot % 2 == 0) ? 1.0f : -1.0f;
            vec3  fp   = pos + right * (side * sideOff);

            float u, v;
            worldToTerrainUV(fp[0], fp[2], u, v);
            float yaw = atan2f(tang[0], tang[2]);

            // Scissor to the footprint's pixel rectangle for efficiency
            int px0 = (int)((u - halfU) * fbW) - 1;
            int py0 = (int)((v - halfV) * fbH) - 1;
            int pw  = (int)(halfU * 2.0f * fbW) + 3;
            int ph  = (int)(halfV * 2.0f * fbH) + 3;
            if (px0 < 0)        px0 = 0;
            if (py0 < 0)        py0 = 0;
            if (px0 + pw > fbW) pw  = fbW - px0;
            if (py0 + ph > fbH) ph  = fbH - py0;
            if (pw <= 0 || ph <= 0) { foot++; continue; }

            glEnable(GL_SCISSOR_TEST);
            glScissor(px0, py0, pw, ph);
            glUniform2f(g_locCenter,   u, v);
            glUniform2f(g_locHalfSize, halfU, halfV);
            glUniform1f(g_locYaw,      yaw);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            foot++;
        }
    }

    // Restore GL state
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)savedFBO);
    glDeleteFramebuffers(1, &fbo);
    glViewport(savedVP[0], savedVP[1], savedVP[2], savedVP[3]);
    if (savedBlend)   glEnable(GL_BLEND);        else glDisable(GL_BLEND);
    if (savedScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    g_overlayTex = overlayTex;
}
