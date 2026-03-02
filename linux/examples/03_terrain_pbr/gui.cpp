
#define PROJECT_03  // already set in project.cpp; here for standalone safety

// gui.cpp — Project 03 ImGui panels.
// Included from examples/03_terrain_pbr/project.cpp after all subsystem headers.

#include "engine/dependancies/imgui/imgui.h"
#include "engine/dependancies/imgui/imgui_impl_glfw.h"
#include "engine/dependancies/imgui/imgui_impl_opengl3.h"
#include "user/terrain/terrain.h"
#include "engine/effects/perlin/perlin.h"

// Forward declarations — these are defined later in the translation unit
void toggleWireframe(void);
void NewFrameGUI();

static void RenderMainDebugControls()
{
    ImGui::Begin("Debug Controls");
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();

    ImGui::Text("Camera Mode");
    ImGui::RadioButton("FPS",    &cameraMode, 0); ImGui::SameLine();
    ImGui::RadioButton("Manual", &cameraMode, 1); ImGui::SameLine();
    ImGui::RadioButton("Orbit",  &cameraMode, 2);
    ImGui::Separator();

    if (cameraMode == 0) {
        ImGui::Text("FPS Camera Position");
        ImGui::SliderFloat("Cam X", &camera_pos[0], -100.0f, 100.0f);
        ImGui::SliderFloat("Cam Y", &camera_pos[1], -100.0f, 100.0f);
        ImGui::SliderFloat("Cam Z", &camera_pos[2], -500.0f, 500.0f);
    } else if (cameraMode == 1) {
        ImGui::Text("Manual Camera Control");
        ImGui::SliderFloat("Eye X", &camera_pos[0], -100.0f, 100.0f);
        ImGui::SliderFloat("Eye Y", &camera_pos[1], -100.0f, 100.0f);
        ImGui::SliderFloat("Eye Z", &camera_pos[2], -500.0f, 500.0f);
        ImGui::Separator();
        ImGui::Text("Center Position");
        ImGui::SliderFloat("Center X", &camCenter[0], -100.0f, 100.0f);
        ImGui::SliderFloat("Center Y", &camCenter[1], -100.0f, 100.0f);
        ImGui::SliderFloat("Center Z", &camCenter[2], -500.0f, 500.0f);
        ImGui::Separator();
        ImGui::Text("Up Vector");
        bool upX = (camUp[0] > 0.5f), upY = (camUp[1] > 0.5f), upZ = (camUp[2] > 0.5f);
        if (ImGui::Checkbox("Up X", &upX)) camUp[0] = upX ? 1.0f : 0.0f;
        if (ImGui::Checkbox("Up Y", &upY)) camUp[1] = upY ? 1.0f : 0.0f;
        if (ImGui::Checkbox("Up Z", &upZ)) camUp[2] = upZ ? 1.0f : 0.0f;
    } else if (cameraMode == 2) {
        ImGui::Text("Orbit Camera Control");
        ImGui::SliderFloat("Radius",   &orbitRadius,    1.0f, 500.0f);
        ImGui::SliderFloat("Center X", &camCenter[0], -100.0f, 100.0f);
        ImGui::SliderFloat("Center Y", &camCenter[1], -100.0f, 100.0f);
        ImGui::SliderFloat("Center Z", &camCenter[2], -500.0f, 500.0f);
        ImGui::Checkbox("Keyboard Control", &orbitKeyboardControl);
        ImGui::Checkbox("Show Visuals",     &showOrbitVisuals);
        if (!orbitKeyboardControl)
            ImGui::TextColored(ImVec4(1,1,0,1), "Right Click & Drag to Rotate");
        else
            ImGui::TextColored(ImVec4(0,1,0,1), "Use Arrow Keys to Rotate");
    }

    ImGui::Separator();
    if (ImGui::Button("Toggle Wireframe")) toggleWireframe();
    ImGui::End();
}

static void RenderPBRControls()
{
    ImGui::Begin("PBR Debug Controls");

    if (ImGui::CollapsingHeader("Light Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Light Pos",   &lightPos[0],   0.1f);
        ImGui::ColorEdit3("Light Color", &lightColor[0]);
        ImGui::DragFloat("Intensity",    &lightIntensity, 0.1f, 0.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("IBL Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use IBL", &useIBL);
        ImGui::Separator();
        ImGui::Text("Skybox / Environment");

        static int selectedPreset = currentSkyboxPreset;
        if (selectedPreset != currentSkyboxPreset) selectedPreset = currentSkyboxPreset;

        if (ImGui::BeginCombo("Mood", skyboxPresets[selectedPreset].name)) {
            for (int i = 0; i < SKYBOX_PRESET_COUNT; i++) {
                bool isSelected = (selectedPreset == i);
                if (ImGui::Selectable(skyboxPresets[i].name, isSelected)) {
                    selectedPreset = i;
                    if (selectedPreset != currentSkyboxPreset) reloadSkyboxPreset(selectedPreset);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImVec4 green(0.2f,0.8f,0.2f,1.0f), red(0.8f,0.2f,0.2f,1.0f);
        ImGui::TextColored(envCubemap    ? green : red, "EnvCubemap: %s (ID: %u)", envCubemap    ? "YES":"NO", envCubemap);
        ImGui::TextColored(irradianceMap ? green : red, "Irradiance: %s (ID: %u)", irradianceMap ? "YES":"NO", irradianceMap);
        ImGui::TextColored(prefilterMap  ? green : red, "Prefilter:  %s (ID: %u)", prefilterMap  ? "YES":"NO", prefilterMap);
        ImGui::TextColored(brdfLUTTexture? green : red, "BRDF LUT:   %s (ID: %u)", brdfLUTTexture? "YES":"NO", brdfLUTTexture);
    }

    if (ImGui::CollapsingHeader("Model Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &modelPosition[0], 0.1f);
        ImGui::DragFloat3("Rotation", &modelRotation[0], 1.0f);
        ImGui::DragFloat3("Scale",    &modelScale[0],    0.1f);
    }

    if (ImGui::CollapsingHeader("Texture Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (helmetMeshes && helmetMeshCount > 0) {
            Material& mat = helmetMeshes[0].material;
            ImGui::Text("Loaded Textures:"); ImGui::Separator();
            ImVec4 tc = mat.diffuseTexture ? ImVec4(0.2f,0.8f,0.2f,1.0f) : ImVec4(0.8f,0.2f,0.2f,1.0f);
            ImGui::TextColored(tc, "Diffuse: %s (ID: %u)", mat.diffuseTexture?"YES":"NO", mat.diffuseTexture);
            ImGui::SameLine(); ImGui::Checkbox("##DD", &debugDisableDiffuse);
            tc = mat.normalTexture ? ImVec4(0.2f,0.8f,0.2f,1.0f) : ImVec4(0.8f,0.2f,0.2f,1.0f);
            ImGui::TextColored(tc, "Normal:  %s (ID: %u)", mat.normalTexture?"YES":"NO", mat.normalTexture);
            ImGui::SameLine(); ImGui::Checkbox("##DN", &debugDisableNormal);
            tc = mat.metallicRoughnessTexture ? ImVec4(0.2f,0.8f,0.2f,1.0f) : ImVec4(0.8f,0.2f,0.2f,1.0f);
            ImGui::TextColored(tc, "ORM:     %s (ID: %u)", mat.metallicRoughnessTexture?"YES":"NO", mat.metallicRoughnessTexture);
            ImGui::SameLine(); ImGui::Checkbox("##DM", &debugDisableMetallic);
            ImGui::SameLine(); ImGui::Checkbox("##DR", &debugDisableRoughness);
            ImGui::SameLine(); ImGui::Checkbox("##DA", &debugDisableAO);
            tc = mat.emissiveTexture ? ImVec4(0.2f,0.8f,0.2f,1.0f) : ImVec4(0.8f,0.2f,0.2f,1.0f);
            ImGui::TextColored(tc, "Emissive:%s (ID: %u)", mat.emissiveTexture?"YES":"NO", mat.emissiveTexture);
            ImGui::SameLine(); ImGui::Checkbox("##DE", &debugDisableEmissive);
        } else {
            ImGui::TextColored(ImVec4(1.0f,0.5f,0.0f,1.0f), "No meshes loaded!");
        }
    }

    if (ImGui::CollapsingHeader("PBR Overrides")) {
        ImGui::Checkbox("Override Roughness", &debugOverrideRoughness);
        if (debugOverrideRoughness) ImGui::SliderFloat("Roughness", &debugRoughness, 0.0f, 1.0f);
        ImGui::Checkbox("Override Metallic",  &debugOverrideMetallic);
        if (debugOverrideMetallic)  ImGui::SliderFloat("Metallic",  &debugMetallic,  0.0f, 1.0f);
        ImGui::Separator();
        ImGui::SliderFloat("AO Strength",        &debugAOStrength,        0.0f, 2.0f);
        ImGui::SliderFloat("Emissive Intensity", &debugEmissiveIntensity, 0.0f, 20.0f);
    }

    if (ImGui::CollapsingHeader("Material Info")) {
        if (helmetMeshes && helmetMeshCount > 0) {
            Material& mat = helmetMeshes[0].material;
            ImGui::Text("Diffuse Color: (%.2f, %.2f, %.2f)", mat.diffuseColor[0], mat.diffuseColor[1], mat.diffuseColor[2]);
            ImGui::Text("Shininess: %.2f | Opacity: %.2f | Emissive: %s", mat.shininess, mat.opacity, mat.isEmissive?"Yes":"No");
            ImGui::Text("Mesh Count: %d", helmetMeshCount);
        }
    }

    ImGui::End();
}

// ============================================================================
// NOISE GENERATOR WINDOW
// ============================================================================

extern int   g_terrainOctaves;
extern float g_terrainPersistence;
extern float g_terrainLacunarity;
extern float g_mountainThreshold;
extern float g_desertThreshold;
extern float g_mountainHeightScale;
extern float g_desertHeightScale;
extern float g_plainsHeightScale;
extern bool  g_useIslandMask;
extern int   g_biomeMode;
extern float g_ridgeStrength;
extern float g_turbulence;
extern int   g_terraceLevels;
extern float g_powerCurve;
extern float g_heightOffset;
extern float g_islandFalloff;
extern int   g_perlinSeed;
extern GLuint HeightMap;

static void RenderNoiseGeneratorWindow()
{
    static bool  showWindow     = true;
    static int   textureWidth   = 512;
    static int   textureHeight  = 512;
    static float noiseFrequency = 0.01f;
    static float heightScale    = 10.0f;
    static bool  showHeightmap  = true;
    static bool  showNormalMap  = false;
    static GLuint heightmapTexture = 0;
    static GLuint normalMapTexture = 0;

    if (!showWindow) return;
    ImGui::Begin("Generate Noise", &showWindow);

    if (ImGui::CollapsingHeader("Seed", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Seed Value", &g_perlinSeed);
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) g_perlinSeed = rand();
    }

    if (ImGui::CollapsingHeader("fBm Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Octaves",     &g_terrainOctaves,     1, 1);
        if (g_terrainOctaves < 1) g_terrainOctaves = 1;
        if (g_terrainOctaves > 12) g_terrainOctaves = 12;
        ImGui::InputFloat("Persistence", &g_terrainPersistence, 0.05f, 0.1f, "%.3f");
        if (g_terrainPersistence < 0.01f) g_terrainPersistence = 0.01f;
        if (g_terrainPersistence > 1.0f)  g_terrainPersistence = 1.0f;
        ImGui::InputFloat("Lacunarity",  &g_terrainLacunarity,  0.1f, 0.5f, "%.2f");
        if (g_terrainLacunarity < 1.0f) g_terrainLacunarity = 1.0f;
        if (g_terrainLacunarity > 4.0f) g_terrainLacunarity = 4.0f;
    }

    if (ImGui::CollapsingHeader("Terrain Modifiers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat("Ridge Strength", &g_ridgeStrength, 0.1f, 0.25f, "%.2f");
        if (g_ridgeStrength < 0.0f) g_ridgeStrength = 0.0f;
        if (g_ridgeStrength > 1.0f) g_ridgeStrength = 1.0f;
        ImGui::InputFloat("Turbulence",    &g_turbulence,    0.5f, 1.0f, "%.2f");
        if (g_turbulence < 0.0f) g_turbulence = 0.0f;
        ImGui::InputInt("Terrace Levels",  &g_terraceLevels, 1, 5);
        if (g_terraceLevels < 0)  g_terraceLevels = 0;
        if (g_terraceLevels > 50) g_terraceLevels = 50;
        ImGui::InputFloat("Power Curve",   &g_powerCurve,    0.1f, 0.25f, "%.2f");
        if (g_powerCurve < 0.1f) g_powerCurve = 0.1f;
        if (g_powerCurve > 4.0f) g_powerCurve = 4.0f;
        ImGui::InputFloat("Height Offset", &g_heightOffset,  1.0f, 5.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Biome Settings")) {
        const char* biomeModes[] = { "Auto (Noise-based)", "Mountains Only", "Desert Only", "Plains Only" };
        ImGui::Combo("Biome Mode", &g_biomeMode, biomeModes, 4);
        if (g_biomeMode == 0) {
            ImGui::Separator(); ImGui::Text("Biome Thresholds:");
            ImGui::InputFloat("Mountain Threshold", &g_mountainThreshold, 0.05f, 0.1f, "%.2f");
            ImGui::InputFloat("Desert Threshold",   &g_desertThreshold,   0.05f, 0.1f, "%.2f");
        }
        ImGui::Separator(); ImGui::Text("Height Scales:");
        ImGui::InputFloat("Mountain Height", &g_mountainHeightScale, 5.0f, 10.0f, "%.1f");
        ImGui::InputFloat("Desert Height",   &g_desertHeightScale,   1.0f, 5.0f,  "%.1f");
        ImGui::InputFloat("Plains Height",   &g_plainsHeightScale,   2.0f, 5.0f,  "%.1f");
        ImGui::Separator();
        ImGui::Checkbox("Island Mask", &g_useIslandMask);
        if (g_useIslandMask) {
            ImGui::InputFloat("Island Falloff", &g_islandFalloff, 0.1f, 0.5f, "%.2f");
            if (g_islandFalloff < 0.1f) g_islandFalloff = 0.1f;
        }
    }

    if (ImGui::CollapsingHeader("Texture Generation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Width##tex",  &textureWidth,  64, 128);
        if (textureWidth  < 32)   textureWidth  = 32;
        if (textureWidth  > 4096) textureWidth  = 4096;
        ImGui::InputInt("Height##tex", &textureHeight, 64, 128);
        if (textureHeight < 32)   textureHeight = 32;
        if (textureHeight > 4096) textureHeight = 4096;
        ImGui::InputFloat("Frequency", &noiseFrequency, 0.001f, 0.01f, "%.4f");
        if (noiseFrequency < 0.0001f) noiseFrequency = 0.0001f;

        static bool autoApply = true;
        ImGui::Checkbox("Auto-apply to Terrain", &autoApply);
        ImGui::Separator();

        auto doGenerate = [&]() {
            if (heightmapTexture) { glDeleteTextures(1, &heightmapTexture); heightmapTexture = 0; }
            if (normalMapTexture) { glDeleteTextures(1, &normalMapTexture); normalMapTexture = 0; }
            heightmapTexture = createHeightMapTexture(textureWidth, textureHeight, noiseFrequency, heightScale);
            normalMapTexture = createNormalMapTexture(textureWidth, textureHeight, noiseFrequency, heightScale);
            if (autoApply && heightmapTexture) {
                if (HeightMap && HeightMap != heightmapTexture) glDeleteTextures(1, &HeightMap);
                HeightMap = heightmapTexture;
            }
        };

        if (ImGui::Button("Generate Textures", ImVec2(200, 30))) doGenerate();
        ImGui::SameLine();
        if (ImGui::Button("Quick Random")) { g_perlinSeed = rand(); doGenerate(); }

        if (!autoApply && heightmapTexture) {
            if (ImGui::Button("Apply to Terrain", ImVec2(200, 0))) {
                if (HeightMap && HeightMap != heightmapTexture) glDeleteTextures(1, &HeightMap);
                HeightMap = heightmapTexture;
            }
        }
    }

    if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Heightmap", &showHeightmap); ImGui::SameLine();
        ImGui::Checkbox("Normal Map", &showNormalMap);
        static float previewSize = 256.0f;
        ImGui::InputFloat("Preview Size", &previewSize, 32.0f, 64.0f, "%.0f");
        if (previewSize < 64.0f) previewSize = 64.0f;
        if (previewSize > 1024.0f) previewSize = 1024.0f;
        if (showHeightmap && heightmapTexture)
            ImGui::Image((void*)(intptr_t)heightmapTexture, ImVec2(previewSize, previewSize), ImVec2(0,1), ImVec2(1,0));
        if (showNormalMap && normalMapTexture)
            ImGui::Image((void*)(intptr_t)normalMapTexture, ImVec2(previewSize, previewSize), ImVec2(0,1), ImVec2(1,0));
        if (!heightmapTexture && !normalMapTexture)
            ImGui::TextColored(ImVec4(1.0f,0.6f,0.0f,1.0f), "Click 'Generate Textures' to preview.");
    }

    if (ImGui::CollapsingHeader("Export to PNG")) {
        static char exportPath[256] = "heightmap.png";
        ImGui::InputText("Filename##export", exportPath, 256);
        if (ImGui::Button("Export Heightmap", ImVec2(150,0))) {
            if (exportHeightmapToPNG(exportPath, textureWidth, textureHeight, noiseFrequency))
                ImGui::OpenPopup("Export OK"); else ImGui::OpenPopup("Export FAIL");
        }
        ImGui::SameLine();
        if (ImGui::Button("Export Normal Map", ImVec2(150,0))) {
            static char npath[256];
            snprintf(npath, 256, "normal_%s", exportPath);
            if (exportNormalmapToPNG(npath, textureWidth, textureHeight, noiseFrequency))
                ImGui::OpenPopup("Export OK"); else ImGui::OpenPopup("Export FAIL");
        }
        if (ImGui::BeginPopupModal("Export OK",   NULL, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::Text("Export successful!"); if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
        if (ImGui::BeginPopupModal("Export FAIL", NULL, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::Text("Export failed!");     if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
    }

    if (ImGui::CollapsingHeader("Status")) {
        ImVec4 green(0.2f,0.8f,0.2f,1.0f), red(0.8f,0.2f,0.2f,1.0f);
        ImGui::TextColored(heightmapTexture ? green : red, "Heightmap: %s", heightmapTexture ? "READY":"NONE");
        ImGui::TextColored(normalMapTexture ? green : red, "Normal Map: %s", normalMapTexture ? "READY":"NONE");
        ImGui::Separator();
        ImGui::Text("Seed: %d | Res: %dx%d | Freq: %.4f", g_perlinSeed, textureWidth, textureHeight, noiseFrequency);
    }

    ImGui::End();
}

// ============================================================================
// TERRAIN MESH WINDOW
// ============================================================================

extern int   g_terrainMeshWidth;
extern int   g_terrainMeshDepth;
extern float g_terrainScale;
extern int   g_tessellationInner;
extern int   g_tessellationOuter;
extern float g_displacementScale;
extern bool  g_wireframeMode;
extern int   g_lodBias;
extern int   g_heightmapSource;
extern GLuint g_loadedHeightmapTexture;
extern char  g_heightmapFilePath[256];

static void RenderTerrainMeshWindow()
{
    static bool showWindow = true;
    if (!showWindow) return;
    ImGui::Begin("Terrain Mesh", &showWindow);

    if (ImGui::CollapsingHeader("Heightmap Source", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* sources[] = { "Generated (from Noise)", "Load from File" };
        ImGui::Combo("Source", &g_heightmapSource, sources, 2);
        if (g_heightmapSource == 0) {
            ImGui::TextColored(ImVec4(0.5f,0.8f,0.5f,1.0f), "Using heightmap from 'Generate Noise' window");
        } else {
            ImGui::InputText("File Path", g_heightmapFilePath, 256);
            if (ImGui::Button("Load Heightmap", ImVec2(150,0))) {
                if (g_loadedHeightmapTexture) glDeleteTextures(1, &g_loadedHeightmapTexture);
                int w, h, c;
                unsigned char* data = stbi_load(g_heightmapFilePath, &w, &h, &c, 4);
                if (data) {
                    glGenTextures(1, &g_loadedHeightmapTexture);
                    glBindTexture(GL_TEXTURE_2D, g_loadedHeightmapTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    stbi_image_free(data);
                    if (HeightMap && HeightMap != g_loadedHeightmapTexture) glDeleteTextures(1, &HeightMap);
                    HeightMap = g_loadedHeightmapTexture;
                    ImGui::OpenPopup("Load OK");
                } else {
                    ImGui::OpenPopup("Load FAIL");
                }
            }
            if (g_loadedHeightmapTexture) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.2f,0.8f,0.2f,1.0f), "Loaded!"); }
            if (ImGui::BeginPopupModal("Load OK",   NULL, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::Text("Loaded!"); if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
            if (ImGui::BeginPopupModal("Load FAIL", NULL, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::Text("Failed!"); if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
        }
    }

    if (ImGui::CollapsingHeader("Mesh Resolution", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Width", &g_terrainMeshWidth, 16, 64);
        if (g_terrainMeshWidth < 16)   g_terrainMeshWidth = 16;
        if (g_terrainMeshWidth > 1024) g_terrainMeshWidth = 1024;
        ImGui::InputInt("Depth", &g_terrainMeshDepth, 16, 64);
        if (g_terrainMeshDepth < 16)   g_terrainMeshDepth = 16;
        if (g_terrainMeshDepth > 1024) g_terrainMeshDepth = 1024;
        ImGui::InputFloat("World Scale", &g_terrainScale, 1.0f, 5.0f, "%.1f");
        if (g_terrainScale < 0.1f) g_terrainScale = 0.1f;
        ImGui::Text("Total vertices: %d", g_terrainMeshWidth * g_terrainMeshDepth);
    }

    if (ImGui::CollapsingHeader("LOD / Tessellation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Inner Tess Level", &g_tessellationInner, 1, 2);
        if (g_tessellationInner < 1)  g_tessellationInner = 1;
        if (g_tessellationInner > 64) g_tessellationInner = 64;
        ImGui::InputInt("Outer Tess Level", &g_tessellationOuter, 1, 2);
        if (g_tessellationOuter < 1)  g_tessellationOuter = 1;
        if (g_tessellationOuter > 64) g_tessellationOuter = 64;
        ImGui::InputFloat("Displacement Scale", &g_displacementScale, 5.0f, 10.0f, "%.1f");
        ImGui::InputInt("LOD Bias", &g_lodBias, 1, 2);
        if (g_lodBias < -5) g_lodBias = -5;
        if (g_lodBias > 5)  g_lodBias = 5;
    }

    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::Checkbox("Wireframe Mode", &g_wireframeMode);
    }

    ImGui::Separator();
    if (ImGui::Button("Regenerate Mesh", ImVec2(200,30))) {
        regenerateTerrainMesh();
        ImGui::OpenPopup("Mesh Regen");
    }
    if (ImGui::BeginPopupModal("Mesh Regen", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Mesh regenerated."); if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup();
    }

    ImGui::End();
}

// ============================================================================
// TERRAIN PBR SETTINGS WINDOW
// ============================================================================

extern bool  g_enableTerrainDiffuse;
extern bool  g_enableTerrainNormalMap;
extern bool  g_enableTerrainARM;
extern bool  g_enableTerrainDisplacement;
extern float g_terrainUVScale;
extern int   g_terrainMaterialIndex;
extern int   g_terrainMaterialCount;
struct TerrainMaterialDef;
extern TerrainMaterialDef g_terrainMaterials[];

static void RenderTerrainPBRWindow()
{
    static bool showWindow = true;
    if (!showWindow) return;
    ImGui::Begin("Terrain PBR Settings", &showWindow);

    if (ImGui::CollapsingHeader("Material Preset", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int selectedMaterial = 0;
        const char* materialNames[] = { "Aerial Beach", "Gray Rocks" };
        if (ImGui::Combo("Material", &selectedMaterial, materialNames, g_terrainMaterialCount)) {
            if (selectedMaterial != g_terrainMaterialIndex) switchTerrainMaterial(selectedMaterial);
        }
    }

    if (ImGui::CollapsingHeader("Texture Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Diffuse Map",                    &g_enableTerrainDiffuse);
        ImGui::Checkbox("Normal Map",                     &g_enableTerrainNormalMap);
        ImGui::Checkbox("ARM Map (AO/Roughness/Metallic)",&g_enableTerrainARM);
        ImGui::Checkbox("Displacement Map",               &g_enableTerrainDisplacement);
    }

    if (ImGui::CollapsingHeader("UV Tiling", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("UV Scale", &g_terrainUVScale, 1.0f, 500.0f, "%.1f");
    }

    ImGui::End();
}

// ============================================================================
// UpdateGUI — called by glfwmain.cpp every frame
// ============================================================================

void UpdateGUI()
{
    NewFrameGUI(); // start ImGui frame
    RenderMainDebugControls();
    RenderPBRControls();
    RenderNoiseGeneratorWindow();
    RenderTerrainMeshWindow();
    RenderTerrainPBRWindow();
}
