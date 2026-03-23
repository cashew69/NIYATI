/* platform_common.cpp
 Minimal shared layer. Only what every project and the platform files need.
 All project-specific logic (camera, shaders, rendering) lives in the active project.

 To switch projects, change the include line (currently build.sh takes care of it.).
*/

// ============================================================================
// SHARED GLOBALS
// ============================================================================

// Used by platform input handlers (WASD movement, mouse capture toggle)
//  still in development so keep it.
// Planning to remove this extra dependancy doesn't look neat and clean for every project.
vec3 camera_pos(0.0f, 0.0f, 10.0f);
int  mouse_x = 0;
int  mouse_y = 0;
bool mouse_captured = false;

// Standard shader attribute slots — used by all projects for buildShaderProgram / createVAO_VBO
const char *attribNames[4]   = {"aPosition", "aNormal", "aColor", "aTexCoord"};
GLint       attribIndices[4]  = {ATTRIB_POSITION, ATTRIB_NORMAL, ATTRIB_COLOR, ATTRIB_TEXCOORD};

// Change this include to switch projects
#include "examples/04_clouds/project.cpp"

// Each project must define:
//   void projectInit()
//   void projectRender()    ← responsible for setting viewMatrix before drawing
//   void projectUpdate()
//   void projectCleanup()



// ============================================================================
// SHARED ENGINE FUNCTIONS
// ============================================================================

void printGLInfo(void) {
    fprintf(gpFile, "OpenGL Vendor   : %s\n", glGetString(GL_VENDOR));
    fprintf(gpFile, "OpenGL Renderer : %s\n", glGetString(GL_RENDERER));
    fprintf(gpFile, "OpenGL Version  : %s\n", glGetString(GL_VERSION));
    fprintf(gpFile, "GLSL Version    : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

int initializeOpenGL(void) {
    printGLInfo();

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    perspectiveProjectionMatrix = mat4::identity();

    projectInit();  // each project sets up its own camera, shaders, meshes

    fprintf(gpFile, "initializeOpenGL() completed\n");
    return 0;
}

void resize(int width, int height) {
    if (height <= 0) height = 1;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    perspectiveProjectionMatrix = vmath::perspective(
        45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 10000.0f);
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    projectRender();  // project is responsible for setting viewMatrix
}

void update(void) {
    projectUpdate();
}

void cleanupOpenGL(void) {
    projectCleanup();
    fprintf(gpFile, "OpenGL resources cleaned up\n");
}
