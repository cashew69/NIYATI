// Win32 headers
#include <windows.h>

// OpenGl Header File
#include <GL/glew.h>
// Must Header before GL.h
#include <GL/gl.h>

// OpenGl Libraries
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")
#include "../../engine/engine.h"
#include "../../platform_common.cpp" // <-- Include the shared header

// MACRO'S
#define WIN_WIDTH 800
#define WIN_HEIGHT 800

// Global function declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#if !defined(PROJECT_03) && !defined(PROJECT_04)
void updateCameraFromMouse(int delta_x, int delta_y) {}
void toggleWireframe(void) {}
#else
void updateCameraFromMouse(int delta_x, int delta_y);
void toggleWireframe(void);
#endif


// Global variable declarations - Windows specific

// Active Window Related Variable
BOOL gbActiveWindow = FALSE;

// Exit KeyPress Related
BOOL gbEscapeKeyIsPressed = FALSE;

// Variables related to FullScreen
BOOL gbFullScreen = FALSE;
HWND ghwnd = NULL;
DWORD dwStyle;
WINDOWPLACEMENT wpPrev;

// OpenGL related global variables
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

// Platform-specific FPS tracking
void print_fps() {
  static float last_time = 0.0f;
  static int frame_count = 0;

  float current_time = platformGetTime();
  frame_count++;

  float elapsed = current_time - last_time;
  if (elapsed >= 1.0f) {
    // printf("FPS: %.2f\n", frame_count / elapsed);
    frame_count = 0;
    last_time = current_time;
  }
}


// Platform Abstraction Implementations
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



// Entry-Point Function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpszCmdLine, int iCmdShow) {
  // Function declarations
  int initialize(void);
  void uninitialize(void);

  // Variable declarations
  WNDCLASSEX wndclass;
  HWND hwnd;
  MSG msg;
  TCHAR szAppName[] = TEXT("Nikhil Sathe's OpenGL Window");
  BOOL bDone = FALSE;

  // Create Log File
  gpFile = fopen(gszLogFileName, "w");
  if (gpFile == NULL) {
    MessageBox(NULL, TEXT("LOG FILE CREATION FAILED"), TEXT("ERROR"), MB_OK);
    exit(0);
  } else {
    fprintf(gpFile, "Program Started Successfully\n\n");
  }

  // WINDOW CLASS INITIALIZATION
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

  // REGISTRATION OF WINDOW CLASS
  RegisterClassEx(&wndclass);

  // Create Window
  hwnd = CreateWindowEx(
      WS_EX_APPWINDOW, szAppName, TEXT("Nikhil Sathe's OpenGL Window"),
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
      (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2,
      (GetSystemMetrics(SM_CYSCREEN) - WIN_HEIGHT) / 2, WIN_WIDTH, WIN_HEIGHT,
      NULL, NULL, hInstance, NULL);

  ghwnd = hwnd;

  // Show Window
  ShowWindow(hwnd, iCmdShow);
  UpdateWindow(hwnd);

  // Initialize
  int result = initialize();
  if (result != 0) {
    fprintf(gpFile, "Initialize Function Failed\n");
    DestroyWindow(hwnd);
    hwnd = NULL;
  } else {
    fprintf(gpFile, "Initialize() completed successfully\n");
  }

  // Set This Window As Foreground And Active Window
  SetForegroundWindow(hwnd);
  SetFocus(hwnd);

  // Game Loop
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
        if (gbEscapeKeyIsPressed) {
          bDone = TRUE;
        }

        // Continuous Keyboard Movement (GLFW-style)
        if (mouse_captured) {
          if (GetAsyncKeyState('W') & 0x8000) camera_pos[2] -= 1.0f;
          if (GetAsyncKeyState('S') & 0x8000) camera_pos[2] += 1.0f;
          if (GetAsyncKeyState('A') & 0x8000) camera_pos[0] -= 1.0f;
          if (GetAsyncKeyState('D') & 0x8000) camera_pos[0] += 1.0f;
          if (GetAsyncKeyState('Q') & 0x8000) camera_pos[1] -= 1.0f;
          if (GetAsyncKeyState('E') & 0x8000) camera_pos[1] += 1.0f;
        }

        // Render
        display();
        SwapBuffers(ghdc);

        // Update
        update();
        print_fps(); // Platform-specific FPS
      }
    }
  }

  uninitialize();
  return ((int)msg.wParam);
}

// Callback Function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  // Local function declaration
  void toggleFullScreen(void);
  void uninitialize(void);
  // void toggleWireframe(void);

  // Code
  switch (iMsg) {
  case WM_CREATE:
    ZeroMemory((void *)&wpPrev, sizeof(WINDOWPLACEMENT));
    wpPrev.length = sizeof(WINDOWPLACEMENT);
#if defined(PROJECT_03) || defined(PROJECT_04)
    // Capture mouse for project 03 & 04
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

      if (lastX != -1 && lastY != -1) {
        updateCameraFromMouse(xPos - lastX, yPos - lastY);
      }

      // Snap back to center
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
    switch (wParam) {
    case VK_ESCAPE:
    case VK_CAPITAL:
      gbEscapeKeyIsPressed = TRUE;
      break;
    default:
      break;
    }
    break;

  case WM_CHAR:
    switch (wParam) {
    case 'F':
    case 'f':
      toggleFullScreen();
      gbFullScreen = !gbFullScreen;
      break;
    case 'c':
    case 'C':
      mouse_captured = !mouse_captured;
      if (mouse_captured) SetMouseVisibility(false);
      else SetMouseVisibility(true);
      break;
    case 'l':
    case 'L':
      toggleWireframe();
      break;
    default:
      break;
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
  // Variable declarations
  MONITORINFO mi;

  // Code
  if (gbFullScreen == FALSE) {
    dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) {
      ZeroMemory((void *)&mi, sizeof(MONITORINFO));
      mi.cbSize = sizeof(MONITORINFO);
      if (GetWindowPlacement(ghwnd, &wpPrev) &&
          GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi)) {
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
      }
    }
    if (mouse_captured) SetMouseVisibility(false);
  } else {
    SetWindowPlacement(ghwnd, &wpPrev);
    SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
    SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER |
                     SWP_FRAMECHANGED);
    if (!mouse_captured) SetMouseVisibility(true);
  }
}



int initialize(void) {
  // Variable declarations
  PIXELFORMATDESCRIPTOR pfd;
  int iPixelFormatIndex = 0;
  GLenum glewResult;

  // Code
  // PIXELFORMATDESCRIPTOR initialization
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

  // Get DC
  ghdc = GetDC(ghwnd);
  if (ghdc == NULL) {
    fprintf(gpFile, "GetDC() Failed\n");
    return (-1);
  }

  // Get Matching Pixel Format Index
  iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
  if (iPixelFormatIndex == 0) {
    fprintf(gpFile, "ChoosePixelFormat() Failed\n");
    return (-2);
  }

  // Set the Pixel Format
  if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE) {
    fprintf(gpFile, "SetPixelFormat() Failed\n");
    return (-3);
  }

  // Create Rendering Context
  ghrc = wglCreateContext(ghdc);
  if (ghrc == NULL) {
    fprintf(gpFile, "wglCreateContext() Failed\n");
    return (-4);
  }

  // Make Rendering Context Current
  if (wglMakeCurrent(ghdc, ghrc) == FALSE) {
    fprintf(gpFile, "wglMakeCurrent() Failed\n");
    return (-5);
  }

  // Initialize GLEW
  glewResult = glewInit();
  if (glewResult != GLEW_OK) {
    fprintf(gpFile, "glewInit() Failed\n");
    return (-6);
  }

  // Initialize OpenGL resources (shared code)
  int result = initializeOpenGL();
  if (result != 0) {
    return result;
  }

  // Warmup Resize
  resize(WIN_WIDTH, WIN_HEIGHT);

  return (0);
}

void uninitialize(void) {
  // Code
  void toggleFullScreen(void);

  // If User is Exiting in FullScreen then restore to normal
  if (gbFullScreen == TRUE) {
    toggleFullScreen();
    gbFullScreen = FALSE;
  }

  cleanupOpenGL();

  // Make HDC as Current Context By Releasing Rendering Context
  if (wglGetCurrentContext() == ghrc) {
    wglMakeCurrent(NULL, NULL);
  }

  // Delete The Rendering Context
  if (ghrc) {
    wglDeleteContext(ghrc);
    ghrc = NULL;
  }

  // Release The DC
  if (ghdc) {
    ReleaseDC(ghwnd, ghdc);
    ghdc = NULL;
  }

  // Destroy Window
  if (ghwnd) {
    DestroyWindow(ghwnd);
    ghwnd = NULL;
  }

  // Closing the File
  if (gpFile) {
    fprintf(gpFile, "Program Terminated Successfully\n");
    fclose(gpFile);
    gpFile = NULL;
  }
}
