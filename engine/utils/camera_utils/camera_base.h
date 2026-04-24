
//#ifdef CAMERA_BASE_H
#define CAMERA_BASE_H



enum CameraMode {
    CAM_MODE_WASD_EULER = 0,
    CAM_MODE_MOUSE_BOARD = 1,
    CAM_MODE_STRATEGIC = 2
};

// Globals for FLIGHT / EDITOR camera (WASD/Mouse)
extern vec3  camera_pos;
extern float camera_yaw;
extern float camera_pitch;
extern float camera_roll;

// Strategic Camera System
#define MAX_STRATEGIC_CAMERAS 16
struct StrategicCamera {
    vec3  pos;
    vec3  target;
    vec3  up;
    float roll;
    char  name[32];
};

extern StrategicCamera g_StrategicCameras[MAX_STRATEGIC_CAMERAS];
extern int g_StrategicCameraCount;
extern int g_SelectedStrategicCameraIndex; // For UI Inspector
extern int g_ActiveStrategicCameraIndex;   // For Rendering (dropdown)

enum CameraMode currentCameraMode;
extern mat4 camera_rotation;
extern float camera_speed;
extern float camera_sensitivity;

// Core Functions
void HandleCameraInput(void* window_handle, float deltaTime);
mat4 GetActiveCameraViewMatrix();
void RenderStrategicCameraHelpers(mat4 view, mat4 proj);
//#endif


