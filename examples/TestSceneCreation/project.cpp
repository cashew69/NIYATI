//#include "camera_base.h"
#include "engine/engine.h"
#include "platform.h"
#include "structs.h"
#include "utils/scenegraph.h"
#include "utils/scene_preloader.h"
#include "utils/scene_preloader.cpp"
#include <cstdio>

#include "krushnaScene1.cpp"
#include "ramScene.cpp"
#include "varah3scene.cpp"

static int   g_currentScene = 0;
static float g_sceneTimer   = 0.0f;

static ScenePreloader g_Preload;   // RAM.scene
static ScenePreloader g_Preload2;  // sdf.scene

static const float SCENE1_DURATION = 24.0f;
static const float SCENE2_DURATION = 20.0f;

void projectInit() {
    LOG_I("Project Initializing...");

    scene1Init();
    InitCustomCameras();

    preloader_Init(&g_Preload);
    preloader_Start(&g_Preload, "RAM.scene");
    LOG_I("[scene] Preloading RAM.scene in background");

    preloader_Init(&g_Preload2);
}

void projectUpdate() {
    preloader_Tick(&g_Preload);
    preloader_Tick(&g_Preload2);

    g_sceneTimer += g_DeltaTime;

    if (g_currentScene == 0) {
        scene1Update();

        if (g_sceneTimer >= SCENE1_DURATION && preloader_IsReady(&g_Preload)) {
            scene1Cleanup();

            SceneNode* ramScene = preloader_Take(&g_Preload);
            sg_FreeNode(g_SceneRoot);
            g_SceneRoot = ramScene;

            ramSceneInit();
            g_currentScene = 1;
            g_sceneTimer   = 0.0f;

            preloader_Start(&g_Preload2, "sdf.scene");
            LOG_I("[scene] Preloading sdf.scene in background");
        }
    } else if (g_currentScene == 1) {
        ramSceneUpdate();

        if (g_sceneTimer >= SCENE2_DURATION && preloader_IsReady(&g_Preload2)) {
            ramSceneCleanup();

            SceneNode* varahScene = preloader_Take(&g_Preload2);
            sg_FreeNode(g_SceneRoot);
            g_SceneRoot = varahScene;

            varah3SceneInit();
            g_currentScene = 2;
            g_sceneTimer   = 0.0f;
        }
    } else if (g_currentScene == 2) {
        varah3SceneUpdate();
    }
}

void projectRender() {
    if (g_currentScene == 0)
        scene1Display();
    else if (g_currentScene == 1)
        ramSceneDisplay();
    else
        varah3SceneDisplay();
}

void nextScene()
{
    if (g_currentScene == 0 && preloader_IsReady(&g_Preload)) {
        scene1Cleanup();

        SceneNode* ramScene = preloader_Take(&g_Preload);
        sg_FreeNode(g_SceneRoot);
        g_SceneRoot = ramScene;

        ramSceneInit();
        g_currentScene = 1;
        g_sceneTimer   = 0.0f;

        preloader_Start(&g_Preload2, "sdf.scene");
        LOG_I("[scene] Preloading sdf.scene in background");
    } else if (g_currentScene == 1 && preloader_IsReady(&g_Preload2)) {
        ramSceneCleanup();

        SceneNode* varahScene = preloader_Take(&g_Preload2);
        sg_FreeNode(g_SceneRoot);
        g_SceneRoot = varahScene;

        varah3SceneInit();
        g_currentScene = 2;
        g_sceneTimer   = 0.0f;
    }
}

void projectCleanup()
{
    preloader_Destroy(&g_Preload);
    preloader_Destroy(&g_Preload2);
}
