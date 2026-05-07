#include "engine/utils/attrdesc.h"
#include "engine/dependancies/imgui/imfilebrowser.h"
#include <vector>

static void CollectTerrainNodes(SceneNode* node, std::vector<SceneNode*>& terrains) {
    if (!node) return;
    if (node->type == ENTITY_TERRAIN) {
        terrains.push_back(node);
    }
    for (int i = 0; i < node->num_children; i++) {
        CollectTerrainNodes(node->children[i], terrains);
    }
}

// Forward declarations
void ShowModelAttributes(Model* model);
void ShowSceneNodeAttributes(SceneNode* node);
void ShowTerrainAttributes(SceneNode* node);

#include "terrain_attribute_layout.cpp"
#include "skybox_attribute_layout.cpp"


// File-scope browser for instance model selection
static ImGui::FileBrowser s_InstanceModelBrowser(
    ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter);
static bool s_InstanceBrowserInited = false;
static InstanceData* s_InstanceBrowserTarget = nullptr;

void showAttributeEditorUI()
{
    ImGui::Begin("Attribute Manager");

    if (g_SceneSelectedType == SEL_MODEL && g_SceneSelectedIndex >= 0 && g_SceneSelectedIndex < sceneModelCount) {
        ShowModelAttributes(&sceneModels[g_SceneSelectedIndex]);
    }
    else if (g_SceneSelectedType == SEL_SCENENODE && g_SelectedSceneNode != nullptr) {
        ShowSceneNodeAttributes(g_SelectedSceneNode);
    }
    else {
        ImGui::TextDisabled("Select an object to edit.");
    }

    ImGui::End();

    // Display instance model browser — must be called outside any Begin/End pair
    s_InstanceModelBrowser.Display();
    if (s_InstanceModelBrowser.HasSelected() && s_InstanceBrowserTarget) {
        auto path = s_InstanceModelBrowser.GetSelected().string();
        strncpy(s_InstanceBrowserTarget->modelPath, path.c_str(),
                sizeof(s_InstanceBrowserTarget->modelPath) - 1);

        extern Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale);
        extern void instance_UpdateAABB(InstanceData* inst);
        if (s_InstanceBrowserTarget->instanceMeshes) {
            free(s_InstanceBrowserTarget->instanceMeshes);
            s_InstanceBrowserTarget->instanceMeshes = nullptr;
            s_InstanceBrowserTarget->instanceMeshCount = 0;
        }
        loadModel(s_InstanceBrowserTarget->modelPath, &s_InstanceBrowserTarget->instanceMeshes, &s_InstanceBrowserTarget->instanceMeshCount, 1.0f);
        instance_UpdateAABB(s_InstanceBrowserTarget);

        s_InstanceBrowserTarget = nullptr;
        s_InstanceModelBrowser.ClearSelected();
    }
}

// ---- Generic attr renderer -------------------------------------------------

static void RenderAttrs(const AttrDesc* attrs, int count, void* base) {
    for (int i = 0; i < count; i++) {
        const AttrDesc& a = attrs[i];
        void* ptr = (char*)base + a.offset;
        bool clamped = (a.lo != 0.0f || a.hi != 0.0f);

        switch (a.type) {
            case ATTR_FLOAT:
                if (clamped) ImGui::DragFloat(a.key, (float*)ptr, a.speed, a.lo, a.hi);
                else         ImGui::DragFloat(a.key, (float*)ptr, a.speed);
                break;
            case ATTR_VEC3:
                if (clamped) ImGui::DragFloat3(a.key, (float*)ptr, a.speed, a.lo, a.hi);
                else         ImGui::DragFloat3(a.key, (float*)ptr, a.speed);
                break;
            case ATTR_COLOR3:
                ImGui::ColorEdit3(a.key, (float*)ptr);
                break;
            case ATTR_BOOL:
                ImGui::Checkbox(a.key, (bool*)ptr);
                break;
            case ATTR_BOOL32: {
                int* v = (int*)ptr;
                bool b = *v != 0;
                if (ImGui::Checkbox(a.key, &b)) *v = b ? 1 : 0;
                break;
            }
            case ATTR_INT:
                if (clamped) ImGui::DragInt(a.key, (int*)ptr, a.speed, (int)a.lo, (int)a.hi);
                else         ImGui::DragInt(a.key, (int*)ptr, a.speed);
                break;
            case ATTR_STRING:
                ImGui::InputText(a.key, (char*)ptr, a.strSize > 0 ? a.strSize : 1);
                break;
        }
    }
}

// Render the entity's registered data section — for types that don't need custom UI.
static void RenderEntitySection(SceneNode* node) {
    const EntityDesc* d = findEntityDesc(node->type);
    if (!d || !d->attrs) return;
    void* base = (char*)node + d->dataOffset;
    if (ImGui::CollapsingHeader(d->sectionKey, ImGuiTreeNodeFlags_DefaultOpen))
        RenderAttrs(d->attrs, d->attrCount, base);
}

static void RenderAttrSection(const char* label, const AttrDesc* attrs, int count, void* base) {
    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        RenderAttrs(attrs, count, base);
}

// ---- Per-type UI -----------------------------------------------------------

void ShowModelAttributes(Model* model) {
    ImGui::Text("Model: %s", model->name);
    ImGui::InputText("Name", model->name, 64);
    ImGui::Spacing();
    ImGui::Text("Source File");
    ImGui::InputText("##ModelPath", model->filePath, 256);
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        ReloadModelFromFile(model, model->filePath);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Position");
        ImGui::DragFloat3("##ModelPos", (float*)&model->transform->position, 0.1f);
        ImGui::Text("Rotation");
        ImGui::DragFloat3("##ModelRot", (float*)&model->transform->rotation, 0.5f);
        ImGui::Text("Scale");
        float uniformScale = (model->transform->scale[0] + model->transform->scale[1] + model->transform->scale[2]) / 3.0f;
        if (ImGui::DragFloat("Uniform Scale", &uniformScale, 0.1f, 0.001f, 1000.0f))
            model->transform->scale = vec3(uniformScale);
        ImGui::DragFloat3("##ModelScale", (float*)&model->transform->scale, 0.1f, 0.001f, 1000.0f);
    }

    if (ImGui::CollapsingHeader("Sub-Meshes")) {
        for (int i = 0; i < model->meshCount; i++) {
            Mesh* mesh = &model->meshes[i];
            char meshHeader[128];
            sprintf(meshHeader, "Mesh [%d]: %s", i, mesh->name[0] ? mesh->name : "Unnamed");
            if (ImGui::TreeNode(meshHeader)) {
                ImGui::SeparatorText("Textures");
                auto texToggle = [](const char* lbl, GLuint id, Bool& useFlag) {
                    bool on = (id != 0 && useFlag);
                    ImGui::BeginDisabled(id == 0);
                    if (ImGui::Checkbox(lbl, &on)) {
                        useFlag = on ? True : False;
                    }
                    ImGui::EndDisabled();
                    if (id != 0) { ImGui::SameLine(); ImGui::TextDisabled("(#%u)", id); }
                };
                texToggle("Diffuse",  mesh->material.diffuseTexture, mesh->material.useDiffuseTexture);
                texToggle("Normal",   mesh->material.normalTexture, mesh->material.useNormalTexture);
                texToggle("ORM",      mesh->material.metallicRoughnessTexture, mesh->material.useMetallicRoughnessTexture);
                texToggle("AO",       mesh->material.aoTexture, mesh->material.useAOTexture);
                texToggle("Emissive", mesh->material.emissiveTexture, mesh->material.useEmissiveTexture);
                ImGui::SeparatorText("PBR Properties");
                ImGui::SliderFloat("Roughness", &mesh->material.roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Metalness", &mesh->material.metalness, 0.0f, 1.0f);
                ImGui::TreePop();
            }
        }
    }
}

void ShowLightAttributes(SceneNode* node) {
    LightData* light = &node->data.light;

    if (ImGui::CollapsingHeader("Light Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* types[] = { "Directional", "Point", "Spot" };
        int typeIdx = light->type;
        if (ImGui::Combo("Type", &typeIdx, types, 3)) {
            light->type = typeIdx;
        }

        ImGui::ColorEdit3("Color", (float*)&light->color);
        ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 1000.0f);

        if (light->type == LIGHT_POINT || light->type == LIGHT_SPOT) {
            ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 1000.0f);
        }

        if (light->type == LIGHT_DIRECTIONAL || light->type == LIGHT_SPOT) {
            ImGui::DragFloat3("Direction", (float*)&light->direction, 0.05f);
        }

        if (light->type == LIGHT_SPOT) {
            float innerDeg = acosf(light->innerCutoff) * (180.0f / 3.14159265f);
            float outerDeg = acosf(light->outerCutoff) * (180.0f / 3.14159265f);

            if (ImGui::SliderFloat("Inner Angle", &innerDeg, 0.0f, 90.0f)) {
                light->innerCutoff = cosf(innerDeg * (3.14159265f / 180.0f));
            }
            if (ImGui::SliderFloat("Outer Angle", &outerDeg, 0.0f, 90.0f)) {
                light->outerCutoff = cosf(outerDeg * (3.14159265f / 180.0f));
            }
        }

        ImGui::Checkbox("Cast Shadows", &light->cast_shadows);
    }
}

void ShowSceneNodeAttributes(SceneNode* node) {
    if (!node) return;

    ImGui::Text("Node: %s", node->name ? node->name : "Unnamed");
    char nameBuf[128];
    strncpy(nameBuf, node->name ? node->name : "", sizeof(nameBuf));
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        if (node->name) free((void*)node->name);
        node->name = strdup(nameBuf);
    }
    ImGui::Separator();

    // Cameras use their own position/target UI — skip generic transform for them
    if (node->type != ENTITY_CAMERA) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            float u = (node->scale[0] + node->scale[1] + node->scale[2]) / 3.0f;
            if (ImGui::DragFloat("Uniform Scale", &u, 0.1f, 0.001f, 1000.0f))
                node->scale = vec3(u, u, u);
            ImGui::Separator();
            RenderAttrs(g_TransformAttrs, g_TransformAttrCount, node);
        }
    }

    if (node->type == ENTITY_MODEL || node->type == ENTITY_LIGHT || node->type == ENTITY_INSTANCE) {
        if (ImGui::CollapsingHeader("Terrain Placement", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Snap to Terrain Y", &node->terrainYOffset);
            if (node->terrainYOffset) {
                std::vector<SceneNode*> terrains;
                extern SceneNode* g_SceneRoot;
                CollectTerrainNodes(g_SceneRoot, terrains);

                if (terrains.empty()) {
                    ImGui::TextDisabled("No terrains in scene graph.");
                } else {
                    const char* preview = "Select Terrain";
                    for (auto t : terrains) {
                        if (t->ID == node->selectedTerrainID) {
                            preview = t->name ? t->name : "Unnamed Terrain";
                            break;
                        }
                    }
                    if (ImGui::BeginCombo("Surface", preview)) {
                        for (auto t : terrains) {
                            bool is_selected = (t->ID == node->selectedTerrainID);
                            const char* tName = t->name ? t->name : "Unnamed Terrain";
                            if (ImGui::Selectable(tName, is_selected)) {
                                node->selectedTerrainID = t->ID;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }
    }

    ImGui::Spacing();

    if (node->type == ENTITY_MODEL) {
        ImGui::Text("Source File");
        ImGui::InputText("##src", node->sourcePath, sizeof(node->sourcePath));
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Textures")) {
            Mesh* mesh = &node->data.mesh;
            auto texToggle = [](const char* lbl, GLuint id, Bool& useFlag) {
                bool on = (id != 0 && useFlag);
                ImGui::BeginDisabled(id == 0);
                if (ImGui::Checkbox(lbl, &on)) {
                    useFlag = on ? True : False;
                }
                ImGui::EndDisabled();
                if (id != 0) { ImGui::SameLine(); ImGui::TextDisabled("(#%u)", id); }
            };
            texToggle("Diffuse",  mesh->material.diffuseTexture, mesh->material.useDiffuseTexture);
            texToggle("Normal",   mesh->material.normalTexture, mesh->material.useNormalTexture);
            texToggle("ORM",      mesh->material.metallicRoughnessTexture, mesh->material.useMetallicRoughnessTexture);
            texToggle("AO",       mesh->material.aoTexture, mesh->material.useAOTexture);
            texToggle("Emissive", mesh->material.emissiveTexture, mesh->material.useEmissiveTexture);
            ImGui::SeparatorText("PBR Properties");
            ImGui::SliderFloat("Roughness", &mesh->material.roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Metalness", &mesh->material.metalness, 0.0f, 1.0f);
            ImGui::Spacing();
            if (ImGui::Button("Clear All Textures")) {
                mesh->material.diffuseTexture            = 0;
                mesh->material.normalTexture             = 0;
                mesh->material.metallicRoughnessTexture  = 0;
                mesh->material.aoTexture                 = 0;
                mesh->material.emissiveTexture           = 0;
            }
        }
    } else if (node->type == ENTITY_LIGHT) {
        ShowLightAttributes(node);
    } else if (node->type == ENTITY_CAMERA) {
        Camera* cam = &node->data.camera;

        if (ImGui::CollapsingHeader("Camera Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", (float*)&cam->position, 0.1f);
            ImGui::DragFloat3("Target",   (float*)&cam->target,   0.1f);
            ImGui::DragFloat3("Up",       (float*)&cam->up,       0.05f);
            ImGui::DragFloat ("Roll",     &cam->roll,  0.5f,  -180.0f, 180.0f,  "%.1f deg");
            ImGui::DragFloat ("FOV",      &cam->fov,   0.5f,  5.0f,    160.0f,  "%.1f deg");
            ImGui::DragFloat ("Near",     &cam->near,  0.01f, 0.001f,  100.0f);
            ImGui::DragFloat ("Far",      &cam->far,   1.0f,  1.0f,    100000.0f);
        }

        ImGui::Spacing();
        bool isActive = (currentCameraMode == CAM_MODE_CUSTOM && g_ActiveCameraNode == node);
        if (isActive) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  Active Camera");
        } else {
            if (ImGui::Button("Set Active", ImVec2(-1, 0)))
                sg_SetActiveCamera(node);
        }
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;

        ImGui::Text("Model Path");
        ImGui::InputText("##instanceModelPath", inst->modelPath, sizeof(inst->modelPath));
        ImGui::SameLine();
        if (ImGui::Button("...##instanceBrowse")) {
            if (!s_InstanceBrowserInited) {
                s_InstanceModelBrowser.SetTitle("Select Model File");
                s_InstanceModelBrowser.SetTypeFilters({ ".obj", ".fbx", ".gltf", ".glb", ".3ds", ".dae" });
                s_InstanceBrowserInited = true;
            }
            s_InstanceBrowserTarget = inst;
            s_InstanceModelBrowser.Open();
        }
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Instancing Pattern", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* patterns[] = { "Grid", "Perlin Noise" };
            int patternIdx = inst->pattern;
            if (ImGui::Combo("Pattern##instance", &patternIdx, patterns, 2)) {
                inst->pattern = (InstancePattern)patternIdx;
            }
            ImGui::Spacing();

            if (inst->pattern == INSTANCE_PATTERN_GRID) {
                ImGui::Text("Grid Parameters");
                ImGui::DragInt("Count X##grid", &inst->gridCountX, 1, 1, 100);
                ImGui::DragInt("Count Z##grid", &inst->gridCountZ, 1, 1, 100);
                ImGui::DragFloat("Spacing X##grid", &inst->spacingX, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Spacing Z##grid", &inst->spacingZ, 0.1f, 0.1f, 100.0f);
            } else {
                ImGui::Text("Perlin Noise Parameters");
                ImGui::DragFloat("Noise Scale##perlin", &inst->noiseScale, 0.01f, 0.01f, 1.0f);
                ImGui::DragFloat("Noise Threshold##perlin", &inst->noiseThreshold, 0.01f, -1.0f, 1.0f);
                ImGui::DragFloat("Area Width##perlin", &inst->areaWidth, 0.5f, 1.0f, 1000.0f);
                ImGui::DragFloat("Area Depth##perlin", &inst->areaDepth, 0.5f, 1.0f, 1000.0f);
            }
        }

        if (ImGui::CollapsingHeader("Common Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Min Scale##inst", &inst->minScale, 0.1f, 0.01f, 100.0f);
            ImGui::DragFloat("Max Scale##inst", &inst->maxScale, 0.1f, 0.01f, 100.0f);
            bool hasRot = inst->randomYRotation > 0.5f;
            if (ImGui::Checkbox("Random Y Rotation##inst", &hasRot))
                inst->randomYRotation = hasRot ? 1.0f : 0.0f;
        }

        if (inst->instanceMeshCount > 0 && ImGui::CollapsingHeader("Materials")) {
            for (int i = 0; i < inst->instanceMeshCount; i++) {
                Mesh* mesh = &inst->instanceMeshes[i];
                char meshHeader[128];
                sprintf(meshHeader, "Mesh [%d]: %s##instMat", i, mesh->name[0] ? mesh->name : "Unnamed");
                if (ImGui::TreeNode(meshHeader)) {
                    ImGui::SliderFloat("Roughness", &mesh->material.roughness, 0.0f, 1.0f);
                    ImGui::SliderFloat("Metalness", &mesh->material.metalness, 0.0f, 1.0f);
                    ImGui::TreePop();
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Rebuild Instances##btn", ImVec2(-1, 0))) {
            extern void instance_GenerateInstances(InstanceData* inst);
            extern void instance_UploadToGPU(InstanceData* inst);
            extern Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale);
            if (inst->instanceMeshes) {
                free(inst->instanceMeshes);
                inst->instanceMeshes = nullptr;
                inst->instanceMeshCount = 0;
            }
            if (inst->modelPath[0] != '\0') {
                loadModel(inst->modelPath, &inst->instanceMeshes, &inst->instanceMeshCount, 1.0f);
            }
            instance_GenerateInstances(inst);
            instance_UploadToGPU(inst);
        }
    } else if (node->type == ENTITY_TERRAIN) {
        ShowTerrainAttributes(node);
    } else if (node->type == ENTITY_SKYBOX) {
        ShowSkyboxAttributes(node);
    }
}
