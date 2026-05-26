#include "engine/utils/attrdesc.h"
#include "engine/dependancies/imgui/imgui.h"
#include <dirent.h>
#include <string.h>

extern void vcCloud_RegenerateSpheres(SceneNode* node);
extern void vcCloud_RegenerateWeatherMap(SceneNode* node);
extern void vcCloud_SaveWeatherMap(SceneNode* node, const char* path);
extern void vcCloud_SetRenderScale(SceneNode* node, float scale);
extern void vcCloud_SetTAA(SceneNode* node, bool enable, float blend);
extern bool vcCloud_LoadNVDF(SceneNode* node, const char* path);
extern bool vcCloud_LoadWeatherMap(SceneNode* node, const char* path);
extern void nvdfgen_SetPreviewSource(GLuint tex3d);

#define NVDF_TEXTURE_DIR    "engine/effects/clouds/nvdf_textures"
#define WEATHER_MAP_DIR     "engine/effects/clouds/weather_maps"

void ShowVolumetricCloudAttributes(SceneNode* node)
{
    if (!node || node->type != ENTITY_VOLUMETRIC_CLOUD) return;
    VolumetricCloudNodeData* c = &node->data.volumetricCloud;

    // ---- Appearance ----
    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SeparatorText("Color");
        ImGui::ColorEdit3("Top / Lit",    (float*)&c->cloudColorTop);
        ImGui::ColorEdit3("Bottom / Shadow", (float*)&c->cloudColorBottom);

        ImGui::SeparatorText("Shape");
        ImGui::SliderFloat("Cloud Type",   &c->cloudType,   0.0f,  1.0f,
                           "%.2f  (0=stratus  0.5=stratocumulus  1=cumulus)");
        ImGui::SliderFloat("Coverage",     &c->coverage,   -2.0f,  2.0f,
                           "%.2f  (neg=wispy, pos=filled)");
        ImGui::SliderFloat("Erosion",      &c->erosion,     0.1f,  1.0f,
                           "%.2f  (low=solid, high=wispy)");
        ImGui::SliderFloat("Noise Scale",  &c->noiseScale,  0.001f, 0.02f,
                           "%.4f  (smaller=larger cloud cells)");
        ImGui::SliderFloat("Detail Scale", &c->detailScale, 0.005f, 0.1f,
                           "%.4f  (higher=finer edge erosion)");
        
        ImGui::SeparatorText("Noise Layers");
        ImGui::Checkbox("Base Noise (PW presence)", &c->useBaseNoise);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ON: Perlin-Worley remap creates individual cloud cells (natural cellular look).\n"
                              "OFF: coverage / weather map IS the density — no cell grid, shape follows map exactly.");
        ImGui::Checkbox("Worley Erosion (GBA channels)", &c->useWorleyErosion);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ON: erodes the cloud surface with 3 octaves of Worley noise.\n"
                              "OFF: smoother, rounder cloud masses.");
        ImGui::Checkbox("Detail Noise (fine erosion)", &c->useDetailNoise);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ON: high-frequency detail texture adds wispy tendrils at cloud base.\n"
                              "OFF: cleaner silhouette, cheaper to render.");

        ImGui::SeparatorText("3D Noise Textures");
        ImGui::DragInt("Noise Resolution", &c->noiseRes, 1.0f, 16, 256,
                       "%d^3  (high = slow to generate)");

        ImGui::InputText("Base Path", c->noiseBaseTexPath, sizeof(c->noiseBaseTexPath));
        ImGui::InputText("Detail Path", c->noiseDetailTexPath, sizeof(c->noiseDetailTexPath));

        if (ImGui::Button("Regenerate & Save Noise Textures", ImVec2(-1, 0))) {

            extern void vcCloud_RegenerateNoise(SceneNode* node);
            vcCloud_RegenerateNoise(node);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bakes new 3D Perlin-Worley noise textures at the chosen resolution.\n"
                              "This is a slow CPU operation (100-500ms).");

        ImGui::SeparatorText("Animation");
        ImGui::DragFloat("Wind Speed",       &c->windSpeed,       0.01f, 0.0f, 10.0f,
                         "%.2f  global drift (base noise + NVDF)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Slides the entire cloud texture through the volume.\n"
                              "Affects base shape noise and NVDF texture drift.");
        ImGui::DragFloat("Local Noise Speed",&c->localNoiseSpeed, 0.01f, 0.0f, 10.0f,
                         "%.2f  detail / curl churn");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Animates only the detail and curl erosion noise.\n"
                              "Controls how fast the cloud surface churns and boils\n"
                              "without moving the whole cloud body.");

        ImGui::SeparatorText("Lighting");
        ImGui::SliderFloat("Absorption",   &c->absorption,  0.01f, 0.5f,
                           "%.3f  (thin→thick)");
        ImGui::SliderFloat("Silver Lining",&c->silverLining,0.0f,  3.0f);
    }

    // ---- Sun & Atmosphere ----
    if (ImGui::CollapsingHeader("Sun & Atmosphere")) {
        // Find if there is a sky atmosphere provider to show that it's driving these
        extern SceneNode* g_SceneRoot;
        auto findAtmo = [](auto& self, SceneNode* n) -> SceneNode* {
            if (!n) return nullptr;
            if (n->type == ENTITY_SKY_ATMOSPHERE) return n;
            for (int i = 0; i < n->num_children; i++) {
                SceneNode* f = self(self, n->children[i]);
                if (f) return f;
            }
            return nullptr;
        };
        SceneNode* atmoNode = findAtmo(findAtmo, g_SceneRoot);

        if (atmoNode) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Driven by Atmosphere (%s)", atmoNode->name);
            ImGui::BeginDisabled();
        }

        ImGui::ColorEdit3("Sun Color",     (float*)&c->sunColor);
        ImGui::DragFloat("Sun Intensity",  &c->sunIntensity,    0.1f, 0.0f, 100.0f);

        if (atmoNode) {
            ImGui::EndDisabled();
            ImGui::Separator();
        }

        ImGui::DragFloat("Ambient",        &c->ambientStrength, 0.01f,0.0f, 10.0f);
        ImGui::DragFloat("Scatter G",      &c->scatterG,        0.01f,-1.0f,1.0f,
                         "%.2f  (HG phase)");
    }

    // ---- Raymarching ----
    if (ImGui::CollapsingHeader("Raymarching")) {
        ImGui::DragFloat("Density Scale", &c->densityScale, 0.1f,  0.1f, 50.0f);
        ImGui::DragInt  ("Max Steps",     &c->maxSteps,     1.0f,  8,    256);
        ImGui::DragFloat("Step Size",     &c->stepSize,     0.01f, 0.05f, 5.0f);
        ImGui::DragFloat("Turbulence",    &c->turbulence,   0.01f, 0.0f, 4.0f);
    }

    // ---- Volume ----
    if (ImGui::CollapsingHeader("Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Sphere Field (skydome)", &c->useSphereField);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Wrap clouds around the viewer as a spherical shell.\n"
                              "Planet centre tracks camera XZ — clouds visible\n"
                              "all around the horizon with realistic curvature.\n"
                              "Overrides Box/Circle volume.");
        if (c->useSphereField) {
            // Quick-setup: sensible defaults for "atmosphere layer below skybox".
            // Sets planet radius and cloud base height relative to scene scale.
            if (ImGui::Button("Quick Setup (atmosphere layer)", ImVec2(-1, 0))) {
                c->planetRadius    = 6000.0f;  // large = nearly flat horizon
                c->cloudBaseHeight = 300.0f;   // Y where cloud base starts
                c->cloudThickness  = 400.0f;   // shell depth
                c->domeExtent      = 0.5f;     // only top half
                c->maxSteps        = 128;
                c->stepSize        = 1.5f;      // shader auto-scales for shell thickness
                c->coverage        = 0.5f;
                c->autoWeatherMap  = true;
                c->useWeatherMap   = true;
                vcCloud_RegenerateWeatherMap(node);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Sets planet radius=6000, base=300, thickness=400.\n"
                                  "Camera at Y=0 will see cloud layer overhead at Y~300.\n"
                                  "Adjust Planet Radius to change horizon curvature.");
            ImGui::SliderFloat("Planet Radius",     &c->planetRadius,    500.0f, 50000.0f,
                               "%.0f m  (bigger = flatter horizon)");
            ImGui::SliderFloat("Cloud Base Height", &c->cloudBaseHeight, 0.0f,   5000.0f,
                               "%.0f m  (world Y of shell bottom)");
            ImGui::SliderFloat("Cloud Thickness",   &c->cloudThickness,  10.0f,  5000.0f,
                               "%.0f m  (shell thickness)");
            ImGui::SliderFloat("Dome Extent",       &c->domeExtent,      0.0f,   1.0f,
                               "%.2f  (0.5 = hemisphere)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Control the vertical coverage of the spherical shell.\n"
                                  "0.5 = only the top half (above the horizon).\n"
                                  "1.0 = full sphere (includes clouds below terrain).");
            ImGui::TextDisabled("Shell Y range:  %.0f  –  %.0f",
                c->cloudBaseHeight, c->cloudBaseHeight + c->cloudThickness);
            ImGui::TextDisabled("(planet centre Y = cloudBase - planetRadius = %.0f)",
                c->cloudBaseHeight - c->planetRadius);
        } else {
            ImGui::DragFloat3("Box Size", (float*)&c->boxSize, 1.0f, 1.0f, 10000.0f);

            ImGui::Spacing();
            ImGui::Checkbox("Circle Field", &c->useCircleField);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Mask the cloud to a cylinder in XZ.\n"
                                  "Hides rectangular box edges on the horizon.");
            if (c->useCircleField) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat("##radius", &c->circleRadius, 1.0f, 10.0f, 10000.0f,
                                 "Radius %.0f m");
                if (c->circleRadius < 10.0f) c->circleRadius = 10.0f;
            }
        }

        ImGui::TextDisabled("Spheres: %d active (ignored in sphere field)",
                            c->sphereCount);
    }

    // ---- Sphere Grid ----
    if (ImGui::CollapsingHeader("Sphere Grid")) {
        bool changed = false;
        if (ImGui::DragInt("Grid X",      &c->gridX, 1.0f, 1, 20)) changed = true;
        if (ImGui::DragInt("Grid Z",      &c->gridZ, 1.0f, 1, 20)) changed = true;
        if (ImGui::DragFloat("Spacing",   &c->gridSpacing, 0.5f, 5.0f, 200.0f)) changed = true;
        if (ImGui::DragFloat("Scale",     &c->gridScale,   0.05f, 0.1f, 10.0f)) changed = true;
        if (ImGui::DragInt("Spheres Min", &c->spheresPerCloudMin, 1.0f, 1, 16)) changed = true;
        if (ImGui::DragInt("Spheres Max", &c->spheresPerCloudMax, 1.0f, 1, 32)) changed = true;
        ImGui::Spacing();
        if (ImGui::Button("Regenerate Spheres", ImVec2(-1, 0)) || changed)
            vcCloud_RegenerateSpheres(node);
    }

    // ---- Rendering ----
    if (ImGui::CollapsingHeader("Rendering")) {
        float scale = c->renderScale;
        if (ImGui::SliderFloat("Render Scale", &scale, 0.1f, 1.0f, "%.2f"))
            vcCloud_SetRenderScale(node, scale);
        ImGui::TextDisabled("Output: %dx%d   Groups: %dx%d",
            c->outputW, c->outputH, (c->outputW+7)/8, (c->outputH+7)/8);
    }

    // ---- Weather Map ----
    if (ImGui::CollapsingHeader("Weather Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        // --- Auto (procedural) mode -------------------------------------------
        if (ImGui::Checkbox("Auto (Noise-Based)", &c->autoWeatherMap)) {
            if (c->autoWeatherMap) {
                c->useWeatherMap = true;
                vcCloud_RegenerateWeatherMap(node);
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Generates a unique RGBA noise map that covers the\n"
                              "cloud volume exactly once (no tiling).\n"
                              "R=coverage  G=precipitation  B=cloud type  A=height scale\n"
                              "Fixes tiling and makes each sky region look different.");
        if (c->autoWeatherMap) {
            ImGui::SameLine();
            ImGui::TextDisabled(c->weatherMapTex ? "(id=%u)" : "(not generated)",
                                c->weatherMapTex);

            // ---- Pattern selector -------------------------------------------
            ImGui::Spacing();
            ImGui::SeparatorText("Generator");

            static const char* s_PatternNames[] = {
                "FBM Noise", "Spiral", "Cyclone", "Bands", "Cellular"
            };
            bool changed = false;
            if (ImGui::BeginCombo("Pattern", s_PatternNames[c->weatherGen.patternType])) {
                for (int i = 0; i < 5; i++) {
                    bool sel = (c->weatherGen.patternType == i);
                    if (ImGui::Selectable(s_PatternNames[i], sel)) {
                        c->weatherGen.patternType = i;
                        changed = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Global parameters shown for all patterns
            changed |= ImGui::SliderFloat("Coverage##wgen", &c->weatherGen.coverageScale,
                                          0.1f, 1.0f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Overall cloud density — scales pattern output before remapping.");

            // Coverage remapping
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
            changed |= ImGui::SliderFloat("Min##covMin", &c->weatherGen.coverageMin,
                                          0.0f, 0.9f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Lifts the dark end — raises minimum coverage across the map.");
            ImGui::SameLine();
            changed |= ImGui::SliderFloat("Max##covMax", &c->weatherGen.coverageMax,
                                          0.1f, 1.0f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Caps the bright end — limits maximum coverage density.");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::TextDisabled("Cov range");

            // ---- Per-pattern parameters -------------------------------------
            ImGui::Indent(8.0f);
            int pt = c->weatherGen.patternType;

            if (pt == 0) { // FBM Noise
                changed |= ImGui::SliderFloat("Frequency##wgen", &c->weatherGen.noiseFreq,
                                              0.2f, 4.0f, "%.2f");
            }
            else if (pt == 1 || pt == 2) { // Spiral / Cyclone
                changed |= ImGui::SliderFloat("Center X##wgen", &c->weatherGen.centerX,
                                              0.0f, 1.0f, "%.2f");
                changed |= ImGui::SliderFloat("Center Y##wgen", &c->weatherGen.centerY,
                                              0.0f, 1.0f, "%.2f");
                changed |= ImGui::SliderInt ("Arms##wgen",     &c->weatherGen.arms,
                                              1, 5);
                changed |= ImGui::SliderFloat("Tightness##wgen", &c->weatherGen.tightness,
                                              0.5f, 15.0f, "%.1f");
                changed |= ImGui::SliderFloat("Radius##wgen", &c->weatherGen.falloffRadius,
                                              0.05f, 0.9f, "%.2f");
                if (pt == 1)
                    ImGui::TextDisabled("Archimedean spiral — tightness = winding rate");
                else
                    ImGui::TextDisabled("Cyclone — dense eye wall + curving rain bands");
            }
            else if (pt == 3) { // Bands
                float deg = c->weatherGen.bandAngle * (180.0f / 3.14159265f);
                if (ImGui::SliderFloat("Angle##wgen", &deg, 0.0f, 180.0f, "%.0f deg")) {
                    c->weatherGen.bandAngle = deg * (3.14159265f / 180.0f);
                    changed = true;
                }
                changed |= ImGui::SliderFloat("Width##wgen",     &c->weatherGen.bandWidth,
                                              0.05f, 0.95f, "%.2f");
                changed |= ImGui::SliderFloat("Spacing##wgen",   &c->weatherGen.bandSpacing,
                                              0.03f, 0.5f,  "%.3f");
                changed |= ImGui::SliderFloat("Turbulence##wgen",&c->weatherGen.bandTurbulence,
                                              0.0f, 0.3f, "%.3f");
                ImGui::TextDisabled("Frontal cloud streets — angle rotates band direction");
            }
            else if (pt == 4) { // Cellular
                changed |= ImGui::SliderFloat("Cell Scale##wgen", &c->weatherGen.noiseFreq,
                                              0.3f, 6.0f, "%.2f");
                ImGui::TextDisabled("Scattered cumulus fields — larger scale = fewer cells");
            }
            ImGui::Unindent(8.0f);

            // ---- World placement -------------------------------------------
            ImGui::Spacing();
            ImGui::SeparatorText("World Placement");

            if (ImGui::Checkbox("Follow Camera##wgen", &c->weatherGen.followCamera))
                changed = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("ON: map centre tracks the camera (default).\n"
                                  "OFF: map is anchored at the fixed world XZ below.");

            if (!c->weatherGen.followCamera) {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
                if (ImGui::DragFloat("Anchor X##wgen", &c->weatherGen.worldAnchorX, 10.0f, -1e6f, 1e6f, "%.0f m"))
                    changed = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("World X coordinate that maps to the pattern centre (UV 0.5,0.5).");
                ImGui::SameLine();
                if (ImGui::DragFloat("Anchor Z##wgen", &c->weatherGen.worldAnchorZ, 10.0f, -1e6f, 1e6f, "%.0f m"))
                    changed = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("World Z coordinate that maps to the pattern centre (UV 0.5,0.5).");
                ImGui::PopItemWidth();
            }

            // ---- Coverage extent (fixes tiling at large planet radius) -------
            ImGui::SeparatorText("Coverage Extent");
            if (ImGui::DragFloat("Grid Extent##wgen", &c->weatherMapGridExtent, 100.0f, 0.0f, 1000000.0f, "%.0f m"))
                changed = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("World-space radius the weather map covers.\n"
                                  "0 = auto (uses planetRadius).\n"
                                  "Decrease to zoom in and eliminate repetition at large planet radius.\n"
                                  "e.g. 5000 m makes the pattern cover a 5 km circle instead of the full shell.");

            // ---- Texture resolution ----------------------------------------
            ImGui::SeparatorText("Texture Quality");
            static const char* s_ResNames[] = { "256 x 256 (fast)", "512 x 512", "1024 x 1024 (slow)" };
            int oldRes = c->weatherGen.texResolution;
            if (ImGui::BeginCombo("Resolution##wgen", s_ResNames[c->weatherGen.texResolution])) {
                for (int i = 0; i < 3; i++) {
                    bool sel = (c->weatherGen.texResolution == i);
                    if (ImGui::Selectable(s_ResNames[i], sel)) {
                        c->weatherGen.texResolution = i;
                        if (oldRes != i) changed = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Higher resolution preserves fine pattern detail.\n"
                                  "512 is a good balance; 1024 adds ~4× bake time.");

            // Auto-rebake on any parameter change
            if (changed) vcCloud_RegenerateWeatherMap(node);

            // ---- Texture preview -------------------------------------------
            if (c->weatherMapTex) {
                ImGui::Spacing();
                ImGui::SeparatorText("Preview  (R=coverage  G=precip  B=type  A=height)");
                float avail = ImGui::GetContentRegionAvail().x;
                float sz    = avail < 180.0f ? avail : 180.0f;
                ImGui::Image((ImTextureID)(uintptr_t)c->weatherMapTex, ImVec2(sz, sz));
            }

            // ---- Actions ---------------------------------------------------
            ImGui::Spacing();
            if (ImGui::Button("Regenerate##auto", ImVec2(110, 0)))
                vcCloud_RegenerateWeatherMap(node);
            ImGui::SameLine();
            if (ImGui::Button("Save PNG##auto", ImVec2(110, 0))) {
                static char s_SavePath[320] = "engine/effects/clouds/weather_maps/generated.png";
                ImGui::OpenPopup("SaveWeatherMap##popup");
            }
            if (ImGui::BeginPopup("SaveWeatherMap##popup")) {
                static char s_SavePath[320] = "engine/effects/clouds/weather_maps/generated.png";
                ImGui::SetNextItemWidth(320.0f);
                ImGui::InputText("Path##save", s_SavePath, sizeof(s_SavePath));
                if (ImGui::Button("Save##doSave")) {
                    vcCloud_SaveWeatherMap(node, s_SavePath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##save")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            if (c->useSphereField) {
                float ext = c->weatherMapGridExtent > 0.0f ? c->weatherMapGridExtent : c->planetRadius;
                ImGui::TextDisabled("Sphere mode — extent: %.0f m  anchor: %s",
                    ext,
                    c->weatherGen.followCamera ? "camera" : "fixed world XZ");
            } else {
                ImGui::TextDisabled("Box mode — extent = box XZ size");
            }
            ImGui::Spacing();
        }

        // --- Manual file mode -------------------------------------------------
        ImGui::Separator();
        ImGui::TextDisabled("Manual override (disables auto if loaded):");
        ImGui::Checkbox("Use Manual Map", &c->useWeatherMap);
        if (!c->autoWeatherMap) {
            ImGui::SameLine();
            ImGui::TextDisabled(c->weatherMapTex ? "(loaded id=%u)" : "(not loaded)",
                                c->weatherMapTex);
        }

        ImGui::InputText("Path##wm", c->weatherMapPath, sizeof(c->weatherMapPath));
        if (ImGui::Button("Reload##wm", ImVec2(120, 0)) && c->weatherMapPath[0]) {
            c->autoWeatherMap = false;
            vcCloud_LoadWeatherMap(node, c->weatherMapPath);
        }
        ImGui::SameLine();
        const char* prevWM = c->weatherMapPath[0] ? c->weatherMapPath
                                                  : "Browse weather_maps/...";
        if (ImGui::BeginCombo("##wmFiles", prevWM)) {
            DIR* dir = opendir(WEATHER_MAP_DIR);
            if (dir) {
                struct dirent* de;
                while ((de = readdir(dir)) != nullptr) {
                    const char* n = de->d_name;
                    if (n[0] == '.') continue;
                    size_t ln = strlen(n);
                    bool ok = (ln > 4) && (
                        !strcmp(n+ln-4, ".png") || !strcmp(n+ln-4, ".jpg") ||
                        !strcmp(n+ln-4, ".tga") || !strcmp(n+ln-4, ".bmp") ||
                        (ln > 5 && !strcmp(n+ln-5, ".jpeg")));
                    if (!ok) continue;
                    char full[320];
                    snprintf(full, sizeof(full), "%s/%s", WEATHER_MAP_DIR, n);
                    if (ImGui::Selectable(n, !strcmp(full, c->weatherMapPath))) {
                        c->autoWeatherMap = false;
                        vcCloud_LoadWeatherMap(node, full);
                    }
                }
                closedir(dir);
            } else {
                ImGui::TextDisabled("(cannot open %s)", WEATHER_MAP_DIR);
            }
            ImGui::EndCombo();
        }

        if (c->useWeatherMap && !c->autoWeatherMap) {
            ImGui::SliderFloat("Scale##wm", &c->weatherMapScale, 0.0001f, 0.05f,
                               "%.5f  (world units → UV, manual mode only)");
            ImGui::TextDisabled("R=coverage  G=precipitation  B=cloud type  A=height scale");
        }
    }

    // ---- Atmospheric Fog (aerial perspective) ----
    if (ImGui::CollapsingHeader("Atmospheric Fog")) {
        ImGui::ColorEdit3("Fog Color", (float*)&c->fogColor);
        ImGui::SliderFloat("Fog Density", &c->fogDensity, 0.0f, 0.01f,
                           "%.5f  (0 = off)");
        ImGui::SliderFloat("Fog Start",   &c->fogStart,   0.0f, 2000.0f,
                           "%.0f m");
    }

    // ---- NVDF (pre-baked density texture) ----
    if (ImGui::CollapsingHeader("NVDF Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use NVDF", &c->useNVDF);
        ImGui::SameLine();
        ImGui::TextDisabled(c->nvdfTex ? "(loaded id=%u)" : "(not loaded)", c->nvdfTex);

        ImGui::InputText("Path", c->nvdfPath, sizeof(c->nvdfPath));
        if (ImGui::Button("Reload", ImVec2(120, 0)) && c->nvdfPath[0])
            vcCloud_LoadNVDF(node, c->nvdfPath);
        ImGui::SameLine();
        if (ImGui::Button("Preview", ImVec2(80, 0)) && c->nvdfTex)
            nvdfgen_SetPreviewSource(c->nvdfTex);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Open the NVDF Generator with this texture loaded in the slice viewer.");
        ImGui::SameLine();

        // List files in nvdf_textures/ so the user can pick without typing.
        const char* preview = c->nvdfPath[0] ? c->nvdfPath : "Browse nvdf_textures/...";
        if (ImGui::BeginCombo("##nvdfFiles", preview)) {
            DIR* dir = opendir(NVDF_TEXTURE_DIR);
            if (dir) {
                struct dirent* de;
                while ((de = readdir(dir)) != nullptr) {
                    const char* n = de->d_name;
                    if (n[0] == '.') continue;
                    size_t ln = strlen(n);
                    if (ln < 5 || strcmp(n + ln - 5, ".nvdf") != 0) continue;
                    char full[320];
                    snprintf(full, sizeof(full), "%s/%s", NVDF_TEXTURE_DIR, n);
                    bool sel = (strcmp(full, c->nvdfPath) == 0);
                    if (ImGui::Selectable(n, sel))
                        vcCloud_LoadNVDF(node, full);
                }
                closedir(dir);
            } else {
                ImGui::TextDisabled("(cannot open %s)", NVDF_TEXTURE_DIR);
            }
            ImGui::EndCombo();
        }

        if (c->useNVDF) {
            // ── Texture scale ────────────────────────────────────────────────
            ImGui::SeparatorText("Scale & Position");

            // Show size as meters-per-tile (more intuitive than raw tile rate).
            // tileScale = 1/sizeM  →  sizeM = 1/tileScale
            float sizeM = (c->nvdfTileScale > 0.0f) ? (1.0f / c->nvdfTileScale) : 500.0f;
            if (ImGui::DragFloat("Texture Size (m)", &sizeM, 5.0f, 10.0f, 5000.0f, "%.0f m")) {
                if (sizeM > 0.0f) c->nvdfTileScale = 1.0f / sizeM;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "World-space meters one full texture tile covers.\n"
                    "Increase to zoom out (shape looks smaller).\n"
                    "Decrease to zoom in (shape looks larger).\n"
                    "Set to match your cloud volume XZ size for a 1:1 fit.");

            // ── Slide XYZ ───────────────────────────────────────────────────
            ImGui::DragFloat("Slide X",  &c->nvdfWorldOffset[0], 1.0f,
                             -9999.0f, 9999.0f, "%.1f m");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shift the texture along world X.");

            ImGui::DragFloat("Slide Y",  &c->nvdfYOffset, 0.005f, -1.0f, 1.0f, "%.3f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Shift which height-slice of the texture the cloud layer sees.\n"
                    "0 = cloud base maps to T=0, cloud top to T=1 (default).\n"
                    "+0.5 = sample from the middle–top half of the texture.\n"
                    "Use this to find your mesh shape in the texture vertically.");

            ImGui::DragFloat("Slide Z",  &c->nvdfWorldOffset[2], 1.0f,
                             -9999.0f, 9999.0f, "%.1f m");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Shift the texture along world Z.");

            // Quick-reset button
            if (ImGui::Button("Reset position##nvdfslide")) {
                c->nvdfWorldOffset = vec3(0.0f);
                c->nvdfYOffset     = 0.0f;
            }

            // ── Rotate XYZ ──────────────────────────────────────────────────
            ImGui::SeparatorText("Rotation");

            // Show angles in degrees for readability; store as radians.
            float degX = c->nvdfRotX * (180.0f / 3.14159265f);
            float degY = c->nvdfRotAngle * (180.0f / 3.14159265f);
            float degZ = c->nvdfRotZ * (180.0f / 3.14159265f);

            if (ImGui::DragFloat("Rotate X##nvdfrot", &degX, 0.5f, -180.0f, 180.0f, "%.1f°"))
                c->nvdfRotX = degX * (3.14159265f / 180.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Tilt the texture around the X axis (tips the top/bottom).");

            if (ImGui::DragFloat("Rotate Y##nvdfrot", &degY, 0.5f, -180.0f, 180.0f, "%.1f°"))
                c->nvdfRotAngle = degY * (3.14159265f / 180.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Spin the texture around the vertical Y axis.");

            if (ImGui::DragFloat("Rotate Z##nvdfrot", &degZ, 0.5f, -180.0f, 180.0f, "%.1f°"))
                c->nvdfRotZ = degZ * (3.14159265f / 180.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Tilt the texture around the Z axis (tips left/right).");

            if (ImGui::Button("Reset rotation##nvdfrot")) {
                c->nvdfRotX = c->nvdfRotAngle = c->nvdfRotZ = 0.0f;
            }

            // ── Anti-repetition (advanced) ───────────────────────────────────
            if (ImGui::TreeNode("Advanced (anti-repetition)")) {
                ImGui::SliderFloat("HW Mod Strength", &c->hwModStrength, 0.0f, 1.0f,
                                   "%.2f  height + warp variation");
                ImGui::SliderFloat("HW Mod Scale",    &c->hwModScale,    0.0001f, 0.02f,
                                   "%.5f  low-freq field rate");
                ImGui::SliderFloat("Curl Erosion",    &c->curlStrength,  0.0f, 1.0f,
                                   "%.2f  high-freq curly-alligator");
                ImGui::TreePop();
            }
        }
    }

    // ---- Occlusion ----
    if (ImGui::CollapsingHeader("Occlusion")) {
        ImGui::Checkbox("Scene Depth Culling", &c->useSceneDepth);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sample the scene depth buffer to skip cloud pixels\n"
                              "fully occluded by terrain or meshes.\n"
                              "Major perf win when looking at geometry-rich horizons.");
    }

    // ---- Performance / Raymarching ----
    if (ImGui::CollapsingHeader("Adaptive Raymarching")) {
        ImGui::SliderFloat("Adaptive Factor",  &c->adaptiveFactor,   0.0f, 0.04f,
                           "%.4f  (0=fixed step, >0 grows with distance)");
        ImGui::SliderFloat("Jitter Switch Dist",&c->jitterSwitchDist,0.0f,1000.0f,
                           "%.0f m  (animated near, static jitter beyond)");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Checkbox("Dual Pass (near / far)", &c->useDualPass);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Near pass (<splitDist): low-res via renderScale, expensive.\n"
                "Far pass (>splitDist): 960x540, cheap via adaptive step.");
        if (c->useDualPass) {
            ImGui::SliderFloat("Split Distance", &c->nearFarSplit, 50.0f, 1000.0f,
                               "%.0f m");
            ImGui::DragInt("Far W", &c->farOutputW, 1, 64, 2048);
            ImGui::SameLine();
            ImGui::DragInt("Far H", &c->farOutputH, 1, 32, 1024);
            ImGui::TextDisabled("Near: %dx%d (set via Render Scale above)",
                                c->outputW, c->outputH);
            if (c->farOutputTex)
                ImGui::TextDisabled("Far tex id=%u", c->farOutputTex);
            if (ImGui::Button("Rebuild Far Texture##dual")) {
                if (c->farOutputTex)  { glDeleteTextures(1, &c->farOutputTex);  c->farOutputTex  = 0; }
                if (c->farHistoryTex) { glDeleteTextures(1, &c->farHistoryTex); c->farHistoryTex = 0; }
                c->farFrameIndex = 0;
            }
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Per-entity NVDF randomization");
        if (ImGui::Button("Re-randomize##nvdfrand")) {
            uintptr_t seed = (uintptr_t)(void*)node ^ (uintptr_t)time(nullptr);
            seed ^= seed >> 33;  seed *= 0xff51afd7ed558ccdULL;
            auto rF = [](uintptr_t s, int n) {
                s ^= (uintptr_t)n * 0x9e3779b97f4a7c15ULL;
                s ^= s >> 17; s *= 0xbf58476d1ce4e5b9ULL;
                return (float)(s & 0xffffffu) * (1.f / (float)0x1000000u);
            };
            c->nvdfRotAngle    = rF(seed, 0) * 6.28318530f;
            c->nvdfWorldOffset = vec3(rF(seed,1)*1200.f-600.f, 0.f, rF(seed,2)*1200.f-600.f);
        }
    }

    // ---- Temporal AA ----
    if (ImGui::CollapsingHeader("Temporal AA (TAA)")) {
        bool taaOn = c->enableTAA;
        if (taaOn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f,0.55f,0.15f,1.0f));
        else       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f,0.15f,0.15f,1.0f));
        if (ImGui::Button(taaOn ? "TAA: ON  (click to disable)"
                                : "TAA: OFF (click to enable)", ImVec2(-1,0)))
            vcCloud_SetTAA(node, !taaOn, c->taaBlend);
        ImGui::PopStyleColor();
        if (c->enableTAA) {
            float blend = c->taaBlend;
            if (ImGui::SliderFloat("Blend", &blend, 0.01f, 1.0f,
                                   "%.2f  (low=smooth, high=sharp)"))
                c->taaBlend = blend;
            ImGui::TextDisabled("Frame %d — ~4x fewer rays vs no TAA", c->frameIndex);
        }
    }
}
