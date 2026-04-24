#include <GL/glew.h>
#include "engine/dependancies/imgui/imgui.h"
#include "engine/dependancies/imgui/imgui_impl_opengl3.h"

#ifdef _WIN32
#include <windows.h>
#include "engine/dependancies/imgui/imgui_impl_win32.h"
#else
#include <GLFW/glfw3.h>
#include "engine/dependancies/imgui/imgui_impl_glfw.h"
#endif

// Viewport FBO Globals
GLuint viewportFBO = 0;
GLuint viewportTexture = 0;
GLuint viewportDepthRBO = 0;
int viewportWidth = 0, viewportHeight = 0;


void updateViewportFBO(int width, int height) {
    if (width == viewportWidth && height == viewportHeight) return;
    if (width <= 0 || height <= 0) return;

    if (viewportFBO) {
        glDeleteFramebuffers(1, &viewportFBO);
        glDeleteTextures(1, &viewportTexture);
        glDeleteRenderbuffers(1, &viewportDepthRBO);
    }

    glGenFramebuffers(1, &viewportFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);

    glGenTextures(1, &viewportTexture);
    glBindTexture(GL_TEXTURE_2D, viewportTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTexture, 0);

    glGenRenderbuffers(1, &viewportDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, viewportDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, viewportDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    viewportWidth = width;
    viewportHeight = height;
}

void InitGUI(void* window_handle) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
#ifdef _WIN32
    ImGui_ImplWin32_Init((HWND)window_handle);
    ImGui_ImplOpenGL3_Init("#version 130");
#else
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window_handle, true);
    ImGui_ImplOpenGL3_Init("#version 460");
#endif
}

void NewFrameGUI() {
    ImGui_ImplOpenGL3_NewFrame();
#ifdef _WIN32
    ImGui_ImplWin32_NewFrame();
#else
    ImGui_ImplGlfw_NewFrame();
#endif
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("MainEditorDockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(2);
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Game Viewport");
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        updateViewportFBO((int)viewportPanelSize.x, (int)viewportPanelSize.y);
        ImGui::Image((void*)(intptr_t)viewportTexture, viewportPanelSize, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();
        ImGui::PopStyleVar();

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    extern GLFWwindow* window;
                    if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

void RenderGUI() {
#ifndef _WIN32
    extern GLFWwindow* window;
    if (window) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
    }
#endif
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ShutdownGUI() {
    ImGui_ImplOpenGL3_Shutdown();
#ifdef _WIN32
    ImGui_ImplWin32_Shutdown();
#else
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}
