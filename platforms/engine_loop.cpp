// platforms/engine_loop.cpp
// Shared globals and engine loop functions used by both builds:
//   platform_common.cpp  (demo / X11 / Win32)
//   editor_root.cpp      (GLFW editor)
// Never compiled standalone — always #include'd into a translation unit.

// ============================================================================
// SHARED GLOBALS
// ============================================================================

vec3  camera_pos(5.0f, 10.0f, 75.0f);
int   mouse_x = 0;
int   mouse_y = 0;
bool  mouse_captured = false;

const char *attribNames[4]  = {"aPosition", "aNormal", "aColor", "aTexCoord"};
GLint       attribIndices[4] = {ATTRIB_POSITION, ATTRIB_NORMAL, ATTRIB_COLOR, ATTRIB_TEXCOORD};

bool  bPerspective   = true;
bool  bOrthographic  = false;
float globalFOV      = 45.0f;
bool  g_VSyncEnabled = true;

// Delta time
float g_DeltaTime    = 1.0f / 60.0f;
float g_Time         = 0.0f;
bool  g_UseDeltaTime = true;

static float s_PrevFrameTime = -1.0f;

static void TickDeltaTime() {
    float now = platformGetTime();
    if (s_PrevFrameTime < 0.0f) { s_PrevFrameTime = now; return; }
    float raw = now - s_PrevFrameTime;
    s_PrevFrameTime = now;
    if (raw > 0.25f) raw = 0.25f;
    g_DeltaTime = g_UseDeltaTime ? raw : (1.0f / 60.0f);
    g_Time += g_DeltaTime;
}

// Defined in imgui_setup.cpp for editor builds; owned here for x11/win32.
#ifndef HAS_IMGUI
int viewportWidth  = 800;
int viewportHeight = 600;
#endif

// Forward declarations — defined by active_project.h (demo) or editor_root.cpp (editor).
void projectInit();
void projectUpdate();
void projectRender();
void projectCleanup();

// ============================================================================
// ENGINE LOOP
// ============================================================================

static void printGLInfo(void) {
    LOG_I("OpenGL Vendor   : %s", glGetString(GL_VENDOR));
    LOG_I("OpenGL Renderer : %s", glGetString(GL_RENDERER));
    LOG_I("OpenGL Version  : %s", glGetString(GL_VERSION));
    LOG_I("GLSL Version    : %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

int initializeOpenGL(void) {
    printGLInfo();
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    perspectiveProjectionMatrix = mat4::identity();
    InitializeShaders();
    projectInit();
    LOG_I("initializeOpenGL() completed");
    return 0;
}

void resize(int width, int height) {
    if (height <= 0) height = 1;
#ifndef HAS_IMGUI
    viewportWidth  = width;
    viewportHeight = height;
#endif
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    if (bPerspective) {
        perspectiveProjectionMatrix = vmath::perspective(
            globalFOV, (GLfloat)width / (GLfloat)height, 0.1f, 10000.0f);
    } else {
        float aspect    = (GLfloat)width / (GLfloat)height;
        float orthoSize = 20.0f;
        perspectiveProjectionMatrix = vmath::ortho(
            -orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, 0.1f, 10000.0f);
    }
}

void display(void) {
    TickDeltaTime();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    projectRender();
}

void update(void) {
    projectUpdate();
}

void cleanupOpenGL(void) {
    projectCleanup();
    LOG_I("OpenGL resources cleaned up");
}
