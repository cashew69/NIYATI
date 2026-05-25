#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"
#include "engine/dependancies/imgui/imfilebrowser.h"
#include <stdio.h>
#include <string>

void ShowTerrainAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    TerrainNodeData* data = &node->data.terrain;

    static ImGui::FileBrowser diffuseBrowser(ImGuiFileBrowserFlags_CloseOnEsc);
    static ImGui::FileBrowser normalBrowser(ImGuiFileBrowserFlags_CloseOnEsc);
    static ImGui::FileBrowser armBrowser(ImGuiFileBrowserFlags_CloseOnEsc);
    static ImGui::FileBrowser dispBrowser(ImGuiFileBrowserFlags_CloseOnEsc);
    static bool browsersInited = false;
    if (!browsersInited) {
        diffuseBrowser.SetTitle("Select Diffuse Texture");
        diffuseBrowser.SetTypeFilters({ ".png", ".jpg", ".tga", ".bmp" });
        normalBrowser.SetTitle("Select Normal Map");
        normalBrowser.SetTypeFilters({ ".png", ".jpg", ".tga", ".bmp" });
        armBrowser.SetTitle("Select ARM Map");
        armBrowser.SetTypeFilters({ ".png", ".jpg", ".tga", ".bmp" });
        dispBrowser.SetTitle("Select Displacement Map");
        dispBrowser.SetTypeFilters({ ".png", ".jpg", ".tga", ".bmp" });
        browsersInited = true;
    }

    if (ImGui::CollapsingHeader("Seed", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Seed Value", &data->seed);
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) data->seed = rand();
    }

    if (ImGui::CollapsingHeader("fBm Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Octaves", &data->octaves, 1, 1);
        if (data->octaves < 1) data->octaves = 1;
        if (data->octaves > 12) data->octaves = 12;
        ImGui::InputFloat("Persistence", &data->persistence, 0.05f, 0.1f, "%.3f");
        if (data->persistence < 0.01f) data->persistence = 0.01f;
        if (data->persistence > 1.0f)  data->persistence = 1.0f;
        ImGui::InputFloat("Lacunarity",  &data->lacunarity,  0.1f, 0.5f, "%.2f");
        if (data->lacunarity < 1.0f) data->lacunarity = 1.0f;
        if (data->lacunarity > 4.0f) data->lacunarity = 4.0f;
    }

    if (ImGui::CollapsingHeader("Terrain Modifiers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat("Ridge Strength", &data->ridgeStrength, 0.1f, 0.25f, "%.2f");
        if (data->ridgeStrength < 0.0f) data->ridgeStrength = 0.0f;
        if (data->ridgeStrength > 1.0f) data->ridgeStrength = 1.0f;
        ImGui::InputFloat("Turbulence", &data->turbulence, 0.5f, 1.0f, "%.2f");
        if (data->turbulence < 0.0f) data->turbulence = 0.0f;
        ImGui::InputInt("Terrace Levels", &data->terraceLevels, 1, 5);
        if (data->terraceLevels < 0)  data->terraceLevels = 0;
        if (data->terraceLevels > 50) data->terraceLevels = 50;
        ImGui::InputFloat("Power Curve", &data->powerCurve, 0.1f, 0.25f, "%.2f");
        if (data->powerCurve < 0.1f) data->powerCurve = 0.1f;
        if (data->powerCurve > 4.0f) data->powerCurve = 4.0f;
        ImGui::InputFloat("Height Offset", &data->heightOffset, 1.0f, 5.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Biome Settings")) {
        const char* biomeModes[] = { "Auto (Noise-based)", "Mountains Only", "Desert Only", "Plains Only" };
        ImGui::Combo("Biome Mode", &data->biomeMode, biomeModes, 4);
        if (data->biomeMode == 0) {
            ImGui::Separator(); ImGui::Text("Biome Thresholds:");
            ImGui::InputFloat("Mountain Threshold", &data->mountainThreshold, 0.05f, 0.1f, "%.2f");
            ImGui::InputFloat("Desert Threshold",   &data->desertThreshold,   0.05f, 0.1f, "%.2f");
        }
        ImGui::Separator(); ImGui::Text("Height Scales:");
        ImGui::InputFloat("Mountain Height", &data->mountainHeightScale, 5.0f, 10.0f, "%.1f");
        ImGui::InputFloat("Desert Height",   &data->desertHeightScale,   1.0f, 5.0f,  "%.1f");
        ImGui::InputFloat("Plains Height",   &data->plainsHeightScale,   2.0f, 5.0f,  "%.1f");
        ImGui::Separator();
        ImGui::Checkbox("Island Mask", &data->useIslandMask);
        if (data->useIslandMask) {
            ImGui::InputFloat("Island Falloff", &data->islandFalloff, 0.1f, 0.5f, "%.2f");
            if (data->islandFalloff < 0.1f) data->islandFalloff = 0.1f;
        }
    }

    if (ImGui::CollapsingHeader("Mesh Resolution", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Width##mesh", &data->meshWidth, 16, 64);
        if (data->meshWidth < 16)   data->meshWidth = 16;
        if (data->meshWidth > 1024) data->meshWidth = 1024;
        ImGui::InputInt("Depth##mesh", &data->meshDepth, 16, 64);
        if (data->meshDepth < 16)   data->meshDepth = 16;
        if (data->meshDepth > 1024) data->meshDepth = 1024;
        ImGui::InputFloat("World Scale##mesh", &data->worldScale, 1.0f, 5.0f, "%.1f");
        if (data->worldScale < 0.1f) data->worldScale = 0.1f;
        ImGui::Text("Total vertices: %d", data->meshWidth * data->meshDepth);
    }

    if (ImGui::CollapsingHeader("LOD / Tessellation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Inner Tess Level", &data->tessInner, 1, 2);
        if (data->tessInner < 1)  data->tessInner = 1;
        if (data->tessInner > 64) data->tessInner = 64;
        ImGui::InputInt("Outer Tess Level", &data->tessOuter, 1, 2);
        if (data->tessOuter < 1)  data->tessOuter = 1;
        if (data->tessOuter > 64) data->tessOuter = 64;
        ImGui::InputFloat("Displacement Scale", &data->displacementScale, 5.0f, 10.0f, "%.1f");
        ImGui::InputInt("LOD Bias", &data->lodBias, 1, 2);
        if (data->lodBias < -5) data->lodBias = -5;
        if (data->lodBias > 5)  data->lodBias = 5;
    }

    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::Checkbox("Wireframe Mode", &data->wireframe);
    }

    if (ImGui::CollapsingHeader("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* materialNames[] = { "Aerial Beach", "Gray Rocks", "Red Mud Stones", "Custom" };
        int currentIdx = data->materialIndex;
        if (currentIdx == -1) currentIdx = 3; // "Custom"
        
        if (ImGui::Combo("Material", &currentIdx, materialNames, 4)) {
            if (currentIdx == 3) data->materialIndex = -1;
            else data->materialIndex = currentIdx;
            
            extern void switchTerrainMaterial(int materialIndex);
            switchTerrainMaterial(data->materialIndex);
        }
        
        if (data->materialIndex == -1) {
            ImGui::Indent();
            ImGui::Text("Custom Textures:");
            
            auto pathSelector = [](const char* label, char* buffer, ImGui::FileBrowser& browser) {
                ImGui::Text("%s:", label);
                ImGui::SameLine(100);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 35);
                ImGui::InputText((std::string("##") + label).c_str(), buffer, 256);
                ImGui::SameLine();
                if (ImGui::Button((std::string("...##") + label).c_str())) {
                    browser.Open();
                }
                
                browser.Display();
                if (browser.HasSelected()) {
                    strncpy(buffer, browser.GetSelected().string().c_str(), 256);
                    browser.ClearSelected();
                }
            };
            
            pathSelector("Diffuse", data->diffusePath, diffuseBrowser);
            pathSelector("Normal",  data->normalPath,  normalBrowser);
            pathSelector("ARM",     data->armPath,     armBrowser);
            pathSelector("Disp",    data->dispPath,    dispBrowser);
            
            if (ImGui::Button("Apply Custom Textures", ImVec2(-1, 25))) {
                extern void switchTerrainMaterial(int materialIndex);
                switchTerrainMaterial(-1);
            }
            ImGui::Unindent();
            ImGui::Spacing();
        }
        
        ImGui::Checkbox("Diffuse Map",                    &data->enableDiffuse);
        ImGui::Checkbox("Normal Map",                     &data->enableNormal);
        ImGui::Checkbox("ARM Map (AO/Roughness/Metallic)",&data->enableARM);
        ImGui::Checkbox("Displacement Map",               &data->enableDisplacement);
        ImGui::Checkbox("Stochastic Tiling (No Repeat)",  &data->enableStochastic);
        if (data->enableStochastic) {
            ImGui::Indent();
            ImGui::SliderFloat("Stoch. Contrast", &data->stochasticContrast, 1.0f, 32.0f, "%.1f");
            ImGui::SliderFloat("Stoch. Scale",    &data->stochasticScale,    0.1f, 10.0f, "%.2f");
            ImGui::Unindent();
        }
        
        ImGui::SliderFloat("Roughness", &data->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metalness", &data->metalness, 0.0f, 1.0f);
        ImGui::SliderFloat("UV Scale", &data->uvScale, 1.0f, 500.0f, "%.1f");
    }

    ImGui::Separator();
    if (ImGui::Button("Regenerate Terrain", ImVec2(-1, 30))) {
        extern void sg_RegenerateTerrain(SceneNode* node);
        sg_RegenerateTerrain(node);
    }
}
