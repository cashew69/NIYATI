#include "ramScene.h"

static SceneNode* ramCamera = nullptr;
static Camera*    ramCam    = nullptr;

static SceneNode* ramSpline = nullptr;
static SceneNode* rama      = nullptr;

static float ramT = 0.0f;

void ramSceneInit(void)
{
    // g_SceneRoot is already set by the preloader in project.cpp
    ramCamera = sg_FindByName("ramCam");
    ramSpline = sg_FindByName("spline");
    rama      = sg_FindById(10);

    if (!ramCamera) { printf("ERROR: ramCam not found\n"); return; }
    if (!ramSpline) { printf("ERROR: spline not found\n"); return; }
    if (!rama)      { printf("ERROR: Rama model (id=10) not found\n"); return; }

    ramCam = sg_Camera(ramCamera);
    if (!ramCam) { printf("ERROR: ramCam is not a camera node\n"); return; }

    printf("ramCam position: %f, %f, %f\n",
           ramCam->position[0], ramCam->position[1], ramCam->position[2]);

    sg_SetActiveCamera(ramCamera);
}

void ramSceneDisplay(void)
{
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

void ramSceneUpdate(void)
{
    if (!ramCam || !ramSpline || !rama) return;

    ramT += g_DeltaTime;

    float splineT = fmodf(ramT / 24.0f, 1.0f);

    ramCam->position = sg_GetSplinePoint(ramSpline, splineT);
    ramCam->target   = { 462.0f, 30.7f, 500.0f };
}

void ramSceneCleanup(void)
{
}
