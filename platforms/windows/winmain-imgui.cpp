#include <windows.h>
#include <GL/glew.h>
#include <GL/gl.h>

#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")

#include "../../engine/engine.h"
#include "../../platform_common.cpp"

#ifdef HAS_IMGUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void NewFrameGUI(void) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RenderGUI(void) {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif

#if !defined(PROJECT_03) && !defined(PROJECT_04)
void UpdateGUI() {
#ifdef HAS_IMGUI
    NewFrameGUI();
#endif
}
#else
extern void UpdateGUI();
#endif

#define WIN_WIDTH 800
#define WIN_HEIGHT 800

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#if !defined(PROJECT_03) && !defined(PROJECT_04)
void updateCameraFromMouse(int delta_x, int delta_y) {}
void toggleWireframe(void) {}
#else
void updateCameraFromMouse(int delta_x, int delta_y);
void toggleWireframe(void);
#endif

BOOL gbActiveWindow = FALSE;
BOOL gbEscapeKeyIsPressed = FALSE;
BOOL gbFullScreen = FALSE;
HWND ghwnd = NULL;
DWORD dwStyle;
WINDOWPLACEMENT wpPrev;

HDC ghdc = NULL;
HGLRC ghrc = NULL;

// Mouse Tracking
static int lastX = -1, lastY = -1;

void SetMouseVisibility(bool visible) {
    static bool isVisible = true;
    if (isVisible == visible) return;
    ShowCursor(visible ? TRUE : FALSE);
    isVisible = visible;
}

void print_fps() {
    static float last_time = 0.0f;
    static int frame_count = 0;
    float current_time = platformGetTime();
    frame_count++;
    float elapsed = current_time - last_time;
    if (elapsed >= 1.0f) {
        frame_count = 0;
        last_time = current_time;
    }
}

float platformGetTime(void) {
    static LARGE_INTEGER frequency;
    static BOOL frequencyAvailable = QueryPerformanceFrequency(&frequency);
    if (!frequencyAvailable) return 0.0f;
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (float)currentTime.QuadPart / (float)frequency.QuadPart;
}

void platformGetFramebufferSize(int* width, int* height) {
    RECT rect;
    if (GetClientRect(ghwnd, &rect)) {
        *width = rect.right - rect.left;
        *height = rect.bottom - rect.top;
    } else {
        *width = WIN_WIDTH;
        *height = WIN_HEIGHT;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow) {
    int initialize(void);
    void uninitialize(void);

    WNDCLASSEX wndclass;
    HWND hwnd;
    MSG msg;
    TCHAR szAppName[] = TEXT("NIYATI Engine");
    BOOL bDone = FALSE;

    gpFile = fopen(gszLogFileName, "w");
    if (gpFile == NULL) {
        MessageBox(NULL, TEXT("LOG FILE CREATION FAILED"), TEXT("ERROR"), MB_OK);
        exit(0);
    }

    wndclass.cbSize = sizeof(WNDCLASSEX);
    wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.lpszClassName = szAppName;
    wndclass.lpszMenuName = NULL;
    wndclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wndclass);

    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW, szAppName, szAppName,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - WIN_HEIGHT) / 2, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL);

    ghwnd = hwnd;

    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    if (initialize() != 0) {
        DestroyWindow(hwnd);
        hwnd = NULL;
    }

    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    while (bDone == FALSE) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                bDone = TRUE;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            if (gbActiveWindow == TRUE) {
                if (gbEscapeKeyIsPressed) bDone = TRUE;

                // Continuous Keyboard Movement (GLFW-style)
                if (mouse_captured) {
                    if (GetAsyncKeyState('W') & 0x8000) camera_pos[2] -= 1.0f;
                    if (GetAsyncKeyState('S') & 0x8000) camera_pos[2] += 1.0f;
                    if (GetAsyncKeyState('A') & 0x8000) camera_pos[0] -= 1.0f;
                    if (GetAsyncKeyState('D') & 0x8000) camera_pos[0] += 1.0f;
                    if (GetAsyncKeyState('Q') & 0x8000) camera_pos[1] -= 1.0f;
                    if (GetAsyncKeyState('E') & 0x8000) camera_pos[1] += 1.0f;
                }

#ifdef HAS_IMGUI
                UpdateGUI();
#endif
                display();
#ifdef HAS_IMGUI
                RenderGUI();
#endif
                SwapBuffers(ghdc);
                update();
                print_fps();
            }
        }
    }
    uninitialize();
    return ((int)msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
    void toggleFullScreen(void);
    void uninitialize(void);

#ifdef HAS_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam))
        return true;
#endif

    switch (iMsg) {
    case WM_CREATE:
        ZeroMemory((void *)&wpPrev, sizeof(WINDOWPLACEMENT));
        wpPrev.length = sizeof(WINDOWPLACEMENT);
#if defined(PROJECT_03) || defined(PROJECT_04)
        mouse_captured = true;
        SetMouseVisibility(false);
#endif
        break;
    case WM_SETFOCUS:
        gbActiveWindow = TRUE;
#if defined(PROJECT_03) || defined(PROJECT_04)
        if (mouse_captured) SetMouseVisibility(false);
#endif
        break;
    case WM_KILLFOCUS:
        gbActiveWindow = FALSE;
#if defined(PROJECT_03) || defined(PROJECT_04)
        SetMouseVisibility(true);
#endif
        break;
    case WM_ERASEBKGND:
        return (0);

    case WM_RBUTTONDOWN:
        mouse_captured = !mouse_captured;
        if (mouse_captured) {
            SetMouseVisibility(false);
            lastX = -1; lastY = -1; // Reset tracking
        } else {
            SetMouseVisibility(true);
        }
        break;

    case WM_MOUSEMOVE:
#if defined(PROJECT_03) || defined(PROJECT_04)
        if (mouse_captured && gbActiveWindow) {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            if (lastX != -1 && lastY != -1) updateCameraFromMouse(xPos - lastX, yPos - lastY);
            RECT rect;
            GetClientRect(hwnd, &rect);
            POINT pt = {(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
            ClientToScreen(hwnd, &pt);
            SetCursorPos(pt.x, pt.y);
            lastX = (rect.right - rect.left) / 2;
            lastY = (rect.bottom - rect.top) / 2;
        }
#endif
        break;
    case WM_SIZE:
        resize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == VK_CAPITAL) gbEscapeKeyIsPressed = TRUE;
        break;
    case WM_CHAR:
        switch (wParam) {
        case 'F': case 'f':
            toggleFullScreen();
            gbFullScreen = !gbFullScreen;
            break;
        case 'c': case 'C':
            mouse_captured = !mouse_captured;
            if (mouse_captured) SetMouseVisibility(false);
            else SetMouseVisibility(true);
            break;
        case 'l': case 'L': toggleWireframe(); break;
        }
        break;
    case WM_CLOSE:
        uninitialize();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return (DefWindowProc(hwnd, iMsg, wParam, lParam));
    }
    return 0;
}

void toggleFullScreen(void) {
    MONITORINFO mi;
    if (gbFullScreen == FALSE) {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW) {
            ZeroMemory((void *)&mi, sizeof(MONITORINFO));
            mi.cbSize = sizeof(MONITORINFO);
            if (GetWindowPlacement(ghwnd, &wpPrev) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        if (mouse_captured) SetMouseVisibility(false);
    } else {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        if (!mouse_captured) SetMouseVisibility(true);
    }
}

int initialize(void) {
    PIXELFORMATDESCRIPTOR pfd;
    int iPixelFormatIndex = 0;

    ZeroMemory((void *)&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cBlueBits = 8;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 32;

    ghdc = GetDC(ghwnd);
    if (ghdc == NULL) return (-1);

    iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
    if (iPixelFormatIndex == 0) return (-2);

    if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE) return (-3);

    ghrc = wglCreateContext(ghdc);
    if (ghrc == NULL) return (-4);

    if (wglMakeCurrent(ghdc, ghrc) == FALSE) return (-5);

    if (glewInit() != GLEW_OK) return (-6);

    if (initializeOpenGL() != 0) return -7;

#ifdef HAS_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(ghwnd);
    ImGui_ImplOpenGL3_Init("#version 130");
#endif

    resize(WIN_WIDTH, WIN_HEIGHT);
    return (0);
}

void uninitialize(void) {
    if (gbFullScreen == TRUE) {
        toggleFullScreen();
        gbFullScreen = FALSE;
    }

    cleanupOpenGL();

#ifdef HAS_IMGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    if (wglGetCurrentContext() == ghrc) wglMakeCurrent(NULL, NULL);
    if (ghrc) { wglDeleteContext(ghrc); ghrc = NULL; }
    if (ghdc) { ReleaseDC(ghwnd, ghdc); ghdc = NULL; }
    if (ghwnd) { DestroyWindow(ghwnd); ghwnd = NULL; }

    if (gpFile) {
        fclose(gpFile);
        gpFile = NULL;
    }
}
