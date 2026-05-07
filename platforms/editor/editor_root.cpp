// platforms/editor/editor_root.cpp
// Editor build root — included by glfwmain.cpp.
// Demo builds use platform_common.cpp + active_project.h instead.

#include "platforms/engine_loop.cpp"

// ============================================================================
// EDITOR LAYOUT
// ============================================================================

#include "engine/editor/editor_layout.cpp"

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
}

void projectUpdate() {}

void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    RenderSceneModels(view, perspectiveProjectionMatrix);
    renderEditorPrimitives(view, perspectiveProjectionMatrix);
    RenderCustomCameraHelpers(view, perspectiveProjectionMatrix);
    extern void RenderLightHelpers(mat4 view, mat4 proj);
    RenderLightHelpers(view, perspectiveProjectionMatrix);
    RenderLightIcons(view, perspectiveProjectionMatrix);
}

void projectCleanup() {}

extern void NewFrameGUI(); // defined in imgui_setup.cpp, included after this file

void UpdateGUI() {
    NewFrameGUI();
    ShowEditorLayout();
}
