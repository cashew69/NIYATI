// NVDF (Nubis Voxel Density Field) authoring tool.
//
// Self-contained ImGui editor for baking 3D cloud density textures that can
// later be sampled by the volumetric cloud raymarcher in place of evaluating
// the full Nubis density function every frame.
//
// File format (.nvdf):
//   char     magic[4]  = "NVDF"
//   uint32_t version   = 1
//   uint32_t width
//   uint32_t height
//   uint32_t depth
//   uint32_t format    (0 = R8)
//   uint8_t  voxels[width * height * depth]   // row-major X, Y, Z
//
// Unity-build file — included by engine.h after volumetricCloudOnCompute.cpp.
// All ImGui usage is HAS_IMGUI-guarded so the headless X11 build still links.

#ifdef HAS_IMGUI

#include "engine/dependancies/imgui/imgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

extern GLuint vcCloud_GetBaseNoiseTex();
extern GLuint vcCloud_GetDetailNoiseTex();
extern char*  readShaderFile(const char* filename);

// ---- Editable parameters ---------------------------------------------------
struct NVDFGenParams {
    int   resX, resY, resZ;
    float cloudType;
    float coverage;
    float noiseScale;
    float detailScale;
    float densityScale;
    float erosion;
    float heightBias;
    float anvilBias;
    char  outputPath[256];
};

static NVDFGenParams s_p = {
    /*res*/      128, 32, 128,
    /*type*/     0.5f,
    /*coverage*/ 0.55f,
    /*nscale*/   2.0f,
    /*dscale*/   8.0f,
    /*dens*/     1.0f,
    /*erode*/    0.45f,
    /*hbias*/    0.0f,
    /*anvil*/    0.0f,
    /*path*/     "engine/effects/clouds/nvdf_textures/cloud.nvdf",
};

// ---- Runtime state ---------------------------------------------------------
static bool   s_winOpen     = false;
static GLuint s_genProg     = 0;   // nvdf_generate.comp.glsl
static GLuint s_previewProg = 0;   // nvdf_preview.comp.glsl
static GLuint s_nvdfTex     = 0;   // R8 3D output from generator
static int    s_nvdfW = 0, s_nvdfH = 0, s_nvdfD = 0;

static GLuint s_previewTex  = 0;   // RGBA8 2D for ImGui::Image
static int    s_previewSize = 256;
static int    s_previewAxis = 1;   // 0=X 1=Y 2=Z
static float  s_previewSlice = 0.5f;
static bool   s_previewDirty = true;
static bool   s_havePresets   = false;

// External texture to preview (set from the cloud attribute panel via
// nvdfgen_SetPreviewSource). When non-zero, the preview shows this instead
// of s_nvdfTex.  Caller owns the texture; we never delete it.
static GLuint s_extPreviewTex = 0;
static int    s_extW = 0, s_extH = 0, s_extD = 0;

// Cached uniform locations
struct {
    GLint cloudType, coverage, noiseScale, detailScale, densityScale;
    GLint erosion, heightBias, anvilBias, useWeatherMap;
    GLint noiseBase, noiseDetail, weatherMap;
} s_gen;

struct {
    GLint nvdf, slice, axis;
} s_prev;

// ---- Compute-shader helper -------------------------------------------------
static GLuint nvdfgen_BuildCompute(const char* path)
{
    char* src = readShaderFile(path);
    if (!src) { LOG_E("nvdfgen: failed to read %s", path); return 0; }

    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(sh, 1, (const GLchar**)&src, NULL);
    glCompileShader(sh);
    free(src);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        LOG_E("nvdfgen: compile %s\n%s", path, log);
        glDeleteShader(sh);
        return 0;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, sh);
    glLinkProgram(p);
    glDetachShader(p, sh);
    glDeleteShader(sh);

    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        LOG_E("nvdfgen: link %s\n%s", path, log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static bool nvdfgen_InitProgs()
{
    if (s_genProg && s_previewProg) return true;

    if (!s_genProg) {
        s_genProg = nvdfgen_BuildCompute(
            "engine/effects/clouds/nvdf_generate.comp.glsl");
        if (!s_genProg) return false;
        s_gen.cloudType     = glGetUniformLocation(s_genProg, "u_cloudType");
        s_gen.coverage      = glGetUniformLocation(s_genProg, "u_coverage");
        s_gen.noiseScale    = glGetUniformLocation(s_genProg, "u_noiseScale");
        s_gen.detailScale   = glGetUniformLocation(s_genProg, "u_detailScale");
        s_gen.densityScale  = glGetUniformLocation(s_genProg, "u_densityScale");
        s_gen.erosion       = glGetUniformLocation(s_genProg, "u_erosion");
        s_gen.heightBias    = glGetUniformLocation(s_genProg, "u_heightBias");
        s_gen.anvilBias     = glGetUniformLocation(s_genProg, "u_anvilBias");
        s_gen.useWeatherMap = glGetUniformLocation(s_genProg, "u_useWeatherMap");
        s_gen.noiseBase     = glGetUniformLocation(s_genProg, "u_noiseBase");
        s_gen.noiseDetail   = glGetUniformLocation(s_genProg, "u_noiseDetail");
        s_gen.weatherMap    = glGetUniformLocation(s_genProg, "u_weatherMap");
    }

    if (!s_previewProg) {
        s_previewProg = nvdfgen_BuildCompute(
            "engine/effects/clouds/nvdf_preview.comp.glsl");
        if (!s_previewProg) return false;
        s_prev.nvdf  = glGetUniformLocation(s_previewProg, "u_nvdf");
        s_prev.slice = glGetUniformLocation(s_previewProg, "u_slice");
        s_prev.axis  = glGetUniformLocation(s_previewProg, "u_axis");
    }

    return true;
}

// ---- Texture lifecycle -----------------------------------------------------
static void nvdfgen_EnsureNVDFTex(int w, int h, int d)
{
    if (s_nvdfTex && s_nvdfW == w && s_nvdfH == h && s_nvdfD == d) return;
    if (s_nvdfTex) glDeleteTextures(1, &s_nvdfTex);

    glGenTextures(1, &s_nvdfTex);
    glBindTexture(GL_TEXTURE_3D, s_nvdfTex);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8, w, h, d);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // S(X) and R(Z) are horizontal tile axes; T(Y) is the cloud height axis.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glBindTexture(GL_TEXTURE_3D, 0);

    s_nvdfW = w; s_nvdfH = h; s_nvdfD = d;
}

static void nvdfgen_EnsurePreviewTex()
{
    if (s_previewTex) return;
    glGenTextures(1, &s_previewTex);
    glBindTexture(GL_TEXTURE_2D, s_previewTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, s_previewSize, s_previewSize);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---- Generation ------------------------------------------------------------
static void nvdfgen_Generate()
{
    if (!nvdfgen_InitProgs()) return;

    GLuint nBase   = vcCloud_GetBaseNoiseTex();
    GLuint nDetail = vcCloud_GetDetailNoiseTex();
    if (!nBase || !nDetail) { LOG_E("nvdfgen: shared noise unavailable"); return; }

    int w = s_p.resX, h = s_p.resY, d = s_p.resZ;
    if (w <= 0 || h <= 0 || d <= 0) return;
    nvdfgen_EnsureNVDFTex(w, h, d);

    glUseProgram(s_genProg);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, nBase);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, nDetail);
    glActiveTexture(GL_TEXTURE0);

    glUniform1i(s_gen.noiseBase,   0);
    glUniform1i(s_gen.noiseDetail, 1);
    glUniform1i(s_gen.weatherMap,  2);
    glUniform1i(s_gen.useWeatherMap, 0);

    glUniform1f(s_gen.cloudType,    s_p.cloudType);
    glUniform1f(s_gen.coverage,     s_p.coverage);
    glUniform1f(s_gen.noiseScale,   s_p.noiseScale);
    glUniform1f(s_gen.detailScale,  s_p.detailScale);
    glUniform1f(s_gen.densityScale, s_p.densityScale);
    glUniform1f(s_gen.erosion,      s_p.erosion);
    glUniform1f(s_gen.heightBias,   s_p.heightBias);
    glUniform1f(s_gen.anvilBias,    s_p.anvilBias);

    glBindImageTexture(0, s_nvdfTex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R8);

    GLuint gx = (w + 7) / 8, gy = (h + 7) / 8, gz = (d + 7) / 8;
    glDispatchCompute(gx, gy, gz);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
                  | GL_TEXTURE_FETCH_BARRIER_BIT);

    glUseProgram(0);
    s_previewDirty = true;
    LOG_I("nvdfgen: baked %dx%dx%d", w, h, d);
}

// ---- Preview ---------------------------------------------------------------
static void nvdfgen_RefreshPreview()
{
    GLuint srcTex = s_extPreviewTex ? s_extPreviewTex : s_nvdfTex;
    if (!srcTex || !nvdfgen_InitProgs()) return;
    nvdfgen_EnsurePreviewTex();

    glUseProgram(s_previewProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, srcTex);
    glUniform1i(s_prev.nvdf, 0);
    glUniform1f(s_prev.slice, s_previewSlice);
    glUniform1i(s_prev.axis,  s_previewAxis);

    glBindImageTexture(0, s_previewTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    GLuint g = (s_previewSize + 7) / 8;
    glDispatchCompute(g, g, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
                  | GL_TEXTURE_FETCH_BARRIER_BIT);
    glUseProgram(0);

    s_previewDirty = false;
}

// ---- Disk I/O --------------------------------------------------------------
static bool nvdfgen_SaveToFile(const char* path)
{
    if (!s_nvdfTex || !path || !path[0]) return false;

    size_t voxels = (size_t)s_nvdfW * s_nvdfH * s_nvdfD;
    unsigned char* buf = (unsigned char*)malloc(voxels);
    if (!buf) { LOG_E("nvdfgen: OOM saving"); return false; }

    glBindTexture(GL_TEXTURE_3D, s_nvdfTex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, buf);
    glBindTexture(GL_TEXTURE_3D, 0);

    FILE* f = fopen(path, "wb");
    if (!f) { LOG_E("nvdfgen: cannot open %s for write", path); free(buf); return false; }

    const char magic[4] = {'N','V','D','F'};
    uint32_t hdr[5] = { 1u,
                        (uint32_t)s_nvdfW, (uint32_t)s_nvdfH, (uint32_t)s_nvdfD,
                        0u /* R8 */ };
    fwrite(magic, 1, 4, f);
    fwrite(hdr,   sizeof(uint32_t), 5, f);
    fwrite(buf,   1, voxels, f);
    fclose(f);
    free(buf);

    LOG_I("nvdfgen: wrote %s  (%dx%dx%d, %zu bytes)",
          path, s_nvdfW, s_nvdfH, s_nvdfD, voxels);
    return true;
}

// ---- Presets ---------------------------------------------------------------
static void nvdfgen_ApplyPreset(int idx)
{
    // Stratus, Cumulus, Cumulonimbus, Stratocumulus
    switch (idx) {
        case 0: // Stratus
            s_p.cloudType = 0.0f; s_p.coverage = 0.85f;
            s_p.erosion   = 0.30f; s_p.anvilBias = 0.0f;
            s_p.heightBias= 0.0f;  s_p.densityScale = 1.0f;
            break;
        case 1: // Cumulus
            s_p.cloudType = 0.7f;  s_p.coverage = 0.45f;
            s_p.erosion   = 0.55f; s_p.anvilBias = 0.0f;
            s_p.heightBias= 0.0f;  s_p.densityScale = 1.1f;
            break;
        case 2: // Cumulonimbus
            s_p.cloudType = 1.0f;  s_p.coverage = 0.70f;
            s_p.erosion   = 0.50f; s_p.anvilBias = 1.0f;
            s_p.heightBias= 0.0f;  s_p.densityScale = 1.3f;
            break;
        case 3: // Stratocumulus
            s_p.cloudType = 0.25f; s_p.coverage = 0.60f;
            s_p.erosion   = 0.45f; s_p.anvilBias = 0.0f;
            s_p.heightBias= 0.0f;  s_p.densityScale = 1.0f;
            break;
    }
}

// ---- ImGui UI --------------------------------------------------------------
void ShowNVDFGenerator()
{
    if (!s_winOpen) return;

    ImGui::SetNextWindowSize(ImVec2(520, 720), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("NVDF Generator", &s_winOpen)) { ImGui::End(); return; }

    ImGui::TextDisabled("Bake a 3D cloud density texture from Nubis-style controls.");
    ImGui::Separator();

    // ---- Presets ----
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Stratus"))         { nvdfgen_ApplyPreset(0); }
        ImGui::SameLine();
        if (ImGui::Button("Stratocumulus"))   { nvdfgen_ApplyPreset(3); }
        ImGui::SameLine();
        if (ImGui::Button("Cumulus"))         { nvdfgen_ApplyPreset(1); }
        ImGui::SameLine();
        if (ImGui::Button("Cumulonimbus"))    { nvdfgen_ApplyPreset(2); }
    }

    // ---- Resolution ----
    if (ImGui::CollapsingHeader("Resolution", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragInt("Res X (width)",  &s_p.resX, 1, 8, 512);
        ImGui::DragInt("Res Y (height)", &s_p.resY, 1, 4, 256);
        ImGui::DragInt("Res Z (depth)",  &s_p.resZ, 1, 8, 512);
        size_t voxels = (size_t)s_p.resX * s_p.resY * s_p.resZ;
        ImGui::TextDisabled("%zu voxels  ≈ %.2f MB (R8)", voxels, voxels / (1024.0 * 1024.0));
    }

    // ---- Shape ----
    if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cloud Type",   &s_p.cloudType, 0.0f, 1.0f,
                           "%.2f  (0=stratus 0.5=strato 1=cumulus)");
        ImGui::SliderFloat("Coverage",     &s_p.coverage,    0.0f, 1.0f);
        ImGui::SliderFloat("Erosion",      &s_p.erosion,     0.0f, 1.0f);
        ImGui::SliderFloat("Height Bias",  &s_p.heightBias, -0.5f, 0.5f);
        ImGui::SliderFloat("Anvil Bias",   &s_p.anvilBias,   0.0f, 1.0f,
                           "%.2f  (cumulonimbus only)");
    }

    // ---- Noise scales ----
    if (ImGui::CollapsingHeader("Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Base Scale",   &s_p.noiseScale,  0.5f, 16.0f,
                           "%.2f  tex-uvw cycles");
        ImGui::SliderFloat("Detail Scale", &s_p.detailScale, 1.0f, 32.0f);
        ImGui::SliderFloat("Density Mul",  &s_p.densityScale,0.1f, 4.0f);
    }

    ImGui::Spacing();
    if (ImGui::Button("Generate / Bake", ImVec2(-1, 32))) {
        nvdfgen_Generate();
    }

    ImGui::Separator();

    // ---- Output ----
    if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("File", s_p.outputPath, sizeof(s_p.outputPath));
        if (ImGui::Button("Save .nvdf", ImVec2(-1, 0))) {
            if (!nvdfgen_SaveToFile(s_p.outputPath))
                LOG_E("nvdfgen: save failed");
        }
        if (s_nvdfTex)
            ImGui::TextDisabled("Last bake: %dx%dx%d (id=%u)",
                                s_nvdfW, s_nvdfH, s_nvdfD, s_nvdfTex);
        else
            ImGui::TextDisabled("No bake yet.");
    }

    // ---- Preview ----
    if (ImGui::CollapsingHeader("Slice Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        GLuint showTex = s_extPreviewTex ? s_extPreviewTex : s_nvdfTex;

        const char* axes[] = { "X", "Y (height)", "Z" };
        int prevAxis = s_previewAxis;
        if (ImGui::Combo("Axis", &s_previewAxis, axes, 3) || prevAxis != s_previewAxis)
            s_previewDirty = true;
        if (ImGui::SliderFloat("Slice", &s_previewSlice, 0.0f, 1.0f))
            s_previewDirty = true;

        if (showTex) {
            int srcW = s_extPreviewTex ? s_extW : s_nvdfW;
            int srcH = s_extPreviewTex ? s_extH : s_nvdfH;
            int srcD = s_extPreviewTex ? s_extD : s_nvdfD;
            ImGui::TextDisabled("%s  %dx%dx%d",
                s_extPreviewTex ? "external (loaded NVDF)" : "baked",
                srcW, srcH, srcD);

            if (s_extPreviewTex && ImGui::Button("Clear external / show baked")) {
                s_extPreviewTex = 0;
                s_previewDirty  = true;
            }

            if (s_previewDirty) nvdfgen_RefreshPreview();

            ImVec2 sz(256, 256);
            ImGui::Image((void*)(intptr_t)s_previewTex, sz, ImVec2(0, 1), ImVec2(1, 0));
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Image((void*)(intptr_t)s_previewTex, ImVec2(512, 512),
                             ImVec2(0, 1), ImVec2(1, 0));
                ImGui::EndTooltip();
            }
        } else {
            ImGui::TextDisabled("Bake a profile or click 'Preview' on a loaded NVDF.");
        }
    }

    ImGui::End();
}

// Called from the cloud attribute panel when the user clicks "Preview".
// Sets the external preview source; does NOT take texture ownership.
void nvdfgen_SetPreviewSource(GLuint tex3d)
{
    s_extPreviewTex = tex3d;
    if (tex3d) {
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH,  &s_extW);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_HEIGHT, &s_extH);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH,  &s_extD);
        glBindTexture(GL_TEXTURE_3D, 0);
    }
    s_previewDirty = true;
    s_winOpen      = true;   // open the generator window automatically
}

void OpenNVDFGenerator()  { s_winOpen = true; }
void CloseNVDFGenerator() { s_winOpen = false; }
bool IsNVDFGeneratorOpen(){ return s_winOpen; }

#endif // HAS_IMGUI
