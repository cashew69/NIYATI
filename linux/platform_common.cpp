

// ============================================================================
// SHARED GLOBAL VARIABLES (definitions)
// ============================================================================

// New Camera Control Variables
int cameraMode = 0; // 0=FPS, 1=Manual, 2=Orbit
vec3 camCenter(0.0f, 0.0f, 0.0f);
vec3 camUp(0.0f, 1.0f, 0.0f);

// Orbit Camera Variables
float orbitRadius = 100.0f;
float orbitYaw = 0.0f;
float orbitPitch = 0.0f;
bool orbitKeyboardControl = false;
bool showOrbitVisuals = true;

// Main Camera
Camera* mainCamera = NULL;

// Mouse input
int mouse_x = 0;
int mouse_y = 0;
bool mouse_captured = false;
float camera_yaw = 0.0f;
float camera_pitch = 0.0f;
float mouse_sensitivity = 0.1f;
// Camera Orientation Globals
quaternion camera_orientation = quaternion(0.0f, 0.0f, 0.0f, 1.0f);
bool use_camera_quaternion = false;

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
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // transparent

    
    // Initialize projection matrix
    perspectiveProjectionMatrix = mat4::identity();
    
    // Initialize Main Camera
    mainCamera = createCamera(vec3(0.0f, 0.0f, 100.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    initPropulsion();

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
    if (!mainCamera) return;

    // vec3 camera_pos;
    // vec3 camera_target;
    vec3 up_vector = vec3(0.0f, 1.0f, 0.0f);

    switch (cameraMode)
    {
    case 0: // FPS Mode
    {
        // Calculate camera direction from yaw and pitch
        float yaw_rad = camera_yaw * 3.14159265f / 180.0f;
        float pitch_rad = camera_pitch * 3.14159265f / 180.0f;

        vec3 direction;
        direction[0] = cos(pitch_rad) * sin(yaw_rad);
        direction[1] = sin(pitch_rad);
        direction[2] = cos(pitch_rad) * cos(yaw_rad);

        // Set up camera position and target
        camera_target = camera_pos + direction;
        up_vector = vec3(0.0f, 1.0f, 0.0f);
        break;
    }
    case 1: // Manual Mode
    {
        camera_target = camera_pos;
        up_vector = camUp;
        break;
    }
    case 2: // Orbit Mode
    {
        float yaw_rad = orbitYaw * 3.14159265f / 180.0f;
        float pitch_rad = orbitPitch * 3.14159265f / 180.0f;

        // Calculate position on sphere
        float x = orbitRadius * cos(pitch_rad) * sin(yaw_rad);
        float y = orbitRadius * sin(pitch_rad);
        float z = orbitRadius * cos(pitch_rad) * cos(yaw_rad);

        camera_pos = camCenter + vec3(x, y, z);
        camera_target = camCenter;
        up_vector = vec3(0.0f, 1.0f, 0.0f); // Usually keep up as Y for orbit
        
        break;
    }
    case 3: // Ship TPP Mode
    {
        shipCam(camera_offsets_for_ship_tpp[0], camera_offsets_for_ship_tpp[1], camera_offsets_for_ship_tpp[2]);
        

        break;
    }
    }
    
    // Update Main Camera
    mainCamera->position = camera_pos;
    mainCamera->target = camera_target;
    mainCamera->up = up_vector;
    
    // Pass quaternion state to camera
    mainCamera->orientation = camera_orientation;
    mainCamera->useQuaternion = use_camera_quaternion;
    
    updateCamera(mainCamera);
    
    // Use view matrix from camera
    viewMatrix = mainCamera->viewMatrix;
    
    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render the scene
    renderer(rotationAngle, HeightMap);

   
}

void update(void)
{
    // Update rotation angle
    rotationAngle += 0.5f;
    if (rotationAngle >= 360.0f)
    {
        rotationAngle = 0.0f;
    }
    shipUpdate();
}

void updateCameraFromMouse(int delta_x, int delta_y)
{
    if (cameraMode == 2) // Orbit Mode
    {
        orbitYaw += delta_x * mouse_sensitivity;
        orbitPitch -= delta_y * mouse_sensitivity;
        
        // Clamp pitch to prevent flipping
        if (orbitPitch > 89.0f) orbitPitch = 89.0f;
        if (orbitPitch < -89.0f) orbitPitch = -89.0f;
    }
    else // FPS Mode (Default)
    {
        camera_yaw += delta_x * mouse_sensitivity;
        camera_pitch -= delta_y * mouse_sensitivity;
        
        // Clamp pitch to prevent camera flipping
        if (camera_pitch > 89.0f) camera_pitch = 89.0f;
        if (camera_pitch < -89.0f) camera_pitch = -89.0f;
    }
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