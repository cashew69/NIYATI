

// ============================================================================
// SHARED GLOBAL VARIABLES (definitions)
// ============================================================================

// Camera variables
float camz = 100.0f;
float camy = 10.0f;
float eyex = 0.0f;
float eyez = 0.0f;

// Mouse input
int mouse_x = 0;
int mouse_y = 0;
bool mouse_captured = false;
float camera_yaw = 0.0f;
float camera_pitch = 0.0f;
float mouse_sensitivity = 0.1f;

// OpenGL rendering state
GLuint HeightMap = 0;
int togglePolyLine = 0;
float rotationAngle = 0.0f;

// Shader attribute configuration
const char* attribNames[4] = {"aPosition", "aNormal", "aColor", "aTexCoord"};
GLint attribIndices[4] = {ATTRIB_POSITION, ATTRIB_NORMAL, ATTRIB_COLOR, ATTRIB_TEXCOORD};

// ============================================================================
// SHARED FUNCTION IMPLEMENTATIONS
// ============================================================================

void printGLInfo(void)
{
    GLint numExtensions, i;

    fprintf(gpFile, "OPENGL INFORMATION\n");
    fprintf(gpFile, "******************\n");
    fprintf(gpFile, "OpenGL Vendor : %s\n", glGetString(GL_VENDOR));
    fprintf(gpFile, "OpenGL Renderer : %s\n", glGetString(GL_RENDERER));
    fprintf(gpFile, "OpenGL Version : %s\n", glGetString(GL_VERSION));
    fprintf(gpFile, "GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    fprintf(gpFile, "Total number of OpenGL Extensions : %d\n", numExtensions);

    for(i = 0; i < numExtensions; i++)
    {
        fprintf(gpFile, "%s \n", glGetStringi(GL_EXTENSIONS, i));
    }
    fprintf(gpFile, "******************\n");
}

int initializeOpenGL(void)
{
    printGLInfo();

    // ===== Main Shader Program (Vertex + Fragment) =====
    const char* mainShaderFiles[5] = {
        "core/shaders/main_vs.glsl",
        NULL,
        NULL,
        NULL,
        "core/shaders/main_fs[lambart].glsl"
    };
    if (!buildShaderProgramFromFiles(mainShaderFiles, 5, 
                &mainShaderProgram, attribNames, attribIndices, 4))
    {
        fprintf(gpFile, "Failed to build main shader program\n");
        return (-7);
    }

    // ===== Tessellation Shader Program (Vertex + TCS + TES + Fragment) =====
    const char* tessShaderFiles[5] = {
        "user/svs.glsl",
        "user/main_tcs.glsl",
        "user/main_tes.glsl",
        NULL,
        "user/sfs.glsl"
    };
    if (!buildShaderProgramFromFiles(tessShaderFiles, 5, 
                &tessellationShaderProgram, attribNames, attribIndices, 4))
    {
        fprintf(gpFile, "Failed to build tessellation shader program\n");
        return (-7);
    }

    // ===== Line Shader Program (Vertex + Fragment) =====
    const char* lineShaderFiles[5] = {
        "core/shaders/lineVert.glsl",
        NULL,
        NULL,
        NULL,
        "core/shaders/lineFrag.glsl"
    };
    if (!buildShaderProgramFromFiles(lineShaderFiles, 5, 
                &lineShaderProgram, attribNames, attribIndices, 4))
    {
        fprintf(gpFile, "Failed to build line shader program\n");
        return (-7);
    }

    // Load model
    if (!loadModel("user/models/ship.fbx", &sceneMeshes, &meshCount, 1.0f))
    {
        fprintf(gpFile, "Failed to load model\n");
    }

    // ===== Terrain =====
    terrainMesh = createTerrainMesh();
    loadPNGTexture(&HeightMap, const_cast<char*>("heightmap.png"), 4, 1);

    // Depth Related Code
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Set clear color
    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);

    // Initialize projection matrix
    perspectiveProjectionMatrix = mat4::identity();

    fprintf(gpFile, "initializeOpenGL() completed successfully\n");
    return 0;
}

void resize(int width, int height)
{
    // If height accidentally becomes 0 or less than 1, make it 1
    if (height <= 0)
    {
        height = 1;
    }

    // Set the viewport
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    // Update perspective projection matrix
    perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 10000.0f);
}

void display(void)
{
    // Calculate camera direction from yaw and pitch
    float yaw_rad = camera_yaw * 3.14159265f / 180.0f;
    float pitch_rad = camera_pitch * 3.14159265f / 180.0f;
    
    vec3 direction;
    direction[0] = cos(pitch_rad) * sin(yaw_rad);
    direction[1] = sin(pitch_rad);
    direction[2] = cos(pitch_rad) * cos(yaw_rad);
    
    // Set up camera position and target
    vec3 camera_pos(eyex, camy, camz);
    vec3 camera_target = camera_pos + direction;
    
    // Update view matrix
    viewMatrix = vmath::lookat(camera_pos, camera_target, vec3(0.0f, 1.0f, 0.0f));
    
    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render the scene
    renderer(rotationAngle, HeightMap);
    
    // note: Platform-specific code should swap buffers after calling this function
}

void update(void)
{
    // Update rotation angle
    rotationAngle += 0.5f;
    if (rotationAngle >= 360.0f)
    {
        rotationAngle = 0.0f;
    }
}

void updateCameraFromMouse(int delta_x, int delta_y)
{
    camera_yaw += delta_x * mouse_sensitivity;
    camera_pitch -= delta_y * mouse_sensitivity;
    
    // Clamp pitch to prevent camera flipping
    if (camera_pitch > 89.0f) camera_pitch = 89.0f;
    if (camera_pitch < -89.0f) camera_pitch = -89.0f;
}

void toggleWireframe(void)
{
    if (togglePolyLine == 0)
    {
        togglePolyLine = 1;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        fprintf(gpFile, "Wireframe mode enabled\n");
    }
    else
    {
        togglePolyLine = 0;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        fprintf(gpFile, "Wireframe mode disabled\n");
    }
}

void cleanupOpenGL(void)
{
    // Free mesh resources
    if (terrainMesh)
    {
        // freeMesh(terrainMesh);
        terrainMesh = NULL;
    }
    

    
    fprintf(gpFile, "OpenGL resources cleaned up\n");
}