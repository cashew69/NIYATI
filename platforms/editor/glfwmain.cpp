#define Bool int
#define True 1
#define False 0

// OpenGL related header files.
#include <GL/glew.h> // GLEW must be first
#include <GL/gl.h>   // OpenGL

#include <GLFW/glfw3.h>

// ImGui

#include "engine/engine.h"
#include "logger.h"
#include "platform_common.cpp"


#include "engine/dependancies/imgui/imgui.h"

// Shared ImGui Setup implementation
#include "engine/editor/imgui_setup.cpp"



// Default stub — project 03 overrides this by defining UpdateGUI() in gui.cpp
// which is included before this point via platform_common.cpp → project.cpp → gui.cpp.
#if !defined(PROJECT_TEMPLATE)

void UpdateGUI() {
    NewFrameGUI();
    // No panels for simple projects
}
// Projects 01/02 don't use mouse look or wireframe — empty stubs so platform compiles
void toggleWireframe(void)           {}
#endif



// Macros
#define winwidth 800
#define winheight 600

// GLFW-specific Global Variables
GLFWwindow *window = NULL;
bool bFullscreen = false;

// Platform abstraction implementations
float platformGetTime(void) {
  return (float)glfwGetTime();
}

void platformGetFramebufferSize(int* width, int* height) {
  if (window) {
    glfwGetFramebufferSize(window, width, height);
  } else {
    *width = 800;
    *height = 600;
  }
}

// Mouse Input (GLFW-specific tracking)
double last_mouse_x = 0.0;
double last_mouse_y = 0.0;
bool first_mouse = true;

// Saved window state for toggling fullscreen
int saved_xpos, saved_ypos, saved_width, saved_height;

void print_fps() {
  static double last_time = 0.0;
  static int frame_count = 0;

  double current_time = glfwGetTime();
  frame_count++;

  double elapsed = current_time - last_time;
  if (elapsed >= 1.0) {
    frame_count = 0;
    last_time = current_time;
  }
}

int main(void) {
  // Function Declarations
  //setenv("GLFW_PLATFORM", "x11", 1);
  int initialize(void);
  void framebuffer_size_callback(GLFWwindow * window, int width, int height);
  void mouse_callback(GLFWwindow * window, double xpos, double ypos);
  void key_callback(GLFWwindow * window, int key, int scancode, int action,
                    int mods);
  void mouse_button_callback(GLFWwindow * window, int button, int action,
                             int mods);
  void toggleFullScreen(void);
  void uninitialize(void);

  // File Create
  Logger_Init(gszLogFileName);

  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  // Initialize GLFW
  if (!glfwInit()) {
    LOG_E("glfwInit() failed");
    return -1;
  }


  // Set Window Hints (Context Version 4.6 Core)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

  // Create Window
  window = glfwCreateWindow(winwidth, winheight, "NIYATI ENGINE",
                            NULL, NULL);
  if (!window) {
    LOG_E("glfwCreateWindow() failed");
    glfwTerminate();
    return -1;
  }


  // Make Context Current
  glfwMakeContextCurrent(window);

  // Initialize GLEW
  GLenum glew_error = glewInit();
  if (glew_error != GLEW_OK) {
    LOG_E("glewInit() failed: %s", glewGetErrorString(glew_error));
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }


  // Setup Callbacks
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);

  // Center the window
  const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  glfwSetWindowPos(window, (mode->width - winwidth) / 2,
                   (mode->height - winheight) / 2);

  // Initialize ImGui
  InitGUI(window);

  // Initialize User Data
  int iResult = initialize();
  if (iResult != 0) {
    LOG_E("initialize() FAILED");
    uninitialize();
    exit(EXIT_FAILURE);
  } else {
  LOG_I("initialize() SUCCEEDED");
  }

  float lastFrameTime = (float)glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    // Poll Events
    glfwPollEvents();

    // Update and Render GUI
    UpdateGUI();

    // Calculate Delta Time
    float currentFrameTime = (float)glfwGetTime();
    float deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    // Handle Camera Input (from engine/utils/camera_utils)
    HandleCameraInput(window, deltaTime);


    // Render
    glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
    display();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Render ImGui
    RenderGUI();


    glfwSwapBuffers(window); // Platform-specific buffer swap

    update();
    print_fps(); // Platform-specific FPS
  }

  uninitialize();
  return (0);
}

int initialize(void) {
  // Initialize OpenGL resources (shared code)
  int result = initializeOpenGL();
  if (result != 0) {
    return result;
  }

  // Warmup Resize
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  resize(width, height);

  return (0);
}

void toggleFullScreen(void) {
  if (!bFullscreen) {
    // Switch to fullscreen
    glfwGetWindowPos(window, &saved_xpos, &saved_ypos);
    glfwGetWindowSize(window, &saved_width, &saved_height);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                         mode->refreshRate);
    bFullscreen = true;
  } else {
    // Switch back to windowed
    glfwSetWindowMonitor(window, NULL, saved_xpos, saved_ypos, saved_width,
                         saved_height, 0);
    bFullscreen = false;
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  resize(width, height);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (action == GLFW_PRESS) {
    switch (key) {
    case GLFW_KEY_CAPS_LOCK:
      glfwSetWindowShouldClose(window, GLFW_TRUE);
      break;
    case GLFW_KEY_F:
      toggleFullScreen();
      break;
    case GLFW_KEY_L:
      // toggleWireframe();
      break;
    default:
      break;
    }
  }
}

void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mouse_captured = !mouse_captured;
    if (mouse_captured) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      first_mouse = true;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  if (!mouse_captured)
    return;

  if (first_mouse) {
    last_mouse_x = xpos;
    last_mouse_y = ypos;
    first_mouse = false;
  }

  mouse_x = (int)(xpos - last_mouse_x);
  mouse_y = (int)(ypos - last_mouse_y);

  last_mouse_x = xpos;
  last_mouse_y = ypos;
}


void uninitialize(void) {
  // Cleanup ImGui
  ShutdownGUI();

  cleanupOpenGL();

  // Cleanup GLFW
  Logger_Cleanup();

  if (window) {
    glfwDestroyWindow(window);
    window = NULL;
  }

  glfwTerminate();
}
