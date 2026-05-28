#include "engine/engine.h"
#include "platform.h"
#include "utils/scenegraph.h"
#include "utils/scene_preloader.h"
#include "utils/scene_preloader.cpp"
#include <cstdio>

// ---- Scene list ------------------------------------------------------------

static const char* g_SceneList[] = {
    "scene.scene",
    "RAM.scene",
};
static const int g_SceneCount = 2;
static int       g_CurrentScene = 0;

// ---- Preloader -------------------------------------------------------------

static ScenePreloader g_Preload;

// ---- Per-scene node pointers -----------------------------------------------

static SceneNode* maincam = nullptr;
static Camera*    cam     = nullptr;

static void lookupSceneNodes() {
    maincam = sg_FindByName("Main Camera");
    cam     = sg_GetCamera(maincam);
    if (maincam)
        sg_SetActiveCamera(maincam);
    else
        LOG_I("[scene] No 'Main Camera' in scene %d", g_CurrentScene);
}

// ---- Called from linmain on N keypress -------------------------------------

void nextScene() {
    if (!preloader_IsReady(&g_Preload)) {
        LOG_I("[scene] Next scene not ready yet — still loading");
        return;
    }

    SceneNode* incoming = preloader_Take(&g_Preload);
    sg_FreeNode(g_SceneRoot);
    g_SceneRoot         = incoming;
    g_SelectedSceneNode = nullptr;

    g_CurrentScene = (g_CurrentScene + 1) % g_SceneCount;
    LOG_I("[scene] Switched to %s", g_SceneList[g_CurrentScene]);
    lookupSceneNodes();

    int nextIndex = (g_CurrentScene + 1) % g_SceneCount;
    preloader_Start(&g_Preload, g_SceneList[nextIndex]);
    LOG_I("[scene] Preloading %s in background", g_SceneList[nextIndex]);
}

// ---- Engine callbacks ------------------------------------------------------

void projectInit() {
    LOG_I("[scene] Loading %s", g_SceneList[0]);
    InitCustomCameras();

    g_SceneRoot = sg_LoadScene(g_SceneList[0]);
    if (!g_SceneRoot)
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");

    lookupSceneNodes();

    preloader_Init(&g_Preload);
    preloader_Start(&g_Preload, g_SceneList[1]);
    LOG_I("[scene] Preloading %s in background", g_SceneList[1]);
}

void projectUpdate() {
    // Calls sg_InitNode on main thread when background parse finishes.
    preloader_Tick(&g_Preload);

    static bool notified = false;
    if (!notified && preloader_IsReady(&g_Preload)) {
        LOG_I("[scene] Next scene is GPU-ready — press N to switch");
        notified = true;
    }
}

void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
}

void projectCleanup() {
    preloader_Destroy(&g_Preload);
}
