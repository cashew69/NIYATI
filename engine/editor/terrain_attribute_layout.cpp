#include "engine/core/gl/structs.h"
#include "engine/dependancies/imgui/imgui.h"
#include <stdio.h>

void ShowTerrainAttributes(SceneNode* node) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    TerrainNodeData* data = &node->data.terrain;

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
        const char* materialNames[] = { "Aerial Beach", "Gray Rocks" };
        ImGui::Combo("Material", &data->materialIndex, materialNames, 2);
        
        ImGui::Checkbox("Diffuse Map",                    &data->enableDiffuse);
        ImGui::Checkbox("Normal Map",                     &data->enableNormal);
        ImGui::Checkbox("ARM Map (AO/Roughness/Metallic)",&data->enableARM);
        ImGui::Checkbox("Displacement Map",               &data->enableDisplacement);
        
        ImGui::SliderFloat("UV Scale", &data->uvScale, 1.0f, 500.0f, "%.1f");
    }

    ImGui::Separator();
    if (ImGui::Button("Regenerate Terrain", ImVec2(-1, 30))) {
        extern void sg_RegenerateTerrain(SceneNode* node);
        sg_RegenerateTerrain(node);
    }
}
