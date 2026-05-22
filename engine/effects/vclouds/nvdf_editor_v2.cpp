#include "nvdf_editor_v2.h"
#include "nvdf_compressor.h"
#include "engine/core/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"
#endif

extern char* readShaderFile(const char* filename);
extern GLuint vcCloud_GetBaseNoiseTex();
extern GLuint vcCloud_GetDetailNoiseTex();

// Preview texture size — fixed 256×256 regardless of source resolution.
static constexpr int PREVIEW_TEX_SIZE = 256;

NVDFEditorV2& NVDFEditorV2::Get() {
    static NVDFEditorV2 instance;
    return instance;
}

void NVDFEditorV2::Init() {
    if (m_profiles.empty()) {
        m_profiles.push_back({"Stratus Layer", 0.0f, 0.45f});
        m_profiles.push_back({"Cumulus Congestus", 1.0f, 0.65f});
    }
}

void NVDFEditorV2::DrawGUI() {
#ifdef HAS_IMGUI
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(700, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("NVDF V2: Cloudscape Baker", &m_open);

    if (ImGui::CollapsingHeader("Volume Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt3("Resolution", &m_resX);
        ImGui::InputText("Export Path", m_exportPath, 256);
        if (ImGui::Button("Re-allocate 3D Texture")) {
            // Re-gen m_nvdfTex
        }
    }

    ImGui::Separator();

    ImGui::Columns(2, "EditorSplit", true);
    ImGui::SetColumnWidth(0, 200);

    // Left column: Profile List
    ImGui::Text("Cloud Profiles");
    if (ImGui::Button("Add Profile")) {
        m_profiles.push_back({"New Cloudscape", 0.5f, 0.5f});
    }

    for (int i = 0; i < (int)m_profiles.size(); ++i) {
        char label[64];
        snprintf(label, 64, "%s##%d", m_profiles[i].name.c_str(), i);
        if (ImGui::Selectable(label, m_selectedProfile == i))
            m_selectedProfile = i;
    }

    ImGui::NextColumn();

    // Right column: Selected Profile Parameters
    if (m_selectedProfile >= 0 && m_selectedProfile < (int)m_profiles.size()) {
        NVDFProfile& p = m_profiles[m_selectedProfile];

        char nameBuf[64];
        strncpy(nameBuf, p.name.c_str(), 64);
        if (ImGui::InputText("Name", nameBuf, 64)) p.name = nameBuf;

        ImGui::SliderFloat("Cloud Type", &p.cloudType, 0.0f, 1.0f);
        ImGui::SliderFloat("Coverage",   &p.coverage,  0.0f, 1.0f);
        ImGui::SliderFloat("Noise Scale",&p.noiseScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Detail Scale",&p.detailScale,1.0f, 20.0f);
        ImGui::SliderFloat("Density",    &p.densityScale,0.0f, 5.0f);
        ImGui::SliderFloat("Erosion",    &p.erosion,    0.0f, 1.0f);
        ImGui::SliderFloat("Height Bias",&p.heightBias,-1.0f, 1.0f);
        ImGui::SliderFloat("Anvil Bias", &p.anvilBias,  0.0f, 1.0f);
        ImGui::SliderFloat("Curlyness",  &p.curlyness,  0.0f, 2.0f);

        if (ImGui::Button("Bake This Profile", ImVec2(-1, 0)))
            BakeCurrentProfile();
    }

    ImGui::Columns(1);
    ImGui::Separator();

    if (ImGui::Button("EXPORT ALL TO BC6H", ImVec2(-1, 40)))
        ExportBC6H();

    // ── NVDF Slice Viewer ────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::CollapsingHeader("NVDF Slice Viewer", ImGuiTreeNodeFlags_DefaultOpen)) {

        GLuint viewTex = m_sourceTex ? m_sourceTex : m_nvdfTex;
        if (!viewTex) {
            ImGui::TextDisabled("No NVDF loaded. Bake a profile or click 'Preview'");
            ImGui::TextDisabled("in the cloud attribute panel to load an existing file.");
        } else {
            // Info line
            ImGui::TextDisabled("Source: %s  (%dx%dx%d)",
                m_sourceTex ? "external" : "baked",
                m_previewSrcW, m_previewSrcH, m_previewSrcD);

            // Axis selector
            bool axisChanged = false;
            ImGui::Text("Axis:");
            ImGui::SameLine();
            if (ImGui::RadioButton("X##ax", m_previewAxis == 0)) { m_previewAxis = 0; axisChanged = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("Y (height)##ax", m_previewAxis == 1)) { m_previewAxis = 1; axisChanged = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("Z##ax", m_previewAxis == 2)) { m_previewAxis = 2; axisChanged = true; }

            // Slice slider
            float oldSlice = m_previewSlice;
            ImGui::SliderFloat("Slice", &m_previewSlice, 0.0f, 1.0f, "%.3f");
            if (axisChanged || m_previewSlice != oldSlice || m_previewDirty)
                RefreshPreview();

            // Display preview texture
            if (m_previewTex) {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float  side  = (avail.x < avail.y ? avail.x : avail.y);
                side = side > 256.0f ? 256.0f : side;
                ImGui::Image((ImTextureID)(intptr_t)m_previewTex, ImVec2(side, side),
                             ImVec2(0,1), ImVec2(1,0));
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Image((ImTextureID)(intptr_t)m_previewTex, ImVec2(512, 512),
                                 ImVec2(0,1), ImVec2(1,0));
                    ImGui::EndTooltip();
                }
            }
        }
    }

    ImGui::End();
#endif
}


void NVDFEditorV2::SetPreviewTex(GLuint tex3d) {
    m_sourceTex = tex3d;
    if (tex3d) {
        glBindTexture(GL_TEXTURE_3D, tex3d);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH,  &m_previewSrcW);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_HEIGHT, &m_previewSrcH);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH,  &m_previewSrcD);
        glBindTexture(GL_TEXTURE_3D, 0);
    }
    m_previewDirty = true;
    EnsurePreviewResources();
    RefreshPreview();
}

void NVDFEditorV2::EnsurePreviewResources() {
    // 2D RGBA8 output texture for ImGui::Image
    if (!m_previewTex) {
        glGenTextures(1, &m_previewTex);
        glBindTexture(GL_TEXTURE_2D, m_previewTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, PREVIEW_TEX_SIZE, PREVIEW_TEX_SIZE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Compile preview compute shader once
    if (!m_previewProg) {
        char* src = readShaderFile("engine/effects/clouds/nvdf_preview.comp.glsl");
        if (!src) {
            LOG_E("NVDFEditorV2: cannot read nvdf_preview.comp.glsl");
            return;
        }
        GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(sh, 1, (const char**)&src, nullptr);
        glCompileShader(sh);
        free(src);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(sh, 512, nullptr, log);
            LOG_E("NVDFEditorV2 preview shader: %s", log);
            glDeleteShader(sh); return;
        }

        m_previewProg = glCreateProgram();
        glAttachShader(m_previewProg, sh);
        glLinkProgram(m_previewProg);
        glDeleteShader(sh);

        glGetProgramiv(m_previewProg, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetProgramInfoLog(m_previewProg, 512, nullptr, log);
            LOG_E("NVDFEditorV2 preview link: %s", log);
            glDeleteProgram(m_previewProg); m_previewProg = 0;
        }
    }
}

void NVDFEditorV2::BakeCurrentProfile() {
    // 1. Setup Compute Shader (m_genProg)
    // 2. Bind m_nvdfTex as Image
    // 3. Set Uniforms from m_profiles[m_selectedProfile]
    // 4. Dispatch
    LOG_I("Baking profile %d...", m_selectedProfile);
}

void NVDFEditorV2::ExportBC6H() {
    // 1. Read back m_nvdfTex from GPU (R8)
    std::vector<uint8_t> r8Data(m_resX * m_resY * m_resZ);
    glBindTexture(GL_TEXTURE_3D, m_nvdfTex);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, r8Data.data());

    // 2. Compress
    LOG_I("Compressing to BC6H...");
    std::vector<uint8_t> bc6Data = NVDFCompressor::CompressVolume(m_resX, m_resY, m_resZ, r8Data.data());

    // 3. Write .nvdf file
    FILE* f = fopen(m_exportPath, "wb");
    if (!f) { LOG_E("Export failed: %s", m_exportPath); return; }

    uint32_t magic = 0x4644564E; // "NVDF"
    uint32_t version = 1;
    uint32_t dims[3] = {(uint32_t)m_resX, (uint32_t)m_resY, (uint32_t)m_resZ};
    uint32_t format = 1; // BC6H

    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(dims, 4, 3, f);
    fwrite(&format, 4, 1, f);
    fwrite(bc6Data.data(), 1, bc6Data.size(), f);
    fclose(f);

    LOG_I("Successfully exported BC6H NVDF to %s", m_exportPath);
}

void NVDFEditorV2::RefreshPreview() {
    m_previewDirty = false;

    GLuint viewTex = m_sourceTex ? m_sourceTex : m_nvdfTex;
    if (!viewTex || !m_previewProg || !m_previewTex) return;

    glUseProgram(m_previewProg);

    // Bind preview image (output)
    glBindImageTexture(0, m_previewTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

    // Bind 3D source as sampler on unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, viewTex);
    glUniform1i(glGetUniformLocation(m_previewProg, "u_nvdf"), 1);

    glUniform1f(glGetUniformLocation(m_previewProg, "u_slice"), m_previewSlice);
    glUniform1i(glGetUniformLocation(m_previewProg, "u_axis"),  m_previewAxis);

    int groups = (PREVIEW_TEX_SIZE + 7) / 8;
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
}

void NVDFEditorV2::Shutdown() {
    if (m_previewTex)  { glDeleteTextures(1, &m_previewTex);  m_previewTex  = 0; }
    if (m_genProg)     { glDeleteProgram(m_genProg);           m_genProg     = 0; }
    if (m_previewProg) { glDeleteProgram(m_previewProg);       m_previewProg = 0; }
}
