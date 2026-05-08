//#include "camera_base.h"
#include "engine/engine.h"
#include "platform.h"
#include "structs.h"
#include "utils/scenegraph.h"
#include <cstdio>

static SceneNode* nikhilCam       = nullptr;
static SceneNode* Cam2       = nullptr;
static Camera* cam = nullptr;
static SceneNode* planeInstance = nullptr;
static SceneNode* redLight = nullptr;
SceneNode* nikhilCamPosSpline = nullptr;
SceneNode* spline2 = nullptr;
SceneNode* krishna = nullptr;
static LightData* light = nullptr;
static float instanceX = 0.0f;
static float instanceY = 0.0f;

static float lightX = 0.0f;
static float lightY = 0.0f;

void projectInit() {
    LOG_I("Project Initializing...");

    InitCustomCameras();

    // Load scene: restores all nodes (meshes, lights, terrain, cameras).
    g_SceneRoot = sg_LoadScene("scene.scene");
    if (!g_SceneRoot)
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");

    // Find scene entities
    krishna = sg_FindByName("krishna");
    planeInstance = sg_FindById(5);
    if (planeInstance) {
        instanceX = planeInstance->position[0];
        instanceY = planeInstance->position[1];
    } else {
        LOG_I("Warning: node ID 5 not found — check Scene Manager.");
    }
    // Camera setup
    nikhilCam = sg_FindByName("NikhilCam");
    Cam2 = sg_FindByName("Cam2");
    nikhilCamPosSpline = sg_FindByName("spline1");
    spline2 = sg_FindByName("spline2");
    if (nikhilCam) {
        printf("%f \n",nikhilCam->position[0]);
    }

    cam = sg_GetCamera(nikhilCam);
    redLight = sg_FindByName("Light");
    /*if (cam) {
        cam->position = vec3(5.0f, 10.0f, 75.0f);
        cam->target   = vec3(0.0f,  0.0f,  0.0f);
    }*/

    sg_SetActiveCamera(nikhilCam);
}

static float t = 0.0f;
static float progress = 0.0f;

void projectUpdate() {
    t += g_DeltaTime;

    if (planeInstance) {
        float radius = 1.0f;
        planeInstance->position[0] = instanceX + radius * cosf(t);
        planeInstance->position[1] = instanceY + radius * sinf(t);

    }

    if (redLight) {
    redLight->position[0] = lightX + 117.0 * sin(t);
    //redLight->position[1] = lightY + radius * sinf(t);
    }

    if (nikhilCam)
    {
    //nikhilCam->data.camera.position[0] = redLight->position[0];
    }


    vec3 newCamPos = sg_AdvanceSpline(nikhilCamPosSpline, &progress, 0.05f * g_DeltaTime);
    cam->position[0] = newCamPos[0];
    cam->position[1] = newCamPos[1];
    cam->position[2] = newCamPos[2];

    vec3 krishnaPos = sg_GetSplinePoint(spline2, t / 5.0f);
    krishna->position[0] = krishnaPos[0];
    krishna->position[1] = krishnaPos[1];
    krishna->position[2] = krishnaPos[2];

    cam->target[0] = krishna->position[0];
    cam->target[1] = krishna->position[1];
    cam->target[2] = krishna->position[2];
}
void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

void projectCleanup()
{



}
