#ifdef HAS_IMGUI
#include "engine/dependancies/imgui/imgui.h"
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
#include <string>

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
    node->data.light.cast_shadows = true;    sg_AddChild(g_SceneRoot, node);
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

bool ReloadModelFromFile(Model* model, const char* path) {
    // This is for the legacy Model struct, might be deprecated by SceneNode later
    return true; 
}

void ShowEditorLayout() {

    ShowEditorToolbar();
    showSceneEditorUI();
    showAttributeEditorUI();

}
#endif
