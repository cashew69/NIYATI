//#include "camera_base.h"

#include <GLFW/glfw3.h>
#define IS_PRESSED(key) (glfwGetKey((GLFWwindow*)window_handle, key) == GLFW_PRESS)

// Global definitions for camera states
float camera_yaw = 0.0f;
float camera_pitch = 0.0f;
float camera_roll = 0.0f;

float camera_speed = 30.0f;
float camera_sensitivity = 0.5f;
mat4 camera_rotation = mat4::identity();

void ProcessWASDQE_Camera(void* window_handle, float dt) {
    float moveAmount = camera_speed * dt;
    float rotAmount = 60.0f * dt * camera_sensitivity;

    // Movement
    if (IS_PRESSED(GLFW_KEY_W)) camera_pos[2] -= moveAmount;
    if (IS_PRESSED(GLFW_KEY_S)) camera_pos[2] += moveAmount;
    if (IS_PRESSED(GLFW_KEY_A)) camera_pos[0] -= moveAmount;
    if (IS_PRESSED(GLFW_KEY_D)) camera_pos[0] += moveAmount;
    
    // Y for UP, Shift + Y for DOWN
    if (IS_PRESSED(GLFW_KEY_Y)) {
        if (glfwGetKey((GLFWwindow*)window_handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey((GLFWwindow*)window_handle, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            camera_pos[1] -= moveAmount;
        } else {
            camera_pos[1] += moveAmount;
        }
    }

    // Rotations
    if (IS_PRESSED(GLFW_KEY_Q)) camera_yaw -= rotAmount;
    if (IS_PRESSED(GLFW_KEY_E)) camera_yaw += rotAmount;
    if (IS_PRESSED(GLFW_KEY_X)) camera_pitch -= rotAmount;
    if (IS_PRESSED(GLFW_KEY_C)) camera_pitch += rotAmount;
    if (IS_PRESSED(GLFW_KEY_Z)) camera_roll -= rotAmount;
    if (IS_PRESSED(GLFW_KEY_V)) camera_roll += rotAmount;

    // Build the rotation matrix separate from manager
    mat4 rollRot  = vmath::rotate(-camera_roll,  vec3(0.0f, 0.0f, 1.0f));
    mat4 pitchRot = vmath::rotate(-camera_pitch, vec3(1.0f, 0.0f, 0.0f));
    mat4 yawRot   = vmath::rotate(-camera_yaw,   vec3(0.0f, 1.0f, 0.0f));
    
    camera_rotation = rollRot * pitchRot * yawRot;
}
