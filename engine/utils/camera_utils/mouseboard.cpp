//#include "camera_base.h"

#include <GLFW/glfw3.h>
#define IS_PRESSED(key) (glfwGetKey((GLFWwindow*)window_handle, key) == GLFW_PRESS)

extern int mouse_x, mouse_y;
extern bool mouse_captured;

void ProcessMouseBoard_Camera(void* window_handle, float dt) {
    float moveAmount = camera_speed * dt;
    float mouseSens = camera_sensitivity * 0.3f;
    float rollSpeed = 60.0f * dt;

    if (mouse_captured) {
        // Look around with mouse
        camera_yaw   += (float)mouse_x * mouseSens;
        camera_pitch -= (float)mouse_y * mouseSens;
        
        mouse_x = 0;
        mouse_y = 0;
    }

    // 1. First calculate the latest rotation matrix
    mat4 rollRot  = vmath::rotate(-camera_roll,  vec3(0.0f, 0.0f, 1.0f));
    mat4 pitchRot = vmath::rotate(-camera_pitch, vec3(1.0f, 0.0f, 0.0f));
    mat4 yawRot   = vmath::rotate(-camera_yaw,   vec3(0.0f, 1.0f, 0.0f));
    
    camera_rotation = rollRot * pitchRot * yawRot;

    // 2. Extract relative basis vectors from the View Matrix
    // In OpenGL/vmath column-major:
    // col0 = Right, col1 = Up, col2 = -Forward (relative to the view transform)
    // To get camera axes in world space, we take the columns of the inverse rotation.
    // Inverse of rotation = Transpose. Columns of Transpose = Rows of Original.
    
    vec3 right   = vec3(camera_rotation[0][0], camera_rotation[1][0], camera_rotation[2][0]);
    vec3 forward = -vec3(camera_rotation[0][2], camera_rotation[1][2], camera_rotation[2][2]);

    // 3. Movement relative to the EYE
    if (IS_PRESSED(GLFW_KEY_W)) camera_pos += forward * moveAmount;
    if (IS_PRESSED(GLFW_KEY_S)) camera_pos -= forward * moveAmount;
    if (IS_PRESSED(GLFW_KEY_A)) camera_pos -= right   * moveAmount;
    if (IS_PRESSED(GLFW_KEY_D)) camera_pos += right   * moveAmount;
    
    // Vertical movement (Absolute Y)
    if (IS_PRESSED(GLFW_KEY_Y)) {
        if (IS_PRESSED(GLFW_KEY_LEFT_SHIFT) || IS_PRESSED(GLFW_KEY_RIGHT_SHIFT)) 
            camera_pos[1] -= moveAmount;
        else 
            camera_pos[1] += moveAmount;
    }
    
    if (IS_PRESSED(GLFW_KEY_Z)) camera_roll += rollSpeed;
    if (IS_PRESSED(GLFW_KEY_V)) camera_roll -= rollSpeed;
}
