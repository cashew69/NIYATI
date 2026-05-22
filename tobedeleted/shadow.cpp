// Jai Shree Ram!!!

// Standard header files.
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <math.h>

// Xlib header files.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "vmath.h"

using namespace vmath;

// OpenGL related header files.
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "YS_MeshLoader.h"

// Macros
#define DEPTH_TEXTURE_SIZE  1080
// Global Variables - Application
int windowWidth = 800;
int windowHeight = 600;
bool isFullscreen = false;
Bool isActiveWindow = False;
bool isPaused = false;

Display *gpDisplay = NULL;
XVisualInfo *visualInfo = NULL;
Window window;
Colormap colormap;

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
GLXFBConfig glxFBConfig;
GLXContext glxContext = NULL;

char gszLogFileName[] = "log.txt";
FILE *gpFile = NULL;

// Global Variables - Shaders & Programs
GLuint lightProgram = 0;
GLuint cameraProgram = 0;
GLuint debugProgram = 0;

GLint locLightMVP = -1;

GLint locCamProj = -1;
GLint locCamMV = -1;
GLint locCamShadow = -1;
GLint locCamFullShading = -1;

// Global Variables - Buffers & VAOs
GLuint depthFBO = 0;
GLuint depthTex = 0;
GLuint depthDebugTex = 0;

GLuint modelVAO = 0;
int modelVertexCount = 0;

GLuint floorVAO = 0;
GLuint floorVBO = 0;

GLuint fullScreenQuadVAO = 0;

// Rendering State
int renderMode = 0; // 0 = Full, 1 = Light, 2 = Depth Debug
double totalTime = 0.0;
double lastTime = 0.0;
float animationTime = 0.0f;

vmath::mat4 cameraProjMatrix;
vmath::mat4 lightProjMatrix;

// Function Declarations
int initialize(void);
void resize(int, int);
void display(void);
void update(void);
void toggleFullScreen(void);
void uninitialize(void);
void loadShaders(void);

// Utility Functions
void printShaderLog(GLuint shader, const char* shaderName)
{
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0)
        {
            char* log = (char*)malloc(length);
            glGetShaderInfoLog(shader, length, NULL, log);
            fprintf(gpFile, "ERROR: Shader Compilation Failed for %s:\n%s\n", shaderName, log);
            free(log);
        }
    }
    else
    {
        fprintf(gpFile, "SUCCESS: Shader Compiled: %s\n", shaderName);
    }
}

void printProgramLog(GLuint program, const char* programName)
{
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0)
        {
            char* log = (char*)malloc(length);
            glGetProgramInfoLog(program, length, NULL, log);
            fprintf(gpFile, "ERROR: Program Linking Failed for %s:\n%s\n", programName, log);
            free(log);
        }
    }
    else
    {
        fprintf(gpFile, "SUCCESS: Program Linked: %s\n", programName);
    }
}

int main(void)
{
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
    }

    int defaultDepth;
    Atom windowManagerDeleteAtom;
    XEvent event;
    Screen * screen = NULL;
    int screenWidth, screenHeight;
    KeySym keysym;
    char keys[52];

    GLXFBConfig *pGLXFBConfig;
    GLXFBConfig bestFBConfig;
    XVisualInfo *pXVisualInfo;
    int iNumFBConfigs = 0;

    int frameBufferAttribute[] =
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

    gpDisplay = XOpenDisplay(NULL);
    if (gpDisplay == NULL)
    {
        printf("xOpenDisplay failed to connect with Xserver\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    int defaultScreen = XDefaultScreen(gpDisplay);
    defaultDepth = XDefaultDepth(gpDisplay, defaultScreen);

    pGLXFBConfig = glXChooseFBConfig(gpDisplay, defaultScreen, frameBufferAttribute, &iNumFBConfigs);
    if(pGLXFBConfig == NULL)
    {
        fprintf(gpFile, "glxChooseFBConfig failed\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    int indexOfBestFBConfig = -1;
    int bestFBConfigScore = -1;
    int bestNumberOfSamples = -1;
    int worstNumberOfSamples = -1;

    for (int i = 0; i < iNumFBConfigs; i++)
    {
        pXVisualInfo = glXGetVisualFromFBConfig(gpDisplay, pGLXFBConfig[i]);
        if (pXVisualInfo)
        {
            int sampleBuffers, samples;
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLE_BUFFERS, &sampleBuffers);
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLES, &samples);

            if (999 > bestNumberOfSamples && samples > bestNumberOfSamples)
            {
                bestFBConfigScore = 1;
                bestNumberOfSamples = samples;
                indexOfBestFBConfig = i;
            }
            if (worstNumberOfSamples < 0 || samples < worstNumberOfSamples)
            {
                indexOfBestFBConfig = i;
                worstNumberOfSamples = samples;
            }
            XFree(pXVisualInfo);
        }
    }

    bestFBConfig = pGLXFBConfig[indexOfBestFBConfig];
    glxFBConfig = bestFBConfig;
    XFree(pGLXFBConfig);

    visualInfo = glXGetVisualFromFBConfig(gpDisplay, glxFBConfig);

    XSetWindowAttributes windowAttributes;
    memset((void*) &windowAttributes, 0, sizeof(XSetWindowAttributes));

    windowAttributes.border_pixel = 0;
    windowAttributes.background_pixmap = 0;
    windowAttributes.background_pixel = XBlackPixel(gpDisplay, visualInfo->screen);
    windowAttributes.event_mask = KeyPressMask | ButtonPressMask | FocusChangeMask | StructureNotifyMask | ExposureMask;

    Window root = XRootWindow(gpDisplay, visualInfo->screen);
    windowAttributes.colormap = XCreateColormap(gpDisplay, root, visualInfo->visual, AllocNone);
    colormap = windowAttributes.colormap;

    window = XCreateWindow(
        gpDisplay, root,
        0, 0, windowWidth, windowHeight, 2,
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
    XStoreName(gpDisplay, window, "Nikhil Sathe's Shadow Mapping");
    XMapWindow(gpDisplay, window);

    screen = XScreenOfDisplay(gpDisplay, visualInfo->screen);
    screenWidth = XWidthOfScreen(screen);
    screenHeight = XHeightOfScreen(screen);
    XMoveWindow(gpDisplay, window, (screenWidth/2 - windowWidth/2), (screenHeight/2 - windowHeight/2));

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
                    isActiveWindow = True;
                    break;
                case FocusOut:
                    isActiveWindow = False;
                    break;
                case ConfigureNotify:
                    resize(event.xconfigure.width, event.xconfigure.height);
                    break;
                case KeyPress:
                    keysym = XkbKeycodeToKeysym(gpDisplay, event.xkey.keycode, 0, 0);
                    switch(keysym)
                    {
                        case XK_Escape:
                            bDone = True;
                            break;
                        default:
                            break;
                    }

                    XLookupString(&event.xkey, keys, sizeof(keys), NULL, NULL);
                    switch(keys[0])
                    {
                        case 'F':
                        case 'f':
                            isFullscreen = !isFullscreen;
                            toggleFullScreen();
                            break;
                        case 'R':
                        case 'r':
                            loadShaders();
                            break;
                        case 'P':
                        case 'p':
                            isPaused = !isPaused;
                            break;
                        case '1':
                            renderMode = 0; // Full
                            break;
                        case '2':
                            renderMode = 1; // Light
                            break;
                        case '3':
                            renderMode = 2; // Depth
                            break;
                        default:
                            break;
                    }
                    break;
                        case ButtonPress:
                            break;
                        case 33:
                            bDone = True;
                            break;
                        default :
                            break;
            }
        }

        if(isActiveWindow == True)
        {
            update();
            display();
        }
    }

    uninitialize();
    return(0);
}

int initialize(void)
{
    GLenum glewResult;

    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    if (glXCreateContextAttribsARB)
    {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 6,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        glxContext = glXCreateContextAttribsARB(gpDisplay, glxFBConfig, 0, True, attribs);
    }

    if (!glxContext)
    {
        glxContext = glXCreateNewContext(gpDisplay, glxFBConfig, GLX_RGBA_TYPE, 0, True);
        if (!glxContext && visualInfo)
        {
            glxContext = glXCreateContext(gpDisplay, visualInfo, 0, True);
        }
        if (!glxContext) return -1;
    }

    glXMakeCurrent(gpDisplay, window, glxContext);

    fprintf(gpFile, "LOG: Context Created.\n");

    glewResult = glewInit();
    if (glewResult != GLEW_OK)
    {
        fprintf(gpFile, "glewInit() Failed");
        return(-6);
    }

    // 1. Compile Shaders
    loadShaders();

    // 2. Load the Hark Model
    fprintf(gpFile, "LOG: Attempting to load Model...\n");
    loadObjModel("hark.obj", &modelVAO, &modelVertexCount);

    // 3. Create a Flat Plane (Floor) VAO
    GLfloat floorVertices[] = {
        // Position             // Normal
        -40.0f, -4.0f, -40.0f,  0.0f, 1.0f, 0.0f,
        -40.0f, -4.0f,  40.0f,  0.0f, 1.0f, 0.0f,
        40.0f, -4.0f, -40.0f,  0.0f, 1.0f, 0.0f,
        40.0f, -4.0f,  40.0f,  0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &floorVAO);
    glBindVertexArray(floorVAO);
    glGenBuffers(1, &floorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 4. Generate empty VAO for fullscreen quad
    glGenVertexArrays(1, &fullScreenQuadVAO);

    // 5. Setup Shadow Map FBO and Textures
    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexStorage2D(GL_TEXTURE_2D, 11, GL_DEPTH_COMPONENT32F, DEPTH_TEXTURE_SIZE, DEPTH_TEXTURE_SIZE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex, 0);

    glGenTextures(1, &depthDebugTex);
    glBindTexture(GL_TEXTURE_2D, depthDebugTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, DEPTH_TEXTURE_SIZE, DEPTH_TEXTURE_SIZE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depthDebugTex, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_DEPTH_TEST);

    fprintf(gpFile, "LOG: Initialization complete. Entering Render Loop.\n");
    return(0);
}

void loadShaders(void)
{
    GLuint vertexShaderObject, fragmentShaderObject;

    if (lightProgram) glDeleteProgram(lightProgram);
    if (cameraProgram) glDeleteProgram(cameraProgram);
    if (debugProgram) glDeleteProgram(debugProgram);

    // --- Program 1: Light Program (Shadow Map Generation) ---
    const GLchar* lightVertexShaderSource =
    "#version 460 core\n"
    "uniform mat4 mvp;\n"
    "layout (location = 0) in vec4 position;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = mvp * position;\n"
    "}\n";

    const GLchar* lightFragmentShaderSource =
    "#version 460 core\n"
    "layout (location = 0) out vec4 color;\n"
    "void main(void)\n"
    "{\n"
    "    color = vec4(gl_FragCoord.z);\n"
    "}\n";

    vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderObject, 1, &lightVertexShaderSource, NULL);
    glCompileShader(vertexShaderObject);

    fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderObject, 1, &lightFragmentShaderSource, NULL);
    glCompileShader(fragmentShaderObject);

    lightProgram = glCreateProgram();
    glAttachShader(lightProgram, vertexShaderObject);
    glAttachShader(lightProgram, fragmentShaderObject);
    glLinkProgram(lightProgram);

    locLightMVP = glGetUniformLocation(lightProgram, "mvp");

    glDeleteShader(vertexShaderObject);
    glDeleteShader(fragmentShaderObject);

    // --- Program 2: Camera Program (Scene Rendering) ---
    const GLchar* cameraVertexShaderSource =
    "#version 460 core\n"
    "uniform mat4 mv_matrix;\n"
    "uniform mat4 proj_matrix;\n"
    "uniform mat4 shadow_matrix;\n"
    "layout (location = 0) in vec4 position;\n"
    "layout (location = 1) in vec3 normal;\n"
    "out VS_OUT\n"
    "{\n"
    "    vec4 shadow_coord;\n"
    "    vec3 N;\n"
    "    vec3 L;\n"
    "    vec3 V;\n"
    "} vs_out;\n"
    "uniform vec3 light_pos = vec3(100.0, 100.0, 100.0);\n"
    "void main(void)\n"
    "{\n"
    "    vec4 P = mv_matrix * position;\n"
    "    vs_out.N = mat3(mv_matrix) * normal;\n"
    "    vs_out.L = light_pos - P.xyz;\n"
    "    vs_out.V = -P.xyz;\n"
    "    vs_out.shadow_coord = shadow_matrix * position;\n"
    "    gl_Position = proj_matrix * P;\n"
    "}\n";

    const GLchar* cameraFragmentShaderSource =
    "#version 460 core\n"
    "layout (location = 0) out vec4 color;\n"
    "layout (binding = 0) uniform sampler2DShadow shadow_tex;\n"
    "in VS_OUT\n"
    "{\n"
    "    vec4 shadow_coord;\n"
    "    vec3 N;\n"
    "    vec3 L;\n"
    "    vec3 V;\n"
    "} fs_in;\n"
    "uniform vec3 diffuse_albedo = vec3(0.9, 0.8, 1.0);\n"
    "uniform vec3 specular_albedo = vec3(0.7);\n"
    "uniform float specular_power = 300.0;\n"
    "uniform bool full_shading = true;\n"
    "void main(void)\n"
    "{\n"
    "    vec3 N = normalize(fs_in.N);\n"
    "    vec3 L = normalize(fs_in.L);\n"
    "    vec3 V = normalize(fs_in.V);\n"
    "    vec3 R = reflect(-L, N);\n"
    "    vec3 diffuse = max(dot(N, L), 0.0) * diffuse_albedo;\n"
    "    vec3 specular = pow(max(dot(R, V), 0.0), specular_power) * specular_albedo;\n"
    "    color = textureProj(shadow_tex, fs_in.shadow_coord) * mix(vec4(1.0), vec4(diffuse + specular, 1.0), bvec4(full_shading));\n"
    "}\n";

    vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderObject, 1, &cameraVertexShaderSource, NULL);
    glCompileShader(vertexShaderObject);

    fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderObject, 1, &cameraFragmentShaderSource, NULL);
    glCompileShader(fragmentShaderObject);

    cameraProgram = glCreateProgram();
    glAttachShader(cameraProgram, vertexShaderObject);
    glAttachShader(cameraProgram, fragmentShaderObject);
    glLinkProgram(cameraProgram);

    locCamProj = glGetUniformLocation(cameraProgram, "proj_matrix");
    locCamMV = glGetUniformLocation(cameraProgram, "mv_matrix");
    locCamShadow = glGetUniformLocation(cameraProgram, "shadow_matrix");
    locCamFullShading = glGetUniformLocation(cameraProgram, "full_shading");

    glDeleteShader(vertexShaderObject);
    glDeleteShader(fragmentShaderObject);

    // --- Program 3: Debug View Program (Show light depth map) ---
    const GLchar* debugVertexShaderSource =
    "#version 460 core\n"
    "void main(void)\n"
    "{\n"
    "    const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.5, 1.0),\n"
    "                                   vec4( 1.0, -1.0, 0.5, 1.0),\n"
    "                                   vec4(-1.0,  1.0, 0.5, 1.0),\n"
    "                                   vec4( 1.0,  1.0, 0.5, 1.0));\n"
    "    gl_Position = vertices[gl_VertexID];\n"
    "}\n";

    const GLchar* debugFragmentShaderSource =
    "#version 460 core\n"
    "layout (binding = 0) uniform sampler2D tex_depth;\n"
    "layout (location = 0) out vec4 color;\n"
    "void main(void)\n"
    "{\n"
    "    float d = texelFetch(tex_depth, ivec2(gl_FragCoord.xy * 3.0) + ivec2(850, 1050), 0).r;\n"
    "    d = (d - 0.95) * 15.0;\n"
    "    color = vec4(d);\n"
    "}\n";

    vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderObject, 1, &debugVertexShaderSource, NULL);
    glCompileShader(vertexShaderObject);

    fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderObject, 1, &debugFragmentShaderSource, NULL);
    glCompileShader(fragmentShaderObject);

    debugProgram = glCreateProgram();
    glAttachShader(debugProgram, vertexShaderObject);
    glAttachShader(debugProgram, fragmentShaderObject);
    glLinkProgram(debugProgram);

    glDeleteShader(vertexShaderObject);
    glDeleteShader(fragmentShaderObject);
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
    fsevent.xclient.data.l[0] = isFullscreen ? 1 : 0;
    fsevent.xclient.data.l[1] = windowManagerFullscreenStateAtom;

    XSendEvent(gpDisplay, XRootWindow(gpDisplay, visualInfo->screen), False, SubstructureNotifyMask, &fsevent);
}

void resize(int width, int height)
{
    if (height <= 0) height = 1;
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    cameraProjMatrix = vmath::perspective(50.0f,
                                          (float)windowWidth / (float)windowHeight,
                                          1.0f,
                                          200.0f);
}

void display(void)
{
    static const GLfloat zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static const GLfloat ones[]  = { 1.0f };
    static const GLfloat gray[]  = { 0.1f, 0.1f, 0.1f, 0.0f };

    static const vmath::mat4 scale_bias_matrix = vmath::mat4(vmath::vec4(0.5f, 0.0f, 0.0f, 0.0f),
                                                             vmath::vec4(0.0f, 0.5f, 0.0f, 0.0f),
                                                             vmath::vec4(0.0f, 0.0f, 0.5f, 0.0f),
                                                             vmath::vec4(0.5f, 0.5f, 0.5f, 1.0f));

    vmath::vec3 light_position = vmath::vec3(20.0f, 20.0f, 20.0f);
    vmath::vec3 view_position = vmath::vec3(0.0f, 0.0f, 40.0f);

    vmath::mat4 light_view_matrix = vmath::lookat(light_position, vmath::vec3(0.0f), vmath::vec3(0.0f, 1.0f, 0.0f));
    vmath::mat4 camera_view_matrix = vmath::lookat(view_position, vmath::vec3(0.0f), vmath::vec3(0.0f, 1.0f, 0.0f));

    vmath::mat4 light_vp_matrix = lightProjMatrix * light_view_matrix;
    vmath::mat4 shadow_sbpv_matrix = scale_bias_matrix * lightProjMatrix * light_view_matrix;

    // Transform Matrices
    vmath::mat4 modelMatrices[4];

    // 1. Plane Surface (Floor)
    modelMatrices[0] = vmath::mat4::identity();

    // 2. Model 1: Standing still in the center
    modelMatrices[1] = vmath::translate(0.0f, -1.0f, 0.0f) * vmath::scale(6.0f);

    // 3. Model 2: Rotating around the center on the y-axis
    modelMatrices[2] = vmath::rotate(animationTime * 45.0f, 0.0f, 1.0f, 0.0f) *
    vmath::translate(4.0f, 2.0f, 0.0f) *
    vmath::scale(4.0f);

    // 4. Model 3: Rotating around itself on the X and Z axis
    modelMatrices[3] = vmath::translate(6.0f, 5.0f, 3.0f) * vmath::rotate(animationTime * 60.0f, 0.0f, 0.0f, 1.0f) *
    vmath::scale(4.0f);

    // ============================================
    // PASS 1: Render from Light's Perspective (Shadow Map)
    // ============================================
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glViewport(0, 0, DEPTH_TEXTURE_SIZE, DEPTH_TEXTURE_SIZE);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(4.0f, 4.0f);

    glClearBufferfv(GL_COLOR, 0, zeros);
    glClearBufferfv(GL_DEPTH, 0, ones);

    glUseProgram(lightProgram);

    // Draw Floor
    glBindVertexArray(floorVAO);
    glUniformMatrix4fv(locLightMVP, 1, GL_FALSE, light_vp_matrix * modelMatrices[0]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Draw Models
    glBindVertexArray(modelVAO);
    for (int i = 1; i < 4; i++)
    {
        glUniformMatrix4fv(locLightMVP, 1, GL_FALSE, light_vp_matrix * modelMatrices[i]);
        glDrawArrays(GL_TRIANGLES, 0, modelVertexCount);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ============================================
    // PASS 2: Render from Camera's Perspective
    // ============================================
    glViewport(0, 0, windowWidth, windowHeight);
    glClearBufferfv(GL_COLOR, 0, gray);
    glClearBufferfv(GL_DEPTH, 0, ones);

    if (renderMode == 2)
    {
        // Debug View: Render depth map to a fullscreen quad
        glDisable(GL_DEPTH_TEST);

        glUseProgram(debugProgram);
        glBindVertexArray(fullScreenQuadVAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthDebugTex);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        // Standard View: Render models with shadow mapping
        glUseProgram(cameraProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTex); // Bind shadow map to sampler2DShadow

        glUniformMatrix4fv(locCamProj, 1, GL_FALSE, cameraProjMatrix);
        glUniform1i(locCamFullShading, renderMode == 0 ? 1 : 0);

        // Draw Floor
        glBindVertexArray(floorVAO);
        vmath::mat4 shadow_matrix_floor = shadow_sbpv_matrix * modelMatrices[0];
        glUniformMatrix4fv(locCamShadow, 1, GL_FALSE, shadow_matrix_floor);
        glUniformMatrix4fv(locCamMV, 1, GL_FALSE, camera_view_matrix * modelMatrices[0]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Draw Models
        glBindVertexArray(modelVAO);
        for (int i = 1; i < 4; i++)
        {
            vmath::mat4 shadow_matrix = shadow_sbpv_matrix * modelMatrices[i];

            glUniformMatrix4fv(locCamShadow, 1, GL_FALSE, shadow_matrix);
            glUniformMatrix4fv(locCamMV, 1, GL_FALSE, camera_view_matrix * modelMatrices[i]);

            glDrawArrays(GL_TRIANGLES, 0, modelVertexCount);
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);

    // Update Title Bar
    char title[256];
    const char* modeStr = (renderMode == 0) ? "Full Shading" : (renderMode == 1) ? "Light Shading Only" : "Shadow Map Debug View";
    sprintf(title, "Nikhil Sathe's Shadow Mapping | Mode: %s", modeStr);
    XStoreName(gpDisplay, window, title);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool error_logged = false;
        if (!error_logged) {
            fprintf(gpFile, "ERROR: OpenGL error in display loop: 0x%x\n", err);
            error_logged = true;
        }
    }

    glXSwapBuffers(gpDisplay, window);
}

double getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void update(void)
{
    double currentTime = getTime();

    if (!isPaused)
    {
        totalTime += (currentTime - lastTime);
    }
    lastTime = currentTime;

    animationTime = (float)totalTime + 30.0f;

    // Calculate light frustum (constant, but can be updated here if light moves)
    lightProjMatrix = vmath::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 200.0f);
}

void uninitialize(void)
{
    if (depthTex) glDeleteTextures(1, &depthTex);
    if (depthDebugTex) glDeleteTextures(1, &depthDebugTex);
    if (depthFBO) glDeleteFramebuffers(1, &depthFBO);

    if (modelVAO) glDeleteVertexArrays(1, &modelVAO);
    if (floorVAO) glDeleteVertexArrays(1, &floorVAO);
    if (floorVBO) glDeleteBuffers(1, &floorVBO);
    if (fullScreenQuadVAO) glDeleteVertexArrays(1, &fullScreenQuadVAO);

    if (window) XDestroyWindow(gpDisplay, window);
    if (colormap) XFreeColormap(gpDisplay, colormap);


    if (lightProgram) glDeleteProgram(lightProgram);
    if (cameraProgram) glDeleteProgram(cameraProgram);
    if (debugProgram) glDeleteProgram(debugProgram);

    GLXContext currentContext = glXGetCurrentContext();
    if (currentContext && currentContext == glxContext)
    {
        glXMakeCurrent(gpDisplay, 0, 0);
    }
    if (glxContext) glXDestroyContext(gpDisplay, glxContext);
    if (gpDisplay)
    {
        XCloseDisplay(gpDisplay);
        gpDisplay = NULL;
    }

    if (visualInfo)
    {
        XFree(visualInfo);
        visualInfo = NULL;
    }

    if (gpFile)
    {
        fprintf(gpFile, "Program terminated successfully...!!!");
        fclose(gpFile);
        gpFile = NULL;
    }
}
