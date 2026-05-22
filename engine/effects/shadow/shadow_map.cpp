// Unity-build file — included by engine.h after all engine headers.
// All engine types (mat4, vec3, GLuint, ShaderProgram, ShadowMap ...) are
// already defined when this file is reached.

#include <stdlib.h>
#include <math.h>

extern FILE*       gpFile;
extern Bool        buildShaderProgramFromFiles(const char**, int, ShaderProgram**, const char**, GLint*, int);
extern const char* attribNames[4];
extern GLint       attribIndices[4];

// Saved GL state for begin/end bracketing
static GLint   s_PrevFBO          = 0;
static GLint   s_PrevViewport[4]  = {0,0,0,0};
static GLint   s_PrevScissorBox[4]= {0,0,0,0};
static GLboolean s_PrevScissor    = GL_FALSE;
static GLboolean s_PrevCullFace   = GL_FALSE;
static GLboolean s_PrevDepthTest  = GL_FALSE;
static GLboolean s_PrevDepthMask  = GL_TRUE;
static GLboolean s_PrevBlend      = GL_FALSE;
static GLint     s_PrevDepthFunc  = GL_LESS;
static GLboolean s_PrevColorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};

static void checkGLError(const char* label) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(gpFile, "[Shadow GL Error] %s: 0x%x\n", label, err);
    }
}

ShadowMap* shadow_Create(int resolution) {
    ShadowMap* sm = new ShadowMap();
    memset(sm, 0, sizeof(ShadowMap));
    sm->resolution       = resolution;
    sm->orthoSize        = 200.0f;
    sm->nearPlane        = 1.0f;
    sm->farPlane         = 400.0f;
    sm->bias             = 0.002f;
    sm->polyOffsetFactor = 1.5f;
    sm->polyOffsetUnits  = 4.0f;

    // Save current FBO so we don't accidentally leak it.
    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);

    glGenFramebuffers(1, &sm->fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, sm->fboID);

    glGenTextures(1, &sm->depthTexID);
    glBindTexture(GL_TEXTURE_2D, sm->depthTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                 resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // IMPORTANT: keep COMPARE_MODE = GL_NONE at creation time.
    // On AMD/Mesa, having GL_COMPARE_REF_TO_TEXTURE active while the texture is
    // attached as the FBO depth target can cause depth writes to silently fail.
    // We toggle COMPARE_MODE on right before sampling in the lit pass.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    // Swizzle the depth as R, R, R, 1 so that when sampled as a plain sampler2D
    // (e.g., for the ImGui debug viewer) it shows up as grayscale instead of red.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sm->depthTexID, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        fprintf(gpFile, "[Shadow] ERROR: FBO incomplete, status=0x%x\n", fboStatus);
    else
        fprintf(gpFile, "[Shadow] FBO complete. depthTexID=%u fboID=%u\n", sm->depthTexID, sm->fboID);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);

    const char* depthFiles[5] = {
        "engine/shaders/shadow_depth.vert",
        NULL, NULL, NULL,
        "engine/shaders/shadow_depth.frag"
    };
    sm->depthProgram = nullptr;
    buildShaderProgramFromFiles(depthFiles, 5, &sm->depthProgram, attribNames, attribIndices, 4);
    sm->locDepthMVP = (sm->depthProgram && sm->depthProgram->id)
                      ? glGetUniformLocation(sm->depthProgram->id, "uLightMVP")
                      : -1;

    fprintf(gpFile, "[Shadow] Created %dx%d shadow map. depthProg=%u locDepthMVP=%d\n",
            resolution, resolution,
            sm->depthProgram ? sm->depthProgram->id : 0,
            sm->locDepthMVP);
    return sm;
}

void shadow_Destroy(ShadowMap* sm) {
    if (!sm) return;
    if (sm->depthTexID) glDeleteTextures(1, &sm->depthTexID);
    if (sm->fboID)      glDeleteFramebuffers(1, &sm->fboID);
    if (sm->depthProgram) {
        if (sm->depthProgram->id) glDeleteProgram(sm->depthProgram->id);
        delete sm->depthProgram;
    }
    delete sm;
}

void shadow_UpdateMatrices(ShadowMap* sm, int lightType, vec3 lightPos, vec3 lightDir, float outerCutoff) {
    if (!sm) return;

    // Normalize lightDir defensively — otherwise lookat / depth values go wild.
    float dlen = sqrtf(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
    if (dlen < 1e-5f) {
        // Degenerate input — fallback to "sun straight down".
        lightDir = vec3(0.0f, -1.0f, 0.0f);
    } else {
        lightDir = vec3(lightDir[0]/dlen, lightDir[1]/dlen, lightDir[2]/dlen);
    }

    // Pick an up vector that is not (anti-)parallel to lightDir.
    vec3 up = (fabsf(lightDir[1]) > 0.99f) ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);

    if (lightType == LIGHT_DIRECTIONAL) {
        vec3 target = lightPos;
        vec3 eye    = target - lightDir * (sm->farPlane * 0.5f);
        sm->lightView = lookat(eye, target, up);
        sm->lightProj = ortho(-sm->orthoSize, sm->orthoSize,
                              -sm->orthoSize, sm->orthoSize,
                               sm->nearPlane, sm->farPlane);
        sm->debugEye    = eye;
        sm->debugTarget = target;
    } else { // Spot
        vec3 target   = lightPos + lightDir;
        sm->lightView = lookat(lightPos, target, up);
        float fovDeg  = acosf(outerCutoff) * 2.0f * (180.0f / 3.14159265f);
        if (fovDeg < 1.0f)   fovDeg = 1.0f;
        if (fovDeg > 170.0f) fovDeg = 170.0f;
        sm->lightProj = perspective(fovDeg, 1.0f, sm->nearPlane, sm->farPlane);
        sm->debugEye    = lightPos;
        sm->debugTarget = target;
    }

    sm->sbpv = sm->lightProj * sm->lightView;
}

void shadow_BeginPass(ShadowMap* sm) {
    if (!sm || !sm->fboID) return;

    // ---- Snapshot caller's GL state so we can restore it cleanly in EndPass ----
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s_PrevFBO);
    glGetIntegerv(GL_VIEWPORT,                 s_PrevViewport);
    glGetIntegerv(GL_SCISSOR_BOX,              s_PrevScissorBox);
    s_PrevScissor   = glIsEnabled(GL_SCISSOR_TEST);
    s_PrevCullFace  = glIsEnabled(GL_CULL_FACE);
    s_PrevDepthTest = glIsEnabled(GL_DEPTH_TEST);
    s_PrevBlend     = glIsEnabled(GL_BLEND);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &s_PrevDepthMask);
    glGetIntegerv(GL_DEPTH_FUNC,      &s_PrevDepthFunc);
    glGetBooleanv(GL_COLOR_WRITEMASK, s_PrevColorMask);

    // ---- Detach the shadow texture from any sampler slot before rendering to it ----
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    // Bind shadow FBO
    glBindFramebuffer(GL_FRAMEBUFFER, sm->fboID);
    checkGLError("BeginPass:bind FBO");

    // ---- Defensive state: anything in the caller (ImGui leftover scissor, etc.)
    //     could clip away the clear and the draws and leave the texture at its
    //     initial (undefined / zero) value. Wipe it all here. ----
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
#ifdef GL_DEPTH_CLAMP
    glDisable(GL_DEPTH_CLAMP);
#endif
    glDepthRange(0.0, 1.0);

    glViewport(0, 0, sm->resolution, sm->resolution);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    checkGLError("BeginPass:clear depth");

    // Polygon offset — tunable per shadow map from the UI. 0/0 disables it
    // entirely (useful when debugging where shadows ought to be).
    if (sm->polyOffsetFactor != 0.0f || sm->polyOffsetUnits != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(sm->polyOffsetFactor, sm->polyOffsetUnits);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // FBO with depth-only attachment — ensure the driver isn't expecting a color draw.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    checkGLError("BeginPass:done");
}

void shadow_EndPass(ShadowMap* sm) {
    if (!sm) return;

    glDisable(GL_POLYGON_OFFSET_FILL);

    // ---- One-time diagnostic: read back a few pixels from the depth texture
    //     so we can SEE whether the depth pass actually wrote anything. ----
    static int s_readbackFrames = 0;
    if (s_readbackFrames < 3) {
        s_readbackFrames++;
        // Sample 9 points across the texture
        float pix[9] = {0};
        struct { int x, y; } pts[9] = {
            {0,0}, {sm->resolution/2,0}, {sm->resolution-1,0},
            {0,sm->resolution/2}, {sm->resolution/2,sm->resolution/2}, {sm->resolution-1,sm->resolution/2},
            {0,sm->resolution-1}, {sm->resolution/2,sm->resolution-1}, {sm->resolution-1,sm->resolution-1}
        };
        for (int i = 0; i < 9; i++) {
            glReadPixels(pts[i].x, pts[i].y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &pix[i]);
        }
        float mn = 1.0f, mx = 0.0f;
        for (int i = 0; i < 9; i++) {
            if (pix[i] < mn) mn = pix[i];
            if (pix[i] > mx) mx = pix[i];
        }
        fprintf(gpFile, "[Shadow READBACK f%d] center=%.4f min=%.4f max=%.4f "
                        "corners=[%.3f %.3f %.3f %.3f]\n",
                s_readbackFrames, pix[4], mn, mx,
                pix[0], pix[2], pix[6], pix[8]);
    }

    // ---- Restore caller's GL state ----
    if (s_PrevColorMask[0] || s_PrevColorMask[1] || s_PrevColorMask[2] || s_PrevColorMask[3])
        glColorMask(s_PrevColorMask[0], s_PrevColorMask[1], s_PrevColorMask[2], s_PrevColorMask[3]);
    else
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glDepthMask(s_PrevDepthMask);
    glDepthFunc(s_PrevDepthFunc ? s_PrevDepthFunc : GL_LEQUAL);
    if (s_PrevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (s_PrevBlend)     glEnable(GL_BLEND);      else glDisable(GL_BLEND);
    if (s_PrevCullFace)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (s_PrevScissor)   glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glScissor(s_PrevScissorBox[0], s_PrevScissorBox[1], s_PrevScissorBox[2], s_PrevScissorBox[3]);
    glViewport(s_PrevViewport[0], s_PrevViewport[1], s_PrevViewport[2], s_PrevViewport[3]);

    // Restore the framebuffer the caller had bound (NOT necessarily 0 — the
    // editor render-to-texture flow has its own FBO bound).
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)s_PrevFBO);

    checkGLError("EndPass");
}

// Returns number of draws attempted
int shadow_DrawDepth(ShadowMap* sm, GLuint vao, int indexCount, mat4 worldMatrix) {
    if (!sm || sm->locDepthMVP < 0 || vao == 0 || indexCount <= 0) return 0;
    mat4 mvp = sm->lightProj * sm->lightView * worldMatrix;
    glUniformMatrix4fv(sm->locDepthMVP, 1, GL_FALSE, (const float*)mvp);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    return 1;
}
