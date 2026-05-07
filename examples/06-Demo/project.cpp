#include "camera_base.h"
#include "engine/engine.h"
#include "platform.h"
#include "structs.h"
#include "utils/scenegraph.h"
#include <cstdio>

// ============================================================================
// Scene entity handles
//
// After building the scene in the editor, check the Scene Manager panel for
// each node's ID. IDs are saved to the .scene file and never change.
//   sg_FindById(2)           — stable lookup by ID
//   sg_FindByName("Sun")     — lookup by name (breaks if renamed)
// Combine with typed accessors:
//   sg_Light(sg_FindByName("Sun"))  — PointLightData* or null if wrong type
//   sg_Mesh(sg_FindById(3))         — Mesh* or null if wrong type
// ============================================================================

static SceneNode* nikhilCam       = nullptr;
static SceneNode* Cam2       = nullptr;
static SceneNode* planeInstance = nullptr;
static SceneNode* redLight = nullptr;
static LightData* light = nullptr;
static float instanceX = 0.0f;
static float instanceY = 0.0f;

static float lightX = 0.0f;
static float lightY = 0.0f;

void projectInit() {
    LOG_I("Project Initializing...");

    InitCustomCameras();

    // Load scene — restores all nodes (meshes, lights, terrain, cameras).
    g_SceneRoot = sg_LoadScene("scene.scene");
    if (!g_SceneRoot)
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");

    // ---- Find scene entities -----------------------------------------------
    planeInstance = sg_FindById(5);
    if (planeInstance) {
        instanceX = planeInstance->position[0];
        instanceY = planeInstance->position[1];
    } else {
        LOG_I("Warning: node ID 5 not found — check Scene Manager.");
    }

    // ---- Camera setup -------------------------------------------------------
    // sg_LoadScene restores camera nodes saved from the editor.
    // If the scene has no camera yet, sg_AddCameraNode creates one.
    nikhilCam = sg_FindByName("NikhilCam");
    Cam2 = sg_FindByName("Cam2");
    if (nikhilCam) {
        printf("%f \n",nikhilCam->position[0]);
    }

    Camera* cam = sg_GetCamera(nikhilCam);
    redLight = sg_FindByName("Light");
    /*if (cam) {
        cam->position = vec3(5.0f, 10.0f, 75.0f);
        cam->target   = vec3(0.0f,  0.0f,  0.0f);
    }*/

    sg_SetActiveCamera(nikhilCam);
}

static float t = 0.0f;

void projectUpdate() {
    t += g_DeltaTime;

    if (planeInstance) {
        float radius = 1.0f;
        planeInstance->position[0] = instanceX + radius * cosf(t);
        planeInstance->position[1] = instanceY + radius * sinf(t);
        redLight->position[0] = lightX + 117.0 * sin(t);
        //redLight->position[1] = lightY + radius * sinf(t);
    }
    if (g_Time > 10.0f && !sg_IsActiveCamera(Cam2))
    {
        sg_SetActiveCamera(Cam2);
    }

}

void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

void projectCleanup() {}
