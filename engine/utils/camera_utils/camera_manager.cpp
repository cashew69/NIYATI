//#include "camera_base.h"

// Forward declarations of mode-specific functions
void ProcessWASDQE_Camera(void* window_handle, float dt);
void ProcessMouseBoard_Camera(void* window_handle, float dt);
void ProcessStrategicCamera(void* window_handle, float dt);

void HandleCameraInput(void* window_handle, float deltaTime) {
    if (currentCameraMode == CAM_MODE_WASD_EULER) {
        ProcessWASDQE_Camera(window_handle, deltaTime);
    } else if (currentCameraMode == CAM_MODE_MOUSE_BOARD) {
        ProcessMouseBoard_Camera(window_handle, deltaTime);
    } else if (currentCameraMode == CAM_MODE_STRATEGIC) {
        ProcessStrategicCamera(window_handle, deltaTime);
    }

    // Clamp pitch to prevent gimbal lock
    if (camera_pitch > 89.0f) camera_pitch = 89.0f;
    if (camera_pitch < -89.0f) camera_pitch = -89.0f;
}

mat4 GetActiveCameraViewMatrix() {
    if (currentCameraMode == CAM_MODE_STRATEGIC) {
        if (g_StrategicCameraCount > 0) {
            if (g_ActiveStrategicCameraIndex >= g_StrategicCameraCount) 
                g_ActiveStrategicCameraIndex = 0;
                
            StrategicCamera* cam = &g_StrategicCameras[g_ActiveStrategicCameraIndex];
            mat4 view = vmath::lookat(cam->pos, cam->target, cam->up);
            if (cam->roll != 0.0f) {
                view = vmath::rotate(cam->roll, vec3(0, 0, 1)) * view;
            }
            return view;
        }
    }

    mat4 translation = vmath::translate(-camera_pos[0], -camera_pos[1], -camera_pos[2]);
    return camera_rotation * translation;
}
