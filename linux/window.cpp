#include "common.h"
//#include "user/terrain.cpp"


// Macros
#define winwidth 800
#define winheight 600

// https://tronche.com/gui/x/xlib
// Global Variables
Display *gpDisplay = NULL; // Display is medium between client-server DS with 77 members.
XVisualInfo *visualInfo = NULL; // device context. (contains graphic hardware info).
Window window; // Object representing Window
Colormap colormap; // Hardware DS.
bool bFullscreen = false;
Bool bActiveWindow = False;

typedef GLXContext (*glXCreateContextAttribsARBProc)
(Display *, GLXFBConfig, GLXContext, Bool, const int *);

glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
GLXFBConfig glxFBConfig;

// OpenGL related vars.
GLXContext glxContext = NULL;

GLuint HeightMap = 0;

float rotationAngle = 0.0f;

float camz = 200.0f;
float camy = 50.0f;

int main(void)
{
    //file create
	gpFile = fopen(gszLogFileName, "w");
	//w - overwrites if file already exists
	
	if (gpFile == NULL)
	{
		printf("Log file creation failed...!!!\n");
		exit(0);
	}
	else
	{
        /* Make log output unbuffered so messages appear immediately on disk */
        setvbuf(gpFile, NULL, _IONBF, 0);
		fprintf(gpFile, "Program started successfully...!!!\n");
		fprintf(gpFile, "HELLO WORLD !!!\n");
	}

    // Function Declarations
    
    int initialize(void);
    void resize(int, int);
    void display(void);
    void update(void);
    void toggleFullScreen(void);
    void uninitialize(void);

    // Variable Declarations
    int defaultDepth;
    Atom windowManagerDeleteAtom;
    XEvent event;
    Screen * screen = NULL;
    int screenWidth , screenHeight;
    KeySym keysym;
    char keys[52]; // We only need 0th index but conventionally array size equal to number of alphabets.

    GLXFBConfig *pGLXFBConfig;
    GLXFBConfig bestFBConfig;
    XVisualInfo *pXVisualInfo;
    int iNumFBConfigs = 0;

     
    // Kadhla tari chalel yala GLX_RGBA karan GLX_RGBA_BIT
    int frameBufferAttributes[] = 
    {
        GLX_X_RENDERABLE,
        True,
        GLX_DRAWABLE_TYPE,
        GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,
        GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,
        GLX_TRUE_COLOR,
        GLX_DOUBLEBUFFER,
        True,
        GLX_RED_SIZE, 8, 
        GLX_GREEN_SIZE, 8, 
        GLX_BLUE_SIZE, 8, 
        GLX_ALPHA_SIZE, 8, 
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        None
    };

    

    

    Bool bDone = False;


    // Code
    
    // Open the connection with xserver.
    gpDisplay = XOpenDisplay(NULL);
    if (gpDisplay == NULL)
    {
        printf("xOpenDisplay failed to connect with Xserver: gpDisplay = xOpenDisplay(NULL)");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    // Create the Default screen object.
    int defaultScreen = XDefaultScreen(gpDisplay);

    // Get Default depth
    defaultDepth = XDefaultDepth(gpDisplay, defaultScreen); 

    pGLXFBConfig = glXChooseFBConfig(gpDisplay, defaultScreen, frameBufferAttributes, &iNumFBConfigs);
    if(pGLXFBConfig == NULL)
    {
        fprintf(gpFile, "glxChooseFBConfig failed");
        uninitialize();
        exit(EXIT_FAILURE);
    }
    fprintf(gpFile, "glxChooseFBConfig returned %d configs\n", iNumFBConfigs);

    int indexOfBestFBConfig = -1;
    int indexOfWorstFBConfig = -1;
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
            if ( worstNumberOfSamples < 0 || samples < worstNumberOfSamples)
            {
                indexOfWorstFBConfig = i;
                worstNumberOfSamples = samples;
            }
            XFree(pXVisualInfo);
        }
    }

    bestFBConfig = pGLXFBConfig[indexOfBestFBConfig];

    // Set Global glxfbconfig
    glxFBConfig = bestFBConfig;
    XFree(pGLXFBConfig);

    visualInfo = glXGetVisualFromFBConfig(gpDisplay, glxFBConfig); // no need for errorchecking we have come through checks above.

    // Set Window Attributes
    XSetWindowAttributes windowAttributes;
    memset((void*) &windowAttributes, 0, sizeof(XSetWindowAttributes));

    windowAttributes.border_pixel = 0;
    windowAttributes.background_pixmap = 0;
    windowAttributes.background_pixel = XBlackPixel(gpDisplay, visualInfo->screen);
    windowAttributes.event_mask = KeyPressMask | ButtonPressMask | FocusChangeMask | StructureNotifyMask | ExposureMask;
    Window root = XRootWindow(gpDisplay, visualInfo->screen);
    printf("XRootWindow before colormap: %lu\n", root);
    windowAttributes.colormap = XCreateColormap(gpDisplay, root, visualInfo->visual, AllocNone);

    colormap = windowAttributes.colormap;

    // Create Window
    root = XRootWindow(gpDisplay, visualInfo->screen);
    printf("XRootWindow before XCreateWindow: %lu\n", root);
    window = XCreateWindow(
            gpDisplay,
            root,
            0, 0,
            winwidth, winheight,
            2,
            visualInfo->depth,
            InputOutput,
            visualInfo->visual,
            CWBorderPixel | CWBackPixel | CWEventMask | CWColormap, 
            &windowAttributes
            );

    if (!window)
    {
        printf("XCreateWindow failed");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    // Create Atom for windowmanager to destroy the window.
    windowManagerDeleteAtom = XInternAtom(gpDisplay, "WM_DELETE_WINDOW", True); 
    XSetWMProtocols(gpDisplay, window, &windowManagerDeleteAtom, 1);

    // Set Window Title
    XStoreName(gpDisplay, window, "Nikhil Sathe's XWINDOW");

    // Map the window to show it.
    XMapWindow(gpDisplay, window);

    // Centering of WIndow
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

    // GAMELOOP.
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
                        printf("KeyPressed F or f\n");
                        if(bFullscreen == false)
                            { 
                                bFullscreen = true; 
                                toggleFullScreen();
                            }
                        else 
                            { 
                                bFullscreen = false; 
                                toggleFullScreen();
                            }

                    case 'w':
                        printf("Moving Forward %f \n ", camz);
                        camz += 1.0f;
                    case 's':
                        printf("Moving Backward %f \n ", camz);
                        camz -= 1.0f;
                    case 'a':
                        printf("Moving Y up %f \n ", camy);
                        camy += 1.0f;
                    case 'd':
                        printf("Moving Y down %f \n ", camy);
                        camy -= 1.0f;
                        break;
                    default:
                        break;
                }

                break;
            case ButtonPress:
                switch(event.xbutton.button)
                {
                    case 1: // LEFT
                        break;
                    case 2: // MID
                        break;
                    case 3: // RIGHT
                        break;
                }
                break;
            case 33:
                bDone = True;
                break;
            default :
                break;
        }
    }

    // Rendering
    if(bActiveWindow == True)
    {
        display();
        update();
    }
    }

    uninitialize();

    return(0);
}    
void printGLInfo(void)
{
	// Variable Declarations
	GLint numExtensions, i;
	// code

	// Print OpenGL Information
	fprintf(gpFile, "OPENGL INFORMATION\n");
	fprintf(gpFile, "******************\n");
	fprintf(gpFile, "OpenGL Vendor : %s\n", glGetString(GL_VENDOR));
	fprintf(gpFile, "OpenGL Renderer : %s\n", glGetString(GL_RENDERER));
	fprintf(gpFile, "OpenGL Version : %s\n", glGetString(GL_VERSION));
	fprintf(gpFile, "GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
	fprintf(gpFile, "Total number of OpenGL Extensions : %d\n", numExtensions);

	for(i = 0; i < numExtensions; i++){fprintf(gpFile, "%s \n", glGetStringi(GL_EXTENSIONS, i));}
	fprintf(gpFile, "+++++ %s +++++ \n", glGetString(numExtensions));
	fprintf(gpFile, "******************\n");
}

int initialize(void)
{
	// function declarations
	void printGLInfo(void);
	void resize(int, int);

    void printGLInfo(void);

    

    GLenum glewResult;

    /* Load the glXCreateContextAttribsARB function pointer if available */
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    if (glXCreateContextAttribsARB)
    {
        /* Request a 4.6 core profile context */
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 6,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        glxContext = glXCreateContextAttribsARB(gpDisplay, glxFBConfig, 0, True, attribs);
        if (glxContext)
        {
            fprintf(gpFile, "got GLX Context 4.6\n");
        }
        else
        {
            fprintf(gpFile, "glXCreateContextAttribsARB returned NULL for 4.6, will try fallback\n");
        }
    }
    else
    {
        fprintf(gpFile, "glXCreateContextAttribsARB not available, will try fallback\n");
    }

    /* Fallback: try to create a legacy context if ARB context creation failed */
    if (!glxContext)
    {
        /* Try glXCreateNewContext with GLX_RGBA_TYPE */
        glxContext = glXCreateNewContext(gpDisplay, glxFBConfig, GLX_RGBA_TYPE, 0, True);
        if (!glxContext)
        {
            /* Last resort: try the old glXCreateContext using visualInfo (if available) */
            if (visualInfo)
            {
                glxContext = glXCreateContext(gpDisplay, visualInfo, 0, True);
            }
        }

        if (glxContext)
        {
            fprintf(gpFile, "Created fallback GLX context\n");
        }
        else
        {
            fprintf(gpFile, "Failed to create any GLX context\n");
            return -1;
        }
    }
    
    glXMakeCurrent(gpDisplay, window, glxContext);

    // Initialize GLEW
	glewResult = glewInit();
	if (glewResult != GLEW_OK)
	{
		fprintf(gpFile, "glewInit() Failed");
		return(-6);
	}	
	
	printGLInfo();

    // Load shader source from files
    //const GLchar* vertexShaderSource = readShaderFile("core/shaders/main_vs.glsl");
    const GLchar* tessellationControlShaderSource = readShaderFile("main_tcs.glsl");
    const GLchar* tessellationEvaluationShaderSource = readShaderFile("main_tes.glsl");
    const GLchar* vertexShaderSource = readShaderFile("user/svs.glsl");
    //const GLchar* fragmentShaderSource = readShaderFile("core/shaders/main_fs[lambart].glsl");
    const GLchar* fragmentShaderSource = readShaderFile("user/sfs.glsl");


    if (!vertexShaderSource || !fragmentShaderSource)
    {
        fprintf(gpFile, "Failed to load shader source files\n");
        if (vertexShaderSource) free((void*)vertexShaderSource);
        if (tessellationControlShaderSource) free((void*)vertexShaderSource);
        if (tessellationEvaluationShaderSource) free((void*)vertexShaderSource);
        if (fragmentShaderSource) free((void*)fragmentShaderSource);
        return (-7);
    }

    // Create shader program
    mainShaderProgram = (ShaderProgram*)malloc(sizeof(ShaderProgram));
    mainShaderProgram->id = 0;
    mainShaderProgram->shaderCount = 0;

    Shader* vertexShader = shaderCompile(vertexShaderSource, GL_VERTEX_SHADER);
    Shader* tessControlShader = shaderCompile(tessellationControlShaderSource, GL_TESS_CONTROL_SHADER);
    Shader* tessEvalShader = shaderCompile(tessellationEvaluationShaderSource, GL_TESS_EVALUATION_SHADER);
    Shader* fragmentShader = shaderCompile(fragmentShaderSource, GL_FRAGMENT_SHADER);
    
    if (!vertexShader || !fragmentShader) {
        fprintf(gpFile, "Shader compilation failed\n");
        return -1;
    }

    Shader* shaders[] = {vertexShader, tessControlShader, tessEvalShader, fragmentShader};
    const char* attribNames[] = {"aPosition", "aNormal", "aColor", "aTexCoord"};
    GLint attribIndices[] = {ATTRIB_POSITION, ATTRIB_NORMAL, ATTRIB_COLOR, ATTRIB_TEXCOORD};
    
    if (!shaderLink(shaders, 4, mainShaderProgram, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Shader linking failed\n");
        return -1;
    }

    // Load model
    /*if (!loadModel("user/models/model.fbx", &sceneMeshes, &meshCount, 1.0f)) {
        fprintf(gpFile, "Failed to load model\n");
        // Continue without model for now
    }*/

    //GLenum types = {GL_VERTEX, GL_FRAGMENT_SHADER};
    const char* attribs[] = {"aPosition", "aNormal", "aColor", "aTexCoord"};
    GLint indices[] = {ATTRIB_POSITION, ATTRIB_NORMAL, ATTRIB_COLOR, ATTRIB_TEXCOORD};
    //buildShaderProgram(ShaderSources, types, 2, &program, attribs, indices, 4);
    //free(ShaderSources);

    // Terrian
    terrainMesh = createTerrainMesh();
    
    loadPNGTexture(&HeightMap, const_cast<char*>("heightmap.png"), 4,1);

    // Set VS uniforms
    setUniforms();

	// Depth Related Code
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
    //glEnable(GL_CULL_FACE);


	// From Here onwards OpenGL Starts
	// Tell OpenGl To Chose the color to clear the screen
	glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
	
    perspectiveProjectionMatrix = mat4::identity();
    //viewMatrix = vmath::lookat(vec3(0.0f, 2.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
	// Warmup Resize means dummy resize
	resize(winwidth, winheight);

	return(0);
}





void toggleFullScreen(void)
{
    // Code
    printf("FS func");
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

    // Send above event to XSERVER.
    XSendEvent(gpDisplay, XRootWindow(gpDisplay, visualInfo->screen), False,
            SubstructureNotifyMask, &fsevent);
}

void resize(int width, int height)
{
	// code

	// if height accidently becomes 0 or less than 1 make it 1
	if (height <= 0)
	{
		height = 1;
	}
	
	// Set the ViewPort
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	// Perspective Projection Matrix
	perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 10000.0f);
	
}

void display(void)
{
	// code

    viewMatrix = vmath::lookat(vec3(0.0f, camy, camz), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
	// Clear OpenGL Buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer(rotationAngle, HeightMap);
    
	// Swap THe Buffers
    glXSwapBuffers(gpDisplay, window);
}

void update(void)
{
	// code
    rotationAngle += 0.5f;
    if (rotationAngle >= 360.0f) {
        rotationAngle = 0.0f;
    }
}

void uninitialize(void)
{

    if (window)
    {
        XDestroyWindow(gpDisplay, window);
    }

    if (colormap)
    {
        XFreeColormap(gpDisplay, colormap);
    }

    if (gpDisplay)
    {
        XCloseDisplay(gpDisplay);
        gpDisplay = NULL;
    }

    /*// Free VBO
	if (vbo_position)
	{
		glDeleteBuffers(1, &vbo_position);
		vbo_position = 0;
	}

	if (vbo_color)
	{
		glDeleteBuffers(1, &vbo_color);
		vbo_color = 0;
	}

	// Free VAO
	if (VAO)
	{
		glDeleteVertexArrays(1, &VAO);
		VAO = 0;
	}*/

	// Detach, Delete Shader Objects and Delete Shader Program Object
	/*if (iShaderProgramObject)
	{
		glUseProgram(iShaderProgramObject);

		GLint iShaderCount;
		glGetProgramiv(iShaderProgramObject, GL_ATTACHED_SHADERS, &iShaderCount);
		if (iShaderCount > 0)
		{
			GLuint *pShaders = (GLuint *)malloc(iShaderCount * sizeof(GLuint));
			if (pShaders != NULL)
			{
				glGetAttachedShaders(iShaderProgramObject, iShaderCount, &iShaderCount, pShaders);
				for (int i = 0; i < iShaderCount; i++)
				{
					glDetachShader(iShaderProgramObject, pShaders[i]);
					glDeleteShader(pShaders[i]);
					pShaders[i] = 0;
				}
				free(pShaders);
				pShaders = NULL;
			}
		}

		glDeleteProgram(iShaderProgramObject);
		iShaderProgramObject = 0;
	}*/
    // In your cleanup function
    /*if (terrainMesh) {
    freeMesh(terrainMesh);
    terrainMesh = NULL;
    }*/

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
        /* visualInfo was allocated by X (glXGetVisualFromFBConfig); free it with XFree */
        XFree(visualInfo);
         visualInfo = NULL;
    }
    // close the file
    if (gpFile)
    {
	fprintf(gpFile, "Program terminated successfully...!!!");
	fclose(gpFile);
	gpFile = NULL;
    }
}
