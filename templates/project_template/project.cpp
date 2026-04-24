#define PROJECT_TEMPLATE
#include "engine/engine.h"
#include "engine/utils/editor_utils.h"

// Include shared editor layout
#include "engine/editor/editor_layout.cpp"

// Forward declarations
extern void NewFrameGUI();
extern void ShowEditorLayout();

void projectInit() {
    LOG_I("Project Initializing...");
    
    // 1. Setup Initial Camera Position
    camera_pos = vec3(10.0f, 10.0f, 10.0f);

    // 2. Build Line Shader (Needed for Grid/Axis)
    const char *lineShaderFiles[5] = { 
        "engine/shaders/lineVert.glsl", 
        NULL, NULL, NULL, 
        "engine/shaders/lineFrag.glsl" 
    };
    buildShaderProgramFromFiles(lineShaderFiles, 5, &lineShaderProgram, attribNames, attribIndices, 4);
    
    // 3. Initialize Grid/Axis Buffers
    initEditorPrimitives();
}


void projectUpdate() {
}

void projectRender() {
    mat4 view = GetActiveCameraViewMatrix();
    renderEditorPrimitives(view, perspectiveProjectionMatrix);
    RenderStrategicCameraHelpers(view, perspectiveProjectionMatrix);
}

void projectCleanup() {
}

#ifdef HAS_IMGUI
void UpdateGUI() {
    NewFrameGUI(); 
    ShowEditorLayout();
}
#endif
