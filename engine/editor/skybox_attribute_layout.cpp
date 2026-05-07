#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"
#include <stdio.h>

extern void reloadSkyboxPreset(int presetIndex);
extern void initSkybox(const char* hdrPath);

void ShowSkyboxAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    SkyboxNodeData* data = &node->data.skybox;

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* presets[] = { "Night Sky", "Standard Daylight" };
        if (ImGui::Combo("Skybox Preset", &data->currentPreset, presets, 2)) {
            reloadSkyboxPreset(data->currentPreset);
            data->hdrPath[0] = '\0';
        }
    }

    if (ImGui::CollapsingHeader("HDR Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("HDR Path", data->hdrPath, sizeof(data->hdrPath));
        if (ImGui::Button("Load HDR")) {
            if (data->hdrPath[0] != '\0') {
                initSkybox(data->hdrPath);
            }
        }
    }
}
