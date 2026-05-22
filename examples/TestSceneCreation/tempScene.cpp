#include "tempScene.h"

static SceneNode* camera = nullptr;
static Camera* nikhilcam = nullptr;

static SceneNode* spline1 = nullptr;
static SceneNode* targetspline = nullptr;
static SceneNode* krushna = nullptr;

static float xpos = 0.0f;

void scene1Init(void)
{
    g_SceneRoot = sg_LoadScene("scene.scene");
    if (!g_SceneRoot)
    {
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    }

    camera = sg_FindByName("NikhilCam");
    spline1 = sg_FindByName("spline1");
    targetspline = spline1;//sg_FindByName("spline2");
    krushna = sg_FindById(9);

    nikhilcam = sg_Camera(camera);

    printf("%f, %f, %f \n", nikhilcam->position[0], nikhilcam->position[1], nikhilcam->position[2]);

    xpos = nikhilcam->position[0];


    sg_SetActiveCamera(camera);

}


void scene1Display(void)
{

    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

static float t = 0.0f;

void scene1Update(void)
{
    t += g_DeltaTime;
    // Krushna position change
    //krushna->position = sg_GetSplinePoint(targetspline, t/6.0f);
    nikhilcam->position = sg_GetSplinePoint(spline1, t/24.0f);
    nikhilcam->target = krushna->position;
    nikhilcam->target[1] = sg_GetSplinePoint(spline1, t/24.0f)[1];

}
void scene1Cleanup(void);
