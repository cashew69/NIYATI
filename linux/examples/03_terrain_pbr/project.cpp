
// ============================================================================
// Example 03: Terrain + PBR + Skybox
// Most Ambitious Example.
// Includes: Tessellation, PBR, Skybox, Camera Modes, Controles etc.

// Currently this project is still not completly decoupled from engine. bare that in mind,
// can be little tricky to figure out declarations and purpose of few functionalities.
// ============================================================================

// Tell platform files that project 03 provides updateCameraFromMouse/toggleWireframe
#define PROJECT_03

// ---- Camera and mouse vars (project 03 owns these) ----
Camera *mainCamera          = NULL;
vec3    camera_target(0.0f, 0.0f, 0.0f);
float   camera_yaw          = 0.0f;
float   camera_pitch        = 0.0f;
float   mouse_sensitivity   = 0.1f;

// Pull in subsystems — ORDER MATTERS: pbr.cpp defines bindIBL(), used by terrain.cpp
#include "user/pbr.cpp"
#include "user/terrain/terrain.h"
#include "user/terrain/terrain.cpp"
#include "user/skybox.cpp"
#include "user/userrendercalls.cpp"

// ============================================================================
// Project 03 Camera System (multi-mode: FPS / Manual / Orbit)
// ============================================================================

int   cameraMode           = 0;   // 0=FPS, 1=Manual, 2=Orbit
vec3  camCenter(0.0f, 0.0f, 0.0f);
vec3  camUp(0.0f, 1.0f, 0.0f);
float orbitRadius          = 100.0f;
bool  orbitKeyboardControl = false;
bool  showOrbitVisuals     = true;
quaternion camera_orientation = quaternion(0.0f, 0.0f, 0.0f, 1.0f);
bool  use_camera_quaternion   = false;

void updateCameraFromMouse(int delta_x, int delta_y) {
    // In Orbit mode camera_yaw/pitch drive the orbit angles (reused as orbit angles)
    // In FPS mode they drive the look direction directly
    camera_yaw   += delta_x * mouse_sensitivity;
    camera_pitch -= delta_y * mouse_sensitivity;
    if (camera_pitch >  89.0f) camera_pitch =  89.0f;
    if (camera_pitch < -89.0f) camera_pitch = -89.0f;
}

static int togglePolyLine = 0;
void toggleWireframe(void) {
    togglePolyLine = !togglePolyLine;
    glPolygonMode(GL_FRONT_AND_BACK, togglePolyLine ? GL_LINE : GL_FILL);
    fprintf(gpFile, "Wireframe: %s\n", togglePolyLine ? "ON" : "OFF");
}

static GLuint HeightMap = 0;

void projectInit()
{
    camera_pos   = vec3(0.0f, 10.0f, 100.0f);
    camera_yaw   = 0.0f;
    camera_pitch = 0.0f;
    mainCamera   = createCamera(camera_pos, camera_target, vec3(0.0f, 1.0f, 0.0f));


    // Used for terrain
    // ===== Tessellation Shader Program =====
    const char *tessShaderFiles[5] = {
        "engine/shaders/vertex_shader.glsl", "user/terrain/main_tcs.glsl",
        "user/terrain/main_tes.glsl", NULL,
        "engine/shaders/PBR/pbrFrag.glsl"
    };
    if (!buildShaderProgramFromFiles(tessShaderFiles, 5,
                                     &tessellationShaderProgram, attribNames,
                                     attribIndices, 4)) {
        fprintf(gpFile, "Failed to build tessellation shader program\n");
    } else {
        tessellationShaderProgram->name = "Tessellation";
    }

    

    // ===== PBR Shader Program =====
    const char *pbrShaderFiles[5] = {
        "engine/shaders/vertex_shader.glsl", NULL, NULL, NULL,
        "engine/shaders/PBR/pbrFrag.glsl"
    };
    if (!buildShaderProgramFromFiles(pbrShaderFiles, 5, &pbrShaderProgram,
                                     attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build PBR shader program\n");
    } else {
        pbrShaderProgram->name = "PBR";
    }

    // Load DamagedHelmet model
    if (!loadModel("user/models/DamagedHelmet.glb", &helmetMeshes,
                   &helmetMeshCount, 1.0f)) {
        fprintf(gpFile, "Failed to load DamagedHelmet model\n");
    }

    // Initialize Skybox and IBL
    initSkybox("rogland_clear_night_4k.hdr");

    // Create Terrain Mesh and Heightmap
    terrainMesh = createTerrainMesh();
    if (terrainMesh) {
        fprintf(gpFile, "Terrain mesh created successfully\n");
    } else {
        fprintf(gpFile, "Warning: Failed to create terrain mesh\n");
    }

    HeightMap = createHeightMapTexture(512, 512, 0.01f, 1.0f);
    if (HeightMap) {
        fprintf(gpFile, "Heightmap texture created: ID=%u\n", HeightMap);
    }

    fprintf(gpFile, "Project 03 (Terrain + PBR) initialized\n");
}

void projectRender()
{
    // Multi-mode camera: reuses camera_yaw/pitch as orbit angles in mode 2
    vec3 up = vec3(0.0f, 1.0f, 0.0f);

    if (cameraMode == 1) { // Manual
        mainCamera->target = camera_target;
        mainCamera->up     = camUp;
    } else if (cameraMode == 2) { // Orbit — camera_yaw/pitch drive orbit angles
        float yr = camera_yaw   * 3.14159265f / 180.0f;
        float pr = camera_pitch * 3.14159265f / 180.0f;
        camera_pos = camCenter + vec3(
            orbitRadius * cos(pr) * sin(yr),
            orbitRadius * sin(pr),
            orbitRadius * cos(pr) * cos(yr));
        mainCamera->target = camCenter;
    } else { // FPS
        float yr = camera_yaw   * 3.14159265f / 180.0f;
        float pr = camera_pitch * 3.14159265f / 180.0f;
        vec3 dir(cos(pr)*sin(yr), sin(pr), cos(pr)*cos(yr));
        mainCamera->target = camera_pos + dir;
    }

    mainCamera->position      = camera_pos;
    mainCamera->up            = up;
    mainCamera->orientation   = camera_orientation;
    mainCamera->useQuaternion = use_camera_quaternion;
    updateCamera(mainCamera);
    viewMatrix = mainCamera->viewMatrix;

    renderTerrain(HeightMap);
    renderUserMeshes(HeightMap);
}

void projectUpdate()
{
    // Nothing extra for now
}

void projectCleanup()
{
    if (terrainMesh) {
        terrainMesh = NULL;
    }
    if (HeightMap) {
        glDeleteTextures(1, &HeightMap);
        HeightMap = 0;
    }
    fprintf(gpFile, "Project 03 (Terrain + PBR) cleaned up\n");
}

// GUI panels — only compiled when building with ImGui (GLFW target sets HAS_IMGUI)
#ifdef HAS_IMGUI
#include "examples/03_terrain_pbr/gui.cpp"
#else
// X11 build: no ImGui — provide empty stubs so glfwmain's stubs don't conflict
void UpdateGUI() {}
void NewFrameGUI() {}
#endif
