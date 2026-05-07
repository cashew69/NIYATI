#include "camera_base.h"

Camera*    g_EditorCamera      = nullptr;
SceneNode* g_ActiveCameraNode  = nullptr;
enum CameraMode currentCameraMode = CAM_MODE_MOUSE_BOARD;

float camera_yaw         = 0.0f;
float camera_pitch       = 0.0f;
float camera_roll        = 0.0f;
float camera_speed       = 30.0f;
float camera_sensitivity = 0.5f;

extern float globalFOV;
extern void  resize(int w, int h);
extern int   viewportWidth, viewportHeight;

void ProcessMouseBoard_Camera(void* window_handle, float dt);

void HandleCameraInput(void* window_handle, float deltaTime) {
    if (currentCameraMode == CAM_MODE_MOUSE_BOARD)
        ProcessMouseBoard_Camera(window_handle, deltaTime);
}

mat4 GetActiveCameraViewMatrix() {
    if (currentCameraMode == CAM_MODE_CUSTOM && g_ActiveCameraNode) {
        Camera* cam = &g_ActiveCameraNode->data.camera;

        if (globalFOV != cam->fov) {
            globalFOV = cam->fov;
            resize(viewportWidth, viewportHeight);
        }

        vec3 up = (cam->up[0] == 0 && cam->up[1] == 0 && cam->up[2] == 0)
                  ? vec3(0.0f, 1.0f, 0.0f) : cam->up;
        mat4 view = vmath::lookat(cam->position, cam->target, up);
        if (cam->roll != 0.0f)
            view = vmath::rotate(cam->roll, vec3(0.0f, 0.0f, 1.0f)) * view;
        return view;
    }

    if (g_EditorCamera) {
        updateCamera(g_EditorCamera);
        if (globalFOV != g_EditorCamera->fov) {
            globalFOV = g_EditorCamera->fov;
            resize(viewportWidth, viewportHeight);
        }
        return g_EditorCamera->viewMatrix;
    }

    return mat4::identity();
}

// Returns active camera position — works for both modes.
vec3 GetActiveCameraPosition() {
    if (currentCameraMode == CAM_MODE_CUSTOM && g_ActiveCameraNode)
        return g_ActiveCameraNode->data.camera.position;
    return g_EditorCamera ? g_EditorCamera->position : vec3(0.0f, 0.0f, 0.0f);
}

Camera* GetActiveCamera() {
    // Only the editor free-look camera is a Camera* — scene cameras are Camera in nodes.
    return g_EditorCamera;
}

SceneNode* sg_GetActiveCameraNode() {
    return (currentCameraMode == CAM_MODE_CUSTOM) ? g_ActiveCameraNode : nullptr;
}

bool sg_IsActiveCamera(SceneNode* node) {
    return node && currentCameraMode == CAM_MODE_CUSTOM && g_ActiveCameraNode == node;
}
