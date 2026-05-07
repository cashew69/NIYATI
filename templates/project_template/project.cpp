#include "engine/engine.h"
#include "utils/scenegraph.h"

// ============================================================================
// Scene entity handles (these are examples, )
//
// Find nodes after sg_LoadScene() using ID or name:
//   sg_FindById(2)           — stable, survives renames (preferred)
//   sg_FindByName("Sun")     — by name (breaks if renamed in editor)
//
// Typed accessors — return nullptr if node is null or the wrong type:
// USE ONLY for Accessing specific entity related Data. check the source for this first.
//   sg_Light(node)           → LightData*
//   sg_Camera(node)          → Camera*
//   sg_Mesh(node)            → Mesh*
//   sg_Instance(node)        → InstanceData*
//   sg_Terrain(node)         → TerrainNodeData*
//
// Camera helpers:
//   sg_GetCamera(node)       → Camera*   (same as sg_Camera, clearer intent)
//   sg_SetActiveCamera(node) — activate a scene camera for rendering
//   sg_IsActiveCamera(node)  — true when that node is currently rendering
// ============================================================================

// Cameras
static SceneNode* nikhilCam  = nullptr;
static SceneNode* cam2 = nullptr;

// Lights
static SceneNode* redLight  = nullptr;
static SceneNode* fillLight = nullptr;

// Objects
static SceneNode* instanceGroup = nullptr;
static float instanceX = 0.0f;
static float instanceY = 0.0f;

// Animation state
static float t = 0.0f;

// ============================================================================
void projectInit() {
    LOG_I("Project Initializing...");

    InitCustomCameras();

    // Load scene — restores all nodes to values in scene file (meshes, lights, cameras, terrain).
    //
    g_SceneRoot = sg_LoadScene("scene.scene");
    if (!g_SceneRoot)
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");

    // Cameras
    // Look up cameras saved from the editor. Create one if the scene has none.
    nikhilCam  = sg_FindByName("NikhilCam");
    cam2  = sg_FindById(13);



    sg_SetActiveCamera(nikhilCam);

    // Instances
    instanceGroup = sg_FindByName("Instance");
    if (instanceGroup)
    {
        instanceX = instanceGroup->position[0];
        instanceY = instanceGroup->position[1];
    }

}

// ============================================================================
void projectUpdate() {
    t += g_DeltaTime;

    if (instanceGroup) {
        float radius = 1.0f;
        instanceGroup->position[0] = instanceX + radius * cosf(t);
        instanceGroup->position[1] = instanceY + radius * sinf(t);
    }

    // Camera switching
    if (cam2)
    {
        if (g_Time > 5.0f && !sg_IsActiveCamera(cam2))
        {
            sg_SetActiveCamera(cam2);
        }
    }
}

// ============================================================================
void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

// ============================================================================
void projectCleanup() {}
