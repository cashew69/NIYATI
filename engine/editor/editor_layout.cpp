#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"

// Framebuffer manager and effects system implementations
#include "engine/core/gl/framebuffer.cpp"
#include "engine/effects/effects_manager.cpp"

// Forward decls
extern void CreateSceneModel(const char* name, const char* path);
extern void CreateSceneQuad(const char* name);
extern bool ReloadModelFromFile(Model* model, const char* path);
extern void AddSceneLight(const char* name);
extern void AddSceneReferenceObject(const char* name);
extern void AddSceneInstance(const char* name);

#include "engine/editor/scenemanager_layout.cpp"
#include "engine/editor/attributemanager_layout.cpp"
#include "engine/editor/utilbar_layout.cpp"
#include "engine/editor/renderorder_layout.cpp"
#include "engine/utils/scenegraph.h"
#include <string>

// --- Render Debug Panel ---
void ShowRenderOrderPanel() {
    if (ImGui::Begin("Render Order Visualizer")) {
        ImGui::Text("Nodes Rendered (Last Frame): %d", g_LastFrameRenderCount);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) g_LastFrameRenderCount = 0;

        if (g_LastFrameRenderCount == 0) {
            ImGui::TextDisabled("Enable 'Distance Sorting' on your camera to see data here.");
        } else {
            if (ImGui::BeginTable("RenderOrderTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Node Name");
                ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < g_LastFrameRenderCount; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i);
                    
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", g_LastFrameRenderOrder[i].name);
                    
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.2f m", g_LastFrameRenderOrder[i].dist);
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

// --- Scene Management Implementations ---

extern Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale);

void CreateSceneModel(const char* name, const char* path) {
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    
    if (path && path[0] != '\0') {
        Mesh* meshes = nullptr;
        int meshCount = 0;
        if (loadModel(path, &meshes, &meshCount, 1.0f)) {
            // Create a parent node for the entire model
            SceneNode* modelRoot = sg_CreateNode(ENTITY_EMPTY, name);
            strncpy(modelRoot->sourcePath, path, sizeof(modelRoot->sourcePath) - 1);
            
            for (int i = 0; i < meshCount; i++) {
                char meshName[64];
                if (strlen(meshes[i].name) > 0) {
                    strncpy(meshName, meshes[i].name, 64);
                } else {
                    snprintf(meshName, 64, "Mesh_%d", i);
                }
                
                SceneNode* meshNode = sg_CreateNode(ENTITY_MODEL, meshName);
                meshNode->data.mesh = meshes[i]; // Copy the mesh structure
                strncpy(meshNode->sourcePath, path, 256);
                meshNode->meshIndex = i;
                sg_AddChild(modelRoot, meshNode);
            }
            sg_AddChild(g_SceneRoot, modelRoot);
            LOG_I("Loaded model '%s' with %d meshes", path, meshCount);
        } else {
            LOG_E("Failed to load model from path: %s", path);
        }
    } else {
        // Create an empty node if no path provided
        SceneNode* node = sg_CreateNode(ENTITY_EMPTY, name);
        sg_AddChild(g_SceneRoot, node);
    }
}

void CreateSceneQuad(const char* name) {
    // Legacy stub, could be implemented with primitive generation
}

void AddSceneLight(const char* name) {
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* node = sg_CreateNode(ENTITY_LIGHT, name);
    // Initialize light data
    node->data.light.type = LIGHT_POINT;
    node->data.light.color = vec3(1.0f, 1.0f, 1.0f);
    node->data.light.intensity = 500.0f;
    node->data.light.radius = 100.0f;
    node->data.light.direction = vec3(0.0f, -1.0f, 0.0f);
    node->data.light.innerCutoff = 0.9f;
    node->data.light.outerCutoff = 0.8f;
    sg_AddChild(g_SceneRoot, node);
}

void AddSceneReferenceObject(const char* name) {
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* node = sg_CreateNode(ENTITY_EMPTY, name);
    sg_AddChild(g_SceneRoot, node);
}

void AddSceneInstance(const char* name) {
    extern void instance_Init(InstanceData* inst);
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* node = sg_CreateNode(ENTITY_INSTANCE, name);
    instance_Init(&node->data.instance);
    sg_AddChild(g_SceneRoot, node);
}

void AddSceneVolumetricCloud(const char* name) {
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* node = sg_CreateNode(ENTITY_VOLUMETRIC_CLOUD, name);
    sg_InitNode(node);
    sg_AddChild(g_SceneRoot, node);
    g_SceneSelectedType = SEL_SCENENODE;
    g_SelectedSceneNode = node;
}

void AddSceneSkyAtmosphere(const char* name) {
    if (!g_SceneRoot) g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* node = sg_CreateNode(ENTITY_SKY_ATMOSPHERE, name);
    sg_InitNode(node);
    sg_AddChild(g_SceneRoot, node);
    g_SceneSelectedType = SEL_SCENENODE;
    g_SelectedSceneNode = node;
}

bool ReloadModelFromFile(Model* model, const char* path) {
    // This is for the legacy Model struct, might be deprecated by SceneNode later
    return true; 
}

extern void ShowNVDFGenerator();

void ShowEditorLayout() {

    ShowEditorToolbar();
    showSceneEditorUI();
    showAttributeEditorUI();
    ShowNVDFGenerator();
    ShowRenderOrderPanel();
    ShowDynamicRenderOrderPanel();

}
#endif
