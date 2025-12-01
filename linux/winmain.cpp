#include "wincommon.h"
#include "common.h"
#include "platform_common.h"  // <-- Include the shared header

// MACRO'S
#define WIN_WIDTH 800
#define WIN_HEIGHT 800

// Global function declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

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

// Platform-specific FPS tracking
void print_fps() {
    static double last_time = 0.0;
    static int frame_count = 0;
    
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    double current_time = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    
    frame_count++;
    
    double elapsed = current_time - last_time;
    if (elapsed >= 1.0) {
        // printf("FPS: %.2f\n", fps);  // Uncomment if you want console output
        frame_count = 0;
        last_time = current_time;
    }
}

// Entry-Point Function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
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
    if (gpFile == NULL)
    {
        MessageBox(NULL, TEXT("LOG FILE CREATION FAILED"), TEXT("ERROR"), MB_OK);
        exit(0);
    }
    else
    {
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
        WS_EX_APPWINDOW,
        szAppName,
        TEXT("Nikhil Sathe's OpenGL Window"),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - WIN_HEIGHT) / 2,
        WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    ghwnd = hwnd;

    // Show Window
    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    // Initialize
    int result = initialize();
    if (result != 0)
    {
        fprintf(gpFile, "Initialize Function Failed\n");
        DestroyWindow(hwnd);
        hwnd = NULL;
    }
    else
    {
        fprintf(gpFile, "Initialize() completed successfully\n");
    }

    // Set This Window As Foreground And Active Window
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // Game Loop
    while (bDone == FALSE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                bDone = TRUE;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            if (gbActiveWindow == TRUE)
            {
                if (gbEscapeKeyIsPressed)
                {
                    bDone = TRUE;
                }

                // Render
                display();       
                SwapBuffers(ghdc);

                // Update
                update();        
                print_fps();     // Platform-specific FPS
            }
        }
    }

    uninitialize();
    return((int)msg.wParam);
}

// Callback Function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    // Local function declaration
    void toggleFullScreen(void);
    void uninitialize(void);

    // Code
    switch (iMsg)
    {
    case WM_CREATE:
        ZeroMemory((void*)&wpPrev, sizeof(WINDOWPLACEMENT));
        wpPrev.length = sizeof(WINDOWPLACEMENT);
        break;

    case WM_SETFOCUS:
        gbActiveWindow = TRUE;
        break;

    case WM_KILLFOCUS:
        gbActiveWindow = FALSE;
        break;

    case WM_ERASEBKGND:
        return(0);

    case WM_SIZE:
        resize(LOWORD(lParam), HIWORD(lParam));  
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            gbEscapeKeyIsPressed = TRUE;
            break;
        default:
            break;
        }
        break;

    case WM_CHAR:
        switch (wParam)
        {
        case 'F':
        case 'f':
            toggleFullScreen();
            gbFullScreen = !gbFullScreen;
            break;
        case 'w':
        case 'W':
            camz -= 1.0f;
            break;
        case 's':
        case 'S':
            camz += 1.0f;
            break;
        case 'a':
        case 'A':
            eyex -= 1.0f;
            break;
        case 'd':
        case 'D':
            eyex += 1.0f;
            break;
        case 'q':
        case 'Q':
            camy -= 1.0f;
            break;
        case 'e':
        case 'E':
            camy += 1.0f;
            break;
        case 'c':
        case 'C':
            mouse_captured = !mouse_captured;
            fprintf(gpFile, "Mouse %s\n", mouse_captured ? "captured" : "released");
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
        return(DefWindowProc(hwnd, iMsg, wParam, lParam));
    }

    return 0;
}

void toggleFullScreen(void)
{
    // Variable declarations
    MONITORINFO mi;

    // Code
    if (gbFullScreen == FALSE)
    {
        dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW)
        {
            ZeroMemory((void*)&mi, sizeof(MONITORINFO));
            mi.cbSize = sizeof(MONITORINFO);
            if (GetWindowPlacement(ghwnd, &wpPrev) && 
                GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
            {
                SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(ghwnd, HWND_TOP, 
                            mi.rcMonitor.left, mi.rcMonitor.top, 
                            mi.rcMonitor.right - mi.rcMonitor.left, 
                            mi.rcMonitor.bottom - mi.rcMonitor.top, 
                            SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
        ShowCursor(FALSE);
    }
    else
    {
        SetWindowPlacement(ghwnd, &wpPrev);
        SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowCursor(TRUE);
    }
}

int initialize(void)
{
    // Variable declarations
    PIXELFORMATDESCRIPTOR pfd;
    int iPixelFormatIndex = 0;
    GLenum glewResult;

    // Code
    // PIXELFORMATDESCRIPTOR initialization
    ZeroMemory((void*)&pfd, sizeof(PIXELFORMATDESCRIPTOR));
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
    if (ghdc == NULL)
    {
        fprintf(gpFile, "GetDC() Failed\n");
        return(-1);
    }

    // Get Matching Pixel Format Index
    iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
    if (iPixelFormatIndex == 0)
    {
        fprintf(gpFile, "ChoosePixelFormat() Failed\n");
        return(-2);
    }

    // Set the Pixel Format
    if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == FALSE)
    {
        fprintf(gpFile, "SetPixelFormat() Failed\n");
        return(-3);
    }

    // Create Rendering Context
    ghrc = wglCreateContext(ghdc);
    if (ghrc == NULL)
    {
        fprintf(gpFile, "wglCreateContext() Failed\n");
        return(-4);
    }

    // Make Rendering Context Current
    if (wglMakeCurrent(ghdc, ghrc) == FALSE)
    {
        fprintf(gpFile, "wglMakeCurrent() Failed\n");
        return(-5);
    }

    // Initialize GLEW
    glewResult = glewInit();
    if (glewResult != GLEW_OK)
    {
        fprintf(gpFile, "glewInit() Failed\n");
        return(-6);
    }

    // Initialize OpenGL resources (shared code)
    int result = initializeOpenGL();  
    if (result != 0)
    {
        return result;
    }

    // Warmup Resize
    resize(WIN_WIDTH, WIN_HEIGHT);  

    return(0);
}

void uninitialize(void)
{
    // Code
    void toggleFullScreen(void);

    // If User is Exiting in FullScreen then restore to normal
    if (gbFullScreen == TRUE)
    {
        toggleFullScreen();
        gbFullScreen = FALSE;
    }

    cleanupOpenGL();  

    // Make HDC as Current Context By Releasing Rendering Context
    if (wglGetCurrentContext() == ghrc)
    {
        wglMakeCurrent(NULL, NULL);
    }

    // Delete The Rendering Context
    if (ghrc)
    {
        wglDeleteContext(ghrc);
        ghrc = NULL;
    }

    // Release The DC
    if (ghdc)
    {
        ReleaseDC(ghwnd, ghdc);
        ghdc = NULL;
    }

    // Destroy Window
    if (ghwnd)
    {
        DestroyWindow(ghwnd);
        ghwnd = NULL;
    }

    // Closing the File
    if (gpFile)
    {
        fprintf(gpFile, "Program Terminated Successfully\n");
        fclose(gpFile);
        gpFile = NULL;
    }
}