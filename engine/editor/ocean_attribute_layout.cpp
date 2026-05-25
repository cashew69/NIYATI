#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"
#include "engine/dependancies/imgui/imfilebrowser.h"

static ImGui::FileBrowser s_OceanNormalBrowser(
    ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter);
static bool s_OceanBrowserInited = false;
static OceanNodeData* s_OceanBrowserTarget = nullptr;

void ShowOceanAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_OCEAN) return;
    OceanNodeData* od = &node->data.ocean;

    if (ImGui::CollapsingHeader("Normal Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        float browseW = 28.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("##oceanNorm", od->normalMapPath, sizeof(od->normalMapPath));
        ImGui::SameLine();
        if (ImGui::Button("...##oceanNormBrowse", ImVec2(browseW, 0))) {
            if (!s_OceanBrowserInited) {
                s_OceanNormalBrowser.SetTitle("Select Normal Map");
                s_OceanNormalBrowser.SetTypeFilters({ ".png", ".jpg", ".jpeg", ".tga", ".bmp" });
                s_OceanBrowserInited = true;
            }
            s_OceanBrowserTarget = od;
            s_OceanNormalBrowser.Open();
        }
        if (ImGui::Button("Reload Normal Map##ocean", ImVec2(-1, 0))) {
            extern GLuint loadGLTexture(const char* filename);
            extern void sg_InitOceanNode(SceneNode* node);
            sg_InitOceanNode(node);
        }
        if (od->normalMapTex)
            ImGui::TextDisabled("Tex ID: %u", od->normalMapTex);
    }

    if (ImGui::CollapsingHeader("Waves", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Height##wave",      &od->waveHeight,    0.01f, 0.01f, 20.0f);
        ImGui::DragFloat("Speed##wave",       &od->waveSpeed,     0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Radius##wave",      &od->waveRadius,    0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Pointiness##wave",  &od->wavePointiness,0.01f, 0.0f,  2.0f);
        ImGui::Separator();
        ImGui::DragFloat("Storm Intensity",   &od->stormIntensity,0.01f, 0.0f,  1.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("0 = calm, 1 = storm.\nMultiplies height x4, speed x2.5, tightens radius, sharpens crests.");
    }

    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Deep Color",    od->deepColor);
        ImGui::ColorEdit3("Shallow Color", od->shallowColor);
        ImGui::Separator();
        ImGui::DragFloat("Roughness",    &od->roughness,    0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Fresnel F0",   &od->fresnelF0,    0.001f, 0.0f, 0.1f, "%.4f");
        ImGui::Separator();
        ImGui::DragFloat("Foam Strength",&od->foamStrength, 0.005f, 0.0f, 1.0f);
        ImGui::ColorEdit3("Foam Color",   od->foamColor);
    }

    // Display normal map browser — must be called outside any Begin/End pair.
    // We store a deferred reload flag and handle it after the browser closes.
    static bool s_OceanPendingReload = false;
    s_OceanNormalBrowser.Display();
    if (s_OceanNormalBrowser.HasSelected() && s_OceanBrowserTarget) {
        auto path = s_OceanNormalBrowser.GetSelected().string();
        strncpy(s_OceanBrowserTarget->normalMapPath, path.c_str(),
                sizeof(s_OceanBrowserTarget->normalMapPath) - 1);
        s_OceanBrowserTarget = nullptr;
        s_OceanNormalBrowser.ClearSelected();
        s_OceanPendingReload = true;
    }
    if (s_OceanPendingReload) {
        extern void sg_InitOceanNode(SceneNode* node);
        sg_InitOceanNode(node);
        s_OceanPendingReload = false;
    }
}
