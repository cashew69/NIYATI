#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"
#include "engine/dependancies/imgui/imfilebrowser.h"

// Static browser — displayed from showAttributeEditorUI() after ImGui::End()
static ImGui::FileBrowser s_SdfTexBrowser(ImGuiFileBrowserFlags_CloseOnEsc |
                                          ImGuiFileBrowserFlags_ConfirmOnEnter);
static SDFNodeData* s_SdfTexTarget = nullptr;

void ShowSDFAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_SDF) return;
    SDFNodeData* sd = &node->data.sdf;

    // ---- Shape ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("SDF Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* shapes[] = { "Sphere" };
        ImGui::Combo("Shape Type", &sd->shapeType, shapes, 1);

        const char* ops[] = { "Union", "Smooth Union" };
        ImGui::Combo("Operation", &sd->operation, ops, 2);

        ImGui::DragFloat("Radius", &sd->radius, 0.05f, 0.05f, 500.0f);

        if (sd->operation == 1)
            ImGui::DragFloat("Smooth K", &sd->smoothK, 0.01f, 0.001f, 20.0f);
    }

    // ---- Material ---------------------------------------------------------------
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Color", sd->color);
        ImGui::SliderFloat("Opacity", &sd->opacity, 0.0f, 1.0f);
        ImGui::TextDisabled("Opacity < 1 disables depth write (transparent).");
    }

    // ---- Texture ----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Texture")) {
        if (sd->texturePath[0])
            ImGui::TextWrapped("%s", sd->texturePath);
        else
            ImGui::TextDisabled("(no texture)");

        if (ImGui::Button("Browse...##sdfTex")) {
            s_SdfTexBrowser.SetTitle("Select Texture Image");
            s_SdfTexBrowser.SetTypeFilters({ ".png", ".jpg", ".jpeg", ".bmp", ".tga" });
            s_SdfTexBrowser.Open();
            s_SdfTexTarget = sd;
        }

        if (sd->texturePath[0]) {
            ImGui::SameLine();
            if (ImGui::Button("Clear##sdfTex")) {
                sd->texturePath[0] = '\0';
                sd->textureID = 0;
            }
            if (sd->textureID != 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(#%u)", sd->textureID);
            }
        }
    }

    // ---- Raymarching ------------------------------------------------------------
    if (ImGui::CollapsingHeader("Raymarching")) {
        ImGui::TextDisabled("Settings below are read from the first SDF node.");
        ImGui::DragInt  ("Max Steps", &sd->maxSteps, 1.0f, 8, 512);
        ImGui::DragFloat("Surf Dist", &sd->surfDist, 0.00001f, 0.00001f, 0.01f, "%.5f");
        ImGui::DragFloat("Max Dist",  &sd->maxDist,  5.0f,     1.0f,     5000.0f);
    }
}
