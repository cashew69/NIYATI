typedef int Bool;
#define True 1
#define False 0

// GLEW
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "common.h"
#include "platform_common.cpp"

// Macros
#define winwidth 800
#define winheight 600

// GLFW-specific Global Variables
GLFWwindow* window = NULL;
bool bFullscreen = false;

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
        // FPS is now shown in ImGui, so we don't need to print
        frame_count = 0;
        last_time = current_time;
    }
}

int main(void)
{
    // Function Declarations
    int initialize(void);
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    void toggleFullScreen(void);
    void uninitialize(void);

    // File Create
    gpFile = fopen(gszLogFileName, "w");
    if (gpFile == NULL)
    {
        printf("Log file creation failed...!!!\n");
        exit(0);
    }
    else
    {
        setvbuf(gpFile, NULL, _IONBF, 0);
        fprintf(gpFile, "Program started successfully...!!!\n");
        fprintf(gpFile, "HELLO WORLD !!!\n");
    }

    // Initialize GLFW
    if (!glfwInit())
    {
        fprintf(gpFile, "glfwInit() failed\n");
        return -1;
    }

    // Set Window Hints (Context Version 4.6 Core)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // Create Window
    window = glfwCreateWindow(winwidth, winheight, "Nikhil Sathe's GLFW Window", NULL, NULL);
    if (!window)
    {
        fprintf(gpFile, "glfwCreateWindow() failed\n");
        glfwTerminate();
        return -1;
    }

    // Make Context Current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    GLenum glew_error = glewInit();
    if (glew_error != GLEW_OK)
    {
        fprintf(gpFile, "glewInit() failed: %s\n", glewGetErrorString(glew_error));
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
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (mode->width - winwidth) / 2, (mode->height - winheight) / 2);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Setup ImGui Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Initialize User Data
    int iResult = initialize();
    if (iResult != 0)
    {
        fprintf(gpFile, "initialize() FAILED\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(gpFile, "initialize() SUCCEEDED\n");
    }

    // GAMELOOP
    while (!glfwWindowShouldClose(window))
    {
        // Poll Events
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui Debug Window
        {
            ImGui::Begin("Debug Controls"); 
            ImGui::Text("Application Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Separator();
            ImGui::Text("Camera Position");
            ImGui::SliderFloat("Cam X", &eyex, -100.0f, 100.0f);
            ImGui::SliderFloat("Cam Y", &camy, -100.0f, 100.0f);
            ImGui::SliderFloat("Cam Z", &camz, -500.0f, 500.0f);
            ImGui::Separator();
            if (ImGui::Button("Toggle Wireframe"))
            {
                toggleWireframe();  
            }
            ImGui::End();
        }

        // Handle Keyboard Movement (Continuous polling)
        if (mouse_captured)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camz -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camz += 1.0f;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) eyex -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) eyex += 1.0f;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camy -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camy += 1.0f;
        }

        // Render
        display();  
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);  // Platform-specific buffer swap
        
        update();      
        print_fps();   // Platform-specific FPS
    }

    uninitialize();
    return(0);
}

int initialize(void)
{
    // Initialize OpenGL resources (shared code)
    int result = initializeOpenGL();  
    if (result != 0)
    {
        return result;
    }

    // Warmup Resize
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    resize(width, height);  

    return(0);
}

void toggleFullScreen(void)
{
    if (!bFullscreen)
    {
        // Switch to fullscreen
        glfwGetWindowPos(window, &saved_xpos, &saved_ypos);
        glfwGetWindowSize(window, &saved_width, &saved_height);
        
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        bFullscreen = true;
    }
    else
    {
        // Switch back to windowed
        glfwSetWindowMonitor(window, NULL, saved_xpos, saved_ypos, saved_width, saved_height, 0);
        bFullscreen = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    resize(width, height);  
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_F:
                toggleFullScreen();
                break;
            case GLFW_KEY_L:
                toggleWireframe();  
                break;
            default:
                break;
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        mouse_captured = !mouse_captured;
        if (mouse_captured)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            first_mouse = true;
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!mouse_captured) return;

    if (first_mouse)
    {
        last_mouse_x = xpos;
        last_mouse_y = ypos;
        first_mouse = false;
    }

    double delta_x = xpos - last_mouse_x;
    double delta_y = ypos - last_mouse_y;

    last_mouse_x = xpos;
    last_mouse_y = ypos;

    updateCameraFromMouse((int)delta_x, (int)delta_y);  
}

void uninitialize(void)
{
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    cleanupOpenGL();  

    // Cleanup GLFW
    if (window)
    {
        glfwDestroyWindow(window);
        window = NULL;
    }

    glfwTerminate();

    if (gpFile)
    {
        fprintf(gpFile, "Program terminated successfully...!!!");
        fclose(gpFile);
        gpFile = NULL;
    }
}