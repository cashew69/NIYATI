#include "camera_base.h"

#ifdef HAS_IMGUI
#include <GLFW/glfw3.h>

#define IS_PRESSED(key) (glfwGetKey((GLFWwindow*)window_handle, key) == GLFW_PRESS)

static double last_mx = 0, last_my = 0;

void ProcessMouseBoard_Camera(void* window_handle, float dt) {
    if (!g_EditorCamera) return;

    float moveAmount = camera_speed * dt;

    mat4 orient  = g_EditorCamera->orientation.asMatrix();
    vec3 right   =  vec3(orient[0][0], orient[0][1], orient[0][2]);
    vec3 up      =  vec3(orient[1][0], orient[1][1], orient[1][2]);
    vec3 forward = -vec3(orient[2][0], orient[2][1], orient[2][2]);

    bool isRightClickHeld = glfwGetMouseButton((GLFWwindow*)window_handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (isRightClickHeld) {
        if (IS_PRESSED(GLFW_KEY_W)) g_EditorCamera->position += forward * moveAmount;
        if (IS_PRESSED(GLFW_KEY_S)) g_EditorCamera->position -= forward * moveAmount;
        if (IS_PRESSED(GLFW_KEY_A)) g_EditorCamera->position -= right   * moveAmount;
        if (IS_PRESSED(GLFW_KEY_D)) g_EditorCamera->position += right   * moveAmount;

        if (IS_PRESSED(GLFW_KEY_Y)) {
            bool shift = IS_PRESSED(GLFW_KEY_LEFT_SHIFT) || IS_PRESSED(GLFW_KEY_RIGHT_SHIFT);
            g_EditorCamera->position += up * (shift ? -moveAmount : moveAmount);
        }
    }

    double mx, my;
    glfwGetCursorPos((GLFWwindow*)window_handle, &mx, &my);

    // Always track position while not clicking so re-pressing never jumps
    float dx = 0.0f, dy = 0.0f;
    if (isRightClickHeld) {
        dx = (float)(mx - last_mx);
        dy = (float)(my - last_my);
    }
    last_mx = mx;
    last_my = my;

    bool rotated = false;
    if (isRightClickHeld) {
        camera_yaw   -= dx * camera_sensitivity * 0.1f;
        camera_pitch -= dy * camera_sensitivity * 0.1f;

        // Clamp pitch BEFORE computing quaternion so this frame is already correct
        if (camera_pitch >  88.0f) camera_pitch =  88.0f;
        if (camera_pitch < -88.0f) camera_pitch = -88.0f;

        rotated = true;
    }

    if (isRightClickHeld) {
        if (IS_PRESSED(GLFW_KEY_Q)) { camera_roll -= 60.0f * dt; rotated = true; }
        if (IS_PRESSED(GLFW_KEY_E)) { camera_roll += 60.0f * dt; rotated = true; }
        if (IS_PRESSED(GLFW_KEY_R)) { camera_roll  = 0.0f;       rotated = true; }
    }

    if (rotated) {
        auto fromAxisAngle = [](vec3 axis, float angle) {
            float rad  = radians(angle);
            float real = cosf(rad * 0.5f);
            float s    = sinf(rad * 0.5f);
            return quaternion(axis[0]*s, axis[1]*s, axis[2]*s, real);
        };

        quaternion qYaw   = fromAxisAngle(vec3(0,1,0), camera_yaw);
        quaternion qPitch = fromAxisAngle(vec3(1,0,0), camera_pitch);

        // Pitch * Yaw keeps the horizon level
        g_EditorCamera->orientation = qPitch * qYaw;

        if (camera_roll != 0.0f) {
            mat4 orientMat   = g_EditorCamera->orientation.asMatrix();
            vec3 localForward = -vec3(orientMat[2][0], orientMat[2][1], orientMat[2][2]);
            quaternion qRoll  = fromAxisAngle(localForward, camera_roll);
            g_EditorCamera->orientation = g_EditorCamera->orientation * qRoll;
        }
    }
}

#else

void ProcessMouseBoard_Camera(void* window_handle, float dt) {}

#endif // HAS_IMGUI
