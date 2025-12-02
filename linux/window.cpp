
#include  "lincommons.h"

// OpenGL related header files. 
#include <GL/glew.h>  // GLEW must be first
#include <GL/gl.h>    // OpenGL
#include <GL/glx.h>

#include "common.h"
#include "platform_common.cpp"

// Macros
#define winwidth 800
#define winheight 600

// Global Variables - X11 specific
Display *gpDisplay = NULL;
XVisualInfo *visualInfo = NULL;
Window window;
Colormap colormap;
bool bFullscreen = false;
Bool bActiveWindow = False;

typedef GLXContext (*glXCreateContextAttribsARBProc)
(Display *, GLXFBConfig, GLXContext, Bool, const int *);

glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
GLXFBConfig glxFBConfig;
GLXContext glxContext = NULL;

// Platform-specific FPS tracking
void print_fps() {
    static double last_time = 0.0;
    static int frame_count = 0;
    static double fps = 0.0;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double current_time = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    
    frame_count++;
    
    double elapsed = current_time - last_time;
    if (elapsed >= 1.0) {
        fps = frame_count / elapsed;
        printf("FPS: %.2f\n", fps);
        
        frame_count = 0;
        last_time = current_time;
    }
}

int main(void)
{
    // Function Declarations
    int initialize(void);
    void toggleFullScreen(void);
    void uninitialize(void);

    // Variable Declarations
    int defaultDepth;
    Atom windowManagerDeleteAtom;
    XEvent event;
    Screen * screen = NULL;
    int screenWidth , screenHeight;
    KeySym keysym;
    char keys[52];

    GLXFBConfig *pGLXFBConfig;
    GLXFBConfig bestFBConfig;
    XVisualInfo *pXVisualInfo;
    int iNumFBConfigs = 0;

    int frameBufferAttributes[] = 
    {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8, 
        GLX_GREEN_SIZE, 8, 
        GLX_BLUE_SIZE, 8, 
        GLX_ALPHA_SIZE, 8, 
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        None
    };

    Bool bDone = False;

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

    // Open connection with Xserver
    gpDisplay = XOpenDisplay(NULL);
    if (gpDisplay == NULL)
    {
        printf("XOpenDisplay failed\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    int defaultScreen = XDefaultScreen(gpDisplay);
    defaultDepth = XDefaultDepth(gpDisplay, defaultScreen); 

    // Choose best framebuffer config
    pGLXFBConfig = glXChooseFBConfig(gpDisplay, defaultScreen, frameBufferAttributes, &iNumFBConfigs);
    if(pGLXFBConfig == NULL)
    {
        fprintf(gpFile, "glXChooseFBConfig failed\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }
    fprintf(gpFile, "glXChooseFBConfig returned %d configs\n", iNumFBConfigs);

    int indexOfBestFBConfig = -1;
    int bestNumberOfSamples = -1;

    for (int i = 0; i < iNumFBConfigs; i++)
    {
        pXVisualInfo = glXGetVisualFromFBConfig(gpDisplay, pGLXFBConfig[i]);
        if (pXVisualInfo)
        {
            int sampleBuffers, samples;
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLE_BUFFERS, &sampleBuffers);
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLES, &samples);

            if (samples > bestNumberOfSamples)
            {
                bestNumberOfSamples = samples;
                indexOfBestFBConfig = i;
            }
            XFree(pXVisualInfo);
        }
    }

    bestFBConfig = pGLXFBConfig[indexOfBestFBConfig];
    glxFBConfig = bestFBConfig;
    XFree(pGLXFBConfig);

    visualInfo = glXGetVisualFromFBConfig(gpDisplay, glxFBConfig);

    // Set Window Attributes
    XSetWindowAttributes windowAttributes;
    memset((void*) &windowAttributes, 0, sizeof(XSetWindowAttributes));

    windowAttributes.border_pixel = 0;
    windowAttributes.background_pixmap = 0;
    windowAttributes.background_pixel = XBlackPixel(gpDisplay, visualInfo->screen);
    windowAttributes.event_mask = KeyPressMask | ButtonPressMask | PointerMotionMask | 
                                   FocusChangeMask | StructureNotifyMask | ExposureMask;
    Window root = XRootWindow(gpDisplay, visualInfo->screen);
    windowAttributes.colormap = XCreateColormap(gpDisplay, root, visualInfo->visual, AllocNone);
    colormap = windowAttributes.colormap;

    // Create Window
    window = XCreateWindow(
            gpDisplay, root,
            0, 0, winwidth, winheight, 2,
            visualInfo->depth, InputOutput, visualInfo->visual,
            CWBorderPixel | CWBackPixel | CWEventMask | CWColormap, 
            &windowAttributes);

    if (!window)
    {
        printf("XCreateWindow failed\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    windowManagerDeleteAtom = XInternAtom(gpDisplay, "WM_DELETE_WINDOW", True); 
    XSetWMProtocols(gpDisplay, window, &windowManagerDeleteAtom, 1);
    XStoreName(gpDisplay, window, "Nikhil Sathe's XWINDOW");
    XMapWindow(gpDisplay, window);

    // Center window
    screen = XScreenOfDisplay(gpDisplay, visualInfo->screen);
    screenWidth = XWidthOfScreen(screen);
    screenHeight = XHeightOfScreen(screen);
    XMoveWindow(gpDisplay, window, (screenWidth/2 - winwidth/2), (screenHeight/2 - winheight/2));

    // Initialize
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
    while(bDone == False)
    {
        while(XPending(gpDisplay))
        {
            XNextEvent(gpDisplay, &event);
            switch(event.type)
            {
                case MapNotify:
                    break;
                case FocusIn:
                    bActiveWindow = True;
                    break;
                case FocusOut:
                    bActiveWindow = False;
                    break;
                case ConfigureNotify:
                    resize(event.xconfigure.width, event.xconfigure.height);
                    break;
                case MotionNotify:
                    if (mouse_captured)
                    {
                        int delta_x = event.xmotion.x - mouse_x;
                        int delta_y = event.xmotion.y - mouse_y;
                        
                        updateCameraFromMouse(delta_x, delta_y);
                        
                        mouse_x = event.xmotion.x;
                        mouse_y = event.xmotion.y;
                    }
                    break;
                case KeyPress:
                    keysym = XkbKeycodeToKeysym(gpDisplay, event.xkey.keycode, 0, 0);
                    switch(keysym)
                    {
                        case XK_Escape:
                            bDone = True;
                            break;
                        case XK_f:
                        case XK_F:
                            toggleFullScreen();
                            bFullscreen = !bFullscreen;
                            break;
                        case XK_w:
                        case XK_W:
                            camera_pos[2] -= 1.0f;
                            handleShipInput('W', true);
                            break;
                        case XK_s:
                        case XK_S:
                            camera_pos[2] += 1.0f;
                            handleShipInput('S', true);
                            break;
                        case XK_a:
                        case XK_A:
                            camera_pos[0] -= 1.0f;
                            handleShipInput('A', true);
                            break;
                        case XK_d:
                        case XK_D:
                            camera_pos[0] += 1.0f;
                            handleShipInput('D', true);
                            break;
                        case XK_q:
                            camera_pos[1] -= 1.0f;
                            break;
                        case XK_e:
                            camera_pos[1] += 1.0f;
                            break;
                        // Ship Pitch/Yaw controls
                        case XK_8:
                        case XK_KP_8:
                            handleShipInput('8', true);
                            break;
                        case XK_5:
                        case XK_KP_5:
                            handleShipInput('5', true);
                            break;
                        case XK_4:
                        case XK_KP_4:
                            handleShipInput('4', true);
                            break;
                        case XK_6:
                        case XK_KP_6:
                            handleShipInput('6', true);
                            break;
                        case XK_l:
                        case XK_L:
                            toggleWireframe();
                            break;
                        case XK_c:
                        case XK_C:
                            mouse_captured = !mouse_captured;
                            fprintf(gpFile, "Mouse %s\n", mouse_captured ? "captured" : "released");
                            break;
                        default:
                            break;
                    }
                    break;
                case 33: // ClientMessage (WM_DELETE_WINDOW)
                    bDone = True;
                    break;
            }
        }

        if (bActiveWindow == True)
        {
            display();
            glXSwapBuffers(gpDisplay, window);
            
            update();
            print_fps();
        }
    }

    uninitialize();
    return 0;
}

int initialize(void)
{
    GLenum glewResult;

    // Get GLX extension for creating modern contexts
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    // Create OpenGL 4.6 Core context
    if (glXCreateContextAttribsARB)
    {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 6,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };
        glxContext = glXCreateContextAttribsARB(gpDisplay, glxFBConfig, 0, True, attribs);
    }
    
    if (!glxContext)
    {
        fprintf(gpFile, "Failed to create GLX context\n");
        return -1;
    }
    
    glXMakeCurrent(gpDisplay, window, glxContext);

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

    // Warmup resize
    resize(winwidth, winheight);

    return 0;
}

void toggleFullScreen(void)
{
    Atom windowManagerNormalStateAtom = XInternAtom(gpDisplay, "_NET_WM_STATE", False);
    Atom windowManagerFullscreenStateAtom = XInternAtom(gpDisplay, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent fsevent;
    memset((void*)&fsevent, 0, sizeof(XEvent));
    fsevent.type = ClientMessage;
    fsevent.xclient.window = window;
    fsevent.xclient.message_type = windowManagerNormalStateAtom;
    fsevent.xclient.format = 32;
    fsevent.xclient.data.l[0] = bFullscreen ? 0 : 1;
    fsevent.xclient.data.l[1] = windowManagerFullscreenStateAtom;

    XSendEvent(gpDisplay, XRootWindow(gpDisplay, visualInfo->screen), False,
            SubstructureNotifyMask, &fsevent);
}

void uninitialize(void)
{
    cleanupOpenGL();

    if (window)
    {
        XDestroyWindow(gpDisplay, window);
    }

    if (colormap)
    {
        XFreeColormap(gpDisplay, colormap);
    }

    GLXContext currentContext = glXGetCurrentContext();
    if (currentContext && currentContext == glxContext)
    {
        glXMakeCurrent(gpDisplay, 0, 0);
    }
    if (glxContext)
    {
        glXDestroyContext(gpDisplay, glxContext);
    }
    if (visualInfo)
    {
        XFree(visualInfo);
        visualInfo = NULL;
    }

    if (gpDisplay)
    {
        XCloseDisplay(gpDisplay);
        gpDisplay = NULL;
    }

    if (gpFile)
    {
        fprintf(gpFile, "Program terminated successfully...!!!");
        fclose(gpFile);
        gpFile = NULL;
    }
}