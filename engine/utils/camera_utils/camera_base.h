#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include "engine/core/gl/camera.h"

enum CameraMode {
    CAM_MODE_MOUSE_BOARD = 0,
    CAM_MODE_CUSTOM      = 1
};

// Maximum scene cameras (used for gizmo batch buffer sizing only)
#define MAX_SCENE_CAMERAS 16

// Editor free-look camera — always available, not a scene node
extern Camera* g_EditorCamera;

// Active scene camera — set via sg_SetActiveCamera(node).
// Null = use editor free-look camera.
struct SceneNode;
extern SceneNode* g_ActiveCameraNode;

extern enum CameraMode currentCameraMode;
extern float camera_speed;
extern float camera_sensitivity;

// Euler angles for editor camera mouse-look
extern float camera_yaw;
extern float camera_pitch;
extern float camera_roll;

// Core functions
void    HandleCameraInput(void* window_handle, float deltaTime);
mat4    GetActiveCameraViewMatrix();
vec3    GetActiveCameraPosition();      // works for both editor and scene cameras
Camera* GetActiveCamera();             // returns g_EditorCamera only (editor free-look)

// Scene-camera queries — use these instead of touching g_ActiveCameraNode directly.
SceneNode* sg_GetActiveCameraNode();           // null when editor free-look is active
bool       sg_IsActiveCamera(SceneNode* node); // true if node is the current scene camera

void   RenderCustomCameraHelpers(mat4 view, mat4 proj);
void   InitCustomCameras();

#endif
