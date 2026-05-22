// platforms/editor/editor_root.cpp
// Editor build root — included by glfwmain.cpp.
// Demo builds use platform_common.cpp + active_project.h instead.

#include "platforms/engine_loop.cpp"

// ============================================================================
// EDITOR LAYOUT
// ============================================================================

#include "engine/editor/editor_layout.cpp"

// ============================================================================
// EFFECTS MANAGER
// ============================================================================

#include "engine/effects/effects_manager.h"

EffectsManager* g_EffectsManager = nullptr;

// ============================================================================
// EDITOR PROJECT
// ============================================================================

void projectInit() {
    LOG_I("Editor Initializing...");
    InitCustomCameras();
    if (g_EditorCamera)
        g_EditorCamera->position = vec3(5.0f, 10.0f, 75.0f);
    if (!g_SceneRoot)
        g_SceneRoot = sg_CreateNode(ENTITY_EMPTY, "Scene Root");
    SceneNode* mainCam = sg_AddCameraNode("Main Camera");
    sg_AddChild(g_SceneRoot, mainCam);

    // Initialize effects manager with fallback dimensions
    extern int viewportWidth, viewportHeight;
    int efxWidth = viewportWidth > 0 ? viewportWidth : 1280;
    int efxHeight = viewportHeight > 0 ? viewportHeight : 720;
    g_EffectsManager = effects_Create(efxWidth, efxHeight);
    if (g_EffectsManager) {
        LOG_I("Effects manager created: %dx%d", efxWidth, efxHeight);
    } else {
        LOG_E("Failed to create effects manager");
    }
}

void projectUpdate() {}

void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();

    // Sync config to effects
    if (g_EffectsManager) {
        effects_UpdateConfig(g_EffectsManager, &g_EffectsConfig);
    }

    if (g_EffectsManager && g_SceneRoot) {
        effects_RenderAll(g_EffectsManager, g_SceneRoot, view, perspectiveProjectionMatrix);
    } else {
        RenderSceneModels(view, perspectiveProjectionMatrix);
    }

    renderEditorPrimitives(view, perspectiveProjectionMatrix);
    RenderCustomCameraHelpers(view, perspectiveProjectionMatrix);
    extern void RenderLightHelpers(mat4 view, mat4 proj);
    RenderLightHelpers(view, perspectiveProjectionMatrix);
    RenderLightIcons(view, perspectiveProjectionMatrix);
}

void projectCleanup() {
    if (g_EffectsManager) {
        effects_Destroy(g_EffectsManager);
        g_EffectsManager = nullptr;
        LOG_I("Effects manager destroyed");
    }
}

extern void NewFrameGUI(); // defined in imgui_setup.cpp, included after this file

void UpdateGUI() {
    NewFrameGUI();
    ShowEditorLayout();
}
