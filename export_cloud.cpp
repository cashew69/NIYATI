// Jai Shree Ram!!!

// Standard header files.
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <math.h>
#include <chrono>

// Xlib header files.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

// OpenGL related header files.
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>

// VMATH
#include "vmath.h"
using namespace vmath;

// Macros
#define winwidth 1280
#define winheight 720

// Global Variables
Display* gpDisplay = NULL;
XVisualInfo* visualInfo = NULL;
Window window;
Colormap colormap;
bool bFullscreen = false;
Bool bActiveWindow = False;

typedef GLXContext(*glXCreateContextAttribsARBProc)
(Display*, GLXFBConfig, GLXContext, Bool, const int*);

glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
GLXFBConfig glxFBConfig;

// OpenGL related vars.
GLXContext glxContext = NULL;

// variables related with file I/O
char gszLogFileName[] = "log.txt";
FILE* gpFile = NULL;

mat4 perspectiveProjectionMatrix;

// ----------------------------------------------------------------------------
// Volumetric Cloud Shaders
// ----------------------------------------------------------------------------

const GLchar* computeShaderSourceCode = R"glsl(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// binding 0 = current frame write target
layout(rgba16f, binding = 0) writeonly uniform image2D u_outputImage;
// binding 1 = previous frame history (read-only)
layout(rgba16f, binding = 1) readonly  uniform image2D u_historyImage;

layout(std430, binding = 1) readonly buffer CloudSpheresBuffer {
    vec4 spheres[];
};

uniform int   u_sphereCount;
uniform vec3  u_cameraPos;
uniform mat4  u_view;
uniform mat4  u_proj;
uniform mat4  u_worldMatrix;
uniform float u_time;

uniform float u_densityScale;
uniform int   u_maxSteps;
uniform float u_stepSize;
uniform float u_turbulence;
uniform float u_windSpeed;
uniform vec3  u_boxMin;
uniform vec3  u_boxMax;

uniform vec3  u_sunColor;
uniform float u_sunIntensity;
uniform float u_ambientStrength;
uniform float u_scatterG;

// TAA
uniform int   u_enableTAA;
uniform int   u_frameIndex;
uniform float u_taaBlend;   // 0.05–0.5

// ---- Noise ----
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash(i+vec3(0,0,0)), hash(i+vec3(1,0,0)), f.x),
            mix(hash(i+vec3(0,1,0)), hash(i+vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i+vec3(0,0,1)), hash(i+vec3(1,0,1)), f.x),
                   mix(hash(i+vec3(0,1,1)), hash(i+vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 p, int oct) {
    float v = 0.0, w = 0.5;
    for (int i = 0; i < oct; i++) { v += w * noise3(p); p *= 2.0; w *= 0.5; }
    return v;
}

// ---- SDF ----
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

float nodeScale() {
    return length(vec3(u_worldMatrix[0]));
}

float sampleSDF(vec3 worldP) {
    if (u_sphereCount == 0) return 9999.0;
    float sc = nodeScale();
    vec3  c0 = (u_worldMatrix * vec4(spheres[0].xyz, 1.0)).xyz;
    float d  = length(worldP - c0) - spheres[0].w * sc;
    for (int i = 1; i < u_sphereCount; i++) {
        vec3  ci = (u_worldMatrix * vec4(spheres[i].xyz, 1.0)).xyz;
        float di = length(worldP - ci) - spheres[i].w * sc;
        d = smin(d, di, 2.1 * sc);
    }
    return d;
}

// ---- Cloud density ----
float cloudDensity(vec3 p) {
    float sdf = sampleSDF(p);
    if (sdf > 3.5) return 0.0;

    vec3 wind = vec3(u_windSpeed, 0.0, u_windSpeed * 0.5) * u_time;
    float disp = fbm((p + wind) * 0.1, 5) * 2.0 * u_turbulence;
    float dsdf = sdf - disp;
    if (dsdf > 0.0) return 0.0;

    float profile = clamp(-dsdf / 4.0, 0.0, 1.0);
    float n = fbm(p * 0.95 + wind * 0.3, 8);
    float wispy  = n;
    float billowy = 1.0 - abs(n * 2.0 - 1.0);
    float nc = mix(wispy, billowy, smoothstep(0.0, 1.0, profile));
    return smoothstep(0.0, 0.25, profile - nc * 0.55) * u_densityScale;
}

// ---- Lighting ----
vec3 sunDir() {
    return normalize(vec3(cos(u_time * 0.5), 0.2, sin(u_time * 0.5)));
}

float PhaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(max(1.0 + g2 - 2.0*g*cosTheta, 1e-4), 1.5) * 0.079577;
}

float lightMarch(vec3 p) {
    vec3 sd = sunDir();
    float d = 0.0, step = 5.0;
    for (int i = 0; i < 4; i++) {
        p += sd * step;
        d += cloudDensity(p) * step;
        step *= 1.5;
    }
    return d;
}

// ---- Ray-AABB ----
bool rayAABB(vec3 ro, vec3 rd, vec3 bMin, vec3 bMax, out float tNear, out float tFar) {
    vec3 inv = 1.0 / rd;
    vec3 t0  = (bMin - ro) * inv;
    vec3 t1  = (bMax - ro) * inv;
    vec3 tMn = min(t0, t1);
    vec3 tMx = max(t0, t1);
    tNear = max(max(tMn.x, tMn.y), tMn.z);
    tFar  = min(min(tMx.x, tMx.y), tMx.z);
    return tFar > max(tNear, 0.0);
}

// ---- Full raymarch for one pixel ----
vec4 raymarchCloud(vec3 rayOrigin, vec3 rayDir) {
    float tNear, tFar;
    if (!rayAABB(rayOrigin, rayDir, u_boxMin, u_boxMax, tNear, tFar)) {
        return vec4(0.0);
    }
    tNear = max(tNear, 0.001);
    float rayLen = min(tFar - tNear, 2000.0);

    vec3  sd       = sunDir();
    float cosTheta = dot(rayDir, sd);
    float phase    = PhaseHG(cosTheta, u_scatterG) * 0.7 + PhaseHG(cosTheta, -0.1) * 0.3;

    vec3  sunLum = u_sunColor * u_sunIntensity;
    vec3  ambLum = vec3(0.3, 0.5, 0.8) * u_ambientStrength;

    float transmittance = 1.0;
    vec3  color         = vec3(0.0);
    float t             = 0.0;
    float dt            = u_stepSize;
    int   maxS          = min(u_maxSteps, 256);

    for (int i = 0; i < maxS; i++) {
        if (t >= rayLen) break;

        vec3  pos     = rayOrigin + rayDir * (tNear + t);
        float density = cloudDensity(pos);

        if (density > 0.001) {
            float ext   = density * 0.12;
            float stepT = exp(-ext * dt);
            float lDen  = lightMarch(pos);

            vec3  shadow  = exp(-lDen * 0.12 * vec3(0.95, 0.97, 1.0));
            float depth   = clamp(-sampleSDF(pos) / 30.0, 0.0, 1.0);
            vec3  ms      = exp(-lDen * 0.02 * vec3(0.95, 0.97, 1.0)) * 0.35 * depth;

            vec3  direct  = sunLum * (shadow + ms) * phase;
            vec3  ambient = ambLum * (0.5 + 0.5 * (1.0 - depth));

            color         += (direct + ambient) * density * transmittance * dt * 0.12;
            transmittance *= stepT;

            if (transmittance < 0.01) break;
            t += dt;
        } else {
            float sdist = max(sampleSDF(pos) - 1.5, dt);
            t += sdist * 0.5;
        }
    }

    color = color / (1.0 + color);
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    return vec4(color, clamp(1.0 - transmittance, 0.0, 1.0));
}

// ---- Main ----
void main() {
    ivec2 pixel   = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(u_outputImage);

    if (pixel.x >= imgSize.x || pixel.y >= imgSize.y) return;

    if (u_enableTAA != 0) {
        uint px      = uint(pixel.x);
        uint py      = uint(pixel.y);
        uint pattern = (px + py * 3u + uint(u_frameIndex)) % 4u;
        if (pattern != 0u) {
            imageStore(u_outputImage, pixel, imageLoad(u_historyImage, pixel));
            return;
        }
    }

    vec2 uv  = (vec2(pixel) + 0.5) / vec2(imgSize);
    vec2 ndc = uv * 2.0 - 1.0;

    mat4 invProj = inverse(u_proj);
    mat4 invView = inverse(u_view);

    vec4 vD = invProj * vec4(ndc, -1.0, 1.0);
    vD.xyz /= vD.w;

    vec3 rayDir    = normalize((invView * vec4(vD.xyz, 0.0)).xyz);
    vec3 rayOrigin = u_cameraPos;

    vec4 result = raymarchCloud(rayOrigin, rayDir);

    if (u_enableTAA != 0) {
        vec4 history = imageLoad(u_historyImage, pixel);
        result = mix(history, result, u_taaBlend);
    }

    imageStore(u_outputImage, pixel, result);
}
)glsl";

const GLchar* quadVertexShaderSourceCode = R"glsl(
#version 460 core
out vec2 vTexCoord;

void main() {
    const vec2 verts[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
    vTexCoord   = verts[gl_VertexID] * 0.5 + 0.5;
}
)glsl";

const GLchar* quadFragmentShaderSourceCode = R"glsl(
#version 460 core
in  vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_cloudTex;
void main() {
    FragColor = texture(u_cloudTex, vTexCoord);
}
)glsl";

// ----------------------------------------------------------------------------
// Cloud Engine Structures & State
// ----------------------------------------------------------------------------

struct VolumetricCloudData {
    GLuint outputTex;
    GLuint historyTex;
    GLuint sphereSSBO;
    GLuint emptyVAO;
    GLuint computeProg;
    GLuint quadProg;

    float* sphereData;
    int sphereCount;

    int outputW, outputH;
    float renderScale;

    // Cloud Params
    vec3 sunColor;
    float sunIntensity;
    float ambientStrength;
    float scatterG;
    float densityScale;
    int maxSteps;
    float stepSize;
    float turbulence;
    float windSpeed;

    vec3 boxSize;
    int gridX, gridZ;
    float gridSpacing;
    float gridScale;
    int spheresPerCloudMin, spheresPerCloudMax;
    bool spheresDirty;

    mat4 worldMatrix;

    bool enableTAA;
    int frameIndex;
    float taaBlend;
};

VolumetricCloudData gCloud;
auto gStartTime = std::chrono::high_resolution_clock::now();

// ----------------------------------------------------------------------------
// Function Prototypes
// ----------------------------------------------------------------------------
int initialize(void);
void resize(int, int);
void display(void);
void update(void);
void toggleFullScreen(void);
void uninitialize(void);
void printGLInfo(void);
void checkShaderCompileError(GLuint shader, const char* type);
void checkProgramLinkError(GLuint program, const char* type);

// Cloud Engine Helper Functions
GLuint makeRGBA16FTex(int w, int h) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    static const float zero[4] = {};
    glClearTexImage(id, 0, GL_RGBA, GL_FLOAT, zero);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

void reallocateTextures() {
    if (gCloud.outputTex) glDeleteTextures(1, &gCloud.outputTex);
    if (gCloud.historyTex) glDeleteTextures(1, &gCloud.historyTex);
    gCloud.outputTex = makeRGBA16FTex(gCloud.outputW, gCloud.outputH);
    gCloud.historyTex = makeRGBA16FTex(gCloud.outputW, gCloud.outputH);
    gCloud.frameIndex = 0;
}

void generateSpheres() {
    srand((unsigned int)time(NULL));

    if (!gCloud.sphereData) {
        gCloud.sphereData = (float*)malloc(256 * 4 * sizeof(float));
    }
    gCloud.sphereCount = 0;

    float startX = -(gCloud.gridX - 1) * gCloud.gridSpacing * 0.5f;
    float startZ = -(gCloud.gridZ - 1) * gCloud.gridSpacing * 0.5f;

    int perMin = gCloud.spheresPerCloudMin;
    int perMax = gCloud.spheresPerCloudMax;
    if (perMin > perMax) perMin = perMax;

    auto rnd = [](float lo, float hi) -> float {
        return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
    };

    for (int gx = 0; gx < gCloud.gridX && gCloud.sphereCount < 255; gx++) {
        for (int gz = 0; gz < gCloud.gridZ && gCloud.sphereCount < 255; gz++) {
            float cx = startX + gx * gCloud.gridSpacing;
            float cy = 4.0f;
            float cz = startZ + gz * gCloud.gridSpacing;

            int cnt = perMin + (rand() % (perMax - perMin + 1));
            if (gCloud.sphereCount + cnt > 256) cnt = 256 - gCloud.sphereCount;
            if (cnt <= 0) break;

            int numLow = (int)(cnt * 0.6f);
            if (numLow < 2 && cnt >= 3) numLow = 2;
            int numUp = cnt - numLow;

            for (int k = 0; k < numLow && gCloud.sphereCount < 256; k++) {
                float* s = &gCloud.sphereData[gCloud.sphereCount * 4];
                s[0] = cx + rnd(-7.5f, 7.5f) * gCloud.gridScale;
                s[1] = cy + rnd(-0.25f, 0.25f) * gCloud.gridScale;
                s[2] = cz + rnd(-1.5f, 1.5f) * gCloud.gridScale;
                s[3] = (2.0f + rnd(0.0f, 1.0f)) * gCloud.gridScale;
                gCloud.sphereCount++;
            }
            for (int k = 0; k < numUp && gCloud.sphereCount < 256; k++) {
                float* s = &gCloud.sphereData[gCloud.sphereCount * 4];
                s[0] = cx + rnd(-6.5f, 6.5f) * gCloud.gridScale;
                s[1] = cy + rnd(2.0f, 3.0f) * gCloud.gridScale;
                s[2] = cz + rnd(-1.0f, 1.0f) * gCloud.gridScale;
                s[3] = (1.0f + rnd(0.0f, 0.6f)) * gCloud.gridScale;
                gCloud.sphereCount++;
            }
        }
    }
    gCloud.spheresDirty = true;
}

void uploadSpheres() {
    if (!gCloud.spheresDirty || gCloud.sphereCount == 0) return;

    size_t bytes = (size_t)gCloud.sphereCount * 4 * sizeof(float);

    if (!gCloud.sphereSSBO) {
        glGenBuffers(1, &gCloud.sphereSSBO);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gCloud.sphereSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, gCloud.sphereData, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    gCloud.spheresDirty = false;
}

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------

int main(void) {
    gpFile = fopen(gszLogFileName, "w");
    if (gpFile == NULL) {
        printf("Log file creation failed...!!!\n");
        exit(0);
    } else {
        setvbuf(gpFile, NULL, _IONBF, 0);
        fprintf(gpFile, "Program started successfully...!!!\n");
    }

    int defaultDepth;
    Atom windowManagerDeleteAtom;
    XEvent event;
    Screen* screen = NULL;
    int screenWidth, screenHeight;
    KeySym keysym;
    char keys[52];

    GLXFBConfig* pGLXFBConfig;
    GLXFBConfig bestFBConfig;
    XVisualInfo* pXVisualInfo;
    int iNumFBConfigs = 0;

    int frameBufferAttribute[] = {
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
    if (gpDisplay == NULL) {
        printf("xOpenDisplay failed to connect with Xserver: gpDisplay = xOpenDisplay(NULL)");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    int defaultScreen = XDefaultScreen(gpDisplay);
    defaultDepth = XDefaultDepth(gpDisplay, defaultScreen);

    pGLXFBConfig = glXChooseFBConfig(gpDisplay, defaultScreen, frameBufferAttribute, &iNumFBConfigs);
    if (pGLXFBConfig == NULL) {
        fprintf(gpFile, "glxChooseFBConfig failed");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    int indexOfBestFBConfig = -1;
    int bestFBConfigScore = -1;
    int bestNumberOfSamples = -1;
    int worstNumberOfSamples = -1;

    for (int i = 0; i < iNumFBConfigs; i++) {
        pXVisualInfo = glXGetVisualFromFBConfig(gpDisplay, pGLXFBConfig[i]);
        if (pXVisualInfo) {
            int sampleBuffers, samples;
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLE_BUFFERS, &sampleBuffers);
            glXGetFBConfigAttrib(gpDisplay, pGLXFBConfig[i], GLX_SAMPLES, &samples);

            if (999 > bestNumberOfSamples && samples > bestNumberOfSamples) {
                bestFBConfigScore = 1;
                bestNumberOfSamples = samples;
                indexOfBestFBConfig = i;
            }
            if (worstNumberOfSamples < 0 || samples < worstNumberOfSamples) {
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
    memset((void*)&windowAttributes, 0, sizeof(XSetWindowAttributes));

    windowAttributes.border_pixel = 0;
    windowAttributes.background_pixmap = 0;
    windowAttributes.background_pixel = XBlackPixel(gpDisplay, visualInfo->screen);
    windowAttributes.event_mask = KeyPressMask | ButtonPressMask | FocusChangeMask | StructureNotifyMask | ExposureMask;
    Window root = XRootWindow(gpDisplay, visualInfo->screen);
    windowAttributes.colormap = XCreateColormap(gpDisplay, root, visualInfo->visual, AllocNone);

    colormap = windowAttributes.colormap;

    window = XCreateWindow(
        gpDisplay, root, 0, 0, winwidth, winheight, 2, visualInfo->depth, InputOutput,
        visualInfo->visual, CWBorderPixel | CWBackPixel | CWEventMask | CWColormap,
        &windowAttributes
    );

    if (!window) {
        printf("XCreateWindow failed");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    windowManagerDeleteAtom = XInternAtom(gpDisplay, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(gpDisplay, window, &windowManagerDeleteAtom, 1);
    XStoreName(gpDisplay, window, "Nikhil Sathe's XWINDOW - Volumetric Clouds Compute");
    XMapWindow(gpDisplay, window);

    screen = XScreenOfDisplay(gpDisplay, visualInfo->screen);
    screenWidth = XWidthOfScreen(screen);
    screenHeight = XHeightOfScreen(screen);
    XMoveWindow(gpDisplay, window, (screenWidth / 2 - winwidth / 2), (screenHeight / 2 - winheight / 2));

    int iResult = initialize();
    if (iResult != 0) {
        fprintf(gpFile, "initialize() FAILED\n");
        uninitialize();
        exit(EXIT_FAILURE);
    }

    while (bDone == False) {
        while (XPending(gpDisplay)) {
            XNextEvent(gpDisplay, &event);
            switch (event.type) {
                case MapNotify: break;
                case FocusIn: bActiveWindow = True; break;
                case FocusOut: bActiveWindow = False; break;
                case ConfigureNotify:
                    resize(event.xconfigure.width, event.xconfigure.height);
                    break;
                case KeyPress:
                    keysym = XkbKeycodeToKeysym(gpDisplay, event.xkey.keycode, 0, 0);
                    switch (keysym) {
                        case XK_Escape:
                            bDone = True;
                            break;
                        case XK_T:
                        case XK_t:
                            gCloud.enableTAA = !gCloud.enableTAA;
                            break;
                    }
                    XLookupString(&event.xkey, keys, sizeof(keys), NULL, NULL);
                    if (keys[0] == 'F' || keys[0] == 'f') {
                        bFullscreen = !bFullscreen;
                        toggleFullScreen();
                    }
                    break;
                        case 33: bDone = True; break;
            }
        }
        if (bActiveWindow == True) {
            display();
            update();
        }
    }

    uninitialize();
    return(0);
}

void printGLInfo(void) {
    GLint numExtensions, i;
    fprintf(gpFile, "OPENGL INFORMATION\n******************\n");
    fprintf(gpFile, "OpenGL Vendor : %s\n", glGetString(GL_VENDOR));
    fprintf(gpFile, "OpenGL Renderer : %s\n", glGetString(GL_RENDERER));
    fprintf(gpFile, "OpenGL Version : %s\n", glGetString(GL_VERSION));
    fprintf(gpFile, "GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void checkShaderCompileError(GLuint shader, const char* type) {
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            GLchar* log = (GLchar*)malloc(length);
            glGetShaderInfoLog(shader, length, NULL, log);
            fprintf(gpFile, "BLACK BOX (%s Shader Compile Log) : %s\n", type, log);
            free(log);
        }
    }
}

void checkProgramLinkError(GLuint program, const char* type) {
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            GLchar* log = (GLchar*)malloc(length);
            glGetProgramInfoLog(program, length, NULL, log);
            fprintf(gpFile, "BLACK BOX (%s Program Link Log) : %s\n", type, log);
            free(log);
        }
    }
}

int initialize(void) {
    GLenum glewResult;

    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    if (glXCreateContextAttribsARB) {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
            GLX_CONTEXT_MINOR_VERSION_ARB, 6,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        glxContext = glXCreateContextAttribsARB(gpDisplay, glxFBConfig, 0, True, attribs);
    }

    if (!glxContext) {
        glxContext = glXCreateNewContext(gpDisplay, glxFBConfig, GLX_RGBA_TYPE, 0, True);
        if (!glxContext && visualInfo) {
            glxContext = glXCreateContext(gpDisplay, visualInfo, 0, True);
        }
    }

    if (!glxContext) return -1;

    glXMakeCurrent(gpDisplay, window, glxContext);

    glewResult = glewInit();
    if (glewResult != GLEW_OK) return(-6);

    printGLInfo();

    // 1. Compile Compute Shader
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSourceCode, NULL);
    glCompileShader(computeShader);
    checkShaderCompileError(computeShader, "Compute");

    gCloud.computeProg = glCreateProgram();
    glAttachShader(gCloud.computeProg, computeShader);
    glLinkProgram(gCloud.computeProg);
    checkProgramLinkError(gCloud.computeProg, "Compute");
    glDeleteShader(computeShader);

    // 2. Compile Quad Shaders
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &quadVertexShaderSourceCode, NULL);
    glCompileShader(vShader);
    checkShaderCompileError(vShader, "Quad Vertex");

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &quadFragmentShaderSourceCode, NULL);
    glCompileShader(fShader);
    checkShaderCompileError(fShader, "Quad Fragment");

    gCloud.quadProg = glCreateProgram();
    glAttachShader(gCloud.quadProg, vShader);
    glAttachShader(gCloud.quadProg, fShader);
    glLinkProgram(gCloud.quadProg);
    checkProgramLinkError(gCloud.quadProg, "Quad");
    glDeleteShader(vShader);
    glDeleteShader(fShader);

    // Empty VAO required for drawing without VBOs in core profile
    glGenVertexArrays(1, &gCloud.emptyVAO);

    // 3. Initialize Cloud Settings
    gCloud.outputTex = 0;
    gCloud.historyTex = 0;
    gCloud.sphereSSBO = 0;
    gCloud.sphereData = nullptr;

    gCloud.sunColor = vec3(1.0f, 0.95f, 0.85f);
    gCloud.sunIntensity = 10.0f;
    gCloud.ambientStrength = 1.5f;
    gCloud.scatterG = 0.3f;
    gCloud.densityScale = 5.0f;
    gCloud.maxSteps = 128;
    gCloud.stepSize = 0.5f;
    gCloud.turbulence = 0.5f;
    gCloud.windSpeed = 0.5f;
    gCloud.boxSize = vec3(500.0f, 150.0f, 500.0f);
    gCloud.gridX = 3;
    gCloud.gridZ = 3;
    gCloud.gridSpacing = 20.0f;
    gCloud.gridScale = 1.0f;
    gCloud.spheresPerCloudMin = 3;
    gCloud.spheresPerCloudMax = 8;
    gCloud.renderScale = 1.0f;
    gCloud.enableTAA = true;
    gCloud.taaBlend = 0.1f;
    gCloud.worldMatrix = mat4::identity();
    gCloud.frameIndex = 0;

    generateSpheres();
    uploadSpheres();

    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    resize(winwidth, winheight);

    return(0);
}

void toggleFullScreen(void) {
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

void resize(int width, int height) {
    if (height <= 0) height = 1;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    perspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 1000.0f);

    gCloud.outputW = (int)(width * gCloud.renderScale);
    gCloud.outputH = (int)(height * gCloud.renderScale);
    reallocateTextures();
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadSpheres();

    // Time calculation
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - gStartTime;
    float timeVal = elapsed.count();

    // Setup Camera Matrix
    vec3 camPos = vec3(0.0f, 20.0f, 100.0f); // Placed to view origin
    mat4 viewMatrix = vmath::lookat(camPos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    // ---- COMPUTE PASS ----
    glUseProgram(gCloud.computeProg);

    glBindImageTexture(0, gCloud.outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(1, gCloud.historyTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gCloud.sphereSSBO);

    glUniform3fv(glGetUniformLocation(gCloud.computeProg, "u_cameraPos"), 1, (float*)&camPos);
    glUniformMatrix4fv(glGetUniformLocation(gCloud.computeProg, "u_view"), 1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(glGetUniformLocation(gCloud.computeProg, "u_proj"), 1, GL_FALSE, perspectiveProjectionMatrix);
    glUniformMatrix4fv(glGetUniformLocation(gCloud.computeProg, "u_worldMatrix"), 1, GL_FALSE, gCloud.worldMatrix);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_time"), timeVal);

    glUniform1i(glGetUniformLocation(gCloud.computeProg, "u_sphereCount"), gCloud.sphereCount);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_densityScale"), gCloud.densityScale);
    glUniform1i(glGetUniformLocation(gCloud.computeProg, "u_maxSteps"), gCloud.maxSteps);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_stepSize"), gCloud.stepSize);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_turbulence"), gCloud.turbulence);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_windSpeed"), gCloud.windSpeed);

    vec3 worldPos = vec3(gCloud.worldMatrix[3][0], gCloud.worldMatrix[3][1], gCloud.worldMatrix[3][2]);
    vec3 halfBox = gCloud.boxSize * 0.5f;
    vec3 boxMin = worldPos - halfBox;
    vec3 boxMax = worldPos + halfBox;

    glUniform3fv(glGetUniformLocation(gCloud.computeProg, "u_boxMin"), 1, (float*)&boxMin);
    glUniform3fv(glGetUniformLocation(gCloud.computeProg, "u_boxMax"), 1, (float*)&boxMax);
    glUniform3fv(glGetUniformLocation(gCloud.computeProg, "u_sunColor"), 1, (float*)&gCloud.sunColor);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_sunIntensity"), gCloud.sunIntensity);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_ambientStrength"), gCloud.ambientStrength);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_scatterG"), gCloud.scatterG);

    glUniform1i(glGetUniformLocation(gCloud.computeProg, "u_enableTAA"), gCloud.enableTAA ? 1 : 0);
    glUniform1i(glGetUniformLocation(gCloud.computeProg, "u_frameIndex"), gCloud.frameIndex);
    glUniform1f(glGetUniformLocation(gCloud.computeProg, "u_taaBlend"), gCloud.taaBlend);

    GLuint groupsX = ((GLuint)gCloud.outputW + 7) / 8;
    GLuint groupsY = ((GLuint)gCloud.outputH + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // ---- QUAD RENDER PASS ----
    glUseProgram(gCloud.quadProg);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCloud.outputTex);
    glUniform1i(glGetUniformLocation(gCloud.quadProg, "u_cloudTex"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(gCloud.emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    if (gCloud.enableTAA) {
        GLuint tmp = gCloud.outputTex;
        gCloud.outputTex = gCloud.historyTex;
        gCloud.historyTex = tmp;
    }

    gCloud.frameIndex++;

    glXSwapBuffers(gpDisplay, window);
}

void update(void) {}

void uninitialize(void) {
    if (gCloud.outputTex) glDeleteTextures(1, &gCloud.outputTex);
    if (gCloud.historyTex) glDeleteTextures(1, &gCloud.historyTex);
    if (gCloud.sphereSSBO) glDeleteBuffers(1, &gCloud.sphereSSBO);
    if (gCloud.sphereData) free(gCloud.sphereData);
    if (gCloud.emptyVAO) glDeleteVertexArrays(1, &gCloud.emptyVAO);
    if (gCloud.computeProg) glDeleteProgram(gCloud.computeProg);
    if (gCloud.quadProg) glDeleteProgram(gCloud.quadProg);

    if (window) XDestroyWindow(gpDisplay, window);
    if (colormap) XFreeColormap(gpDisplay, colormap);
    if (gpDisplay) {
        XCloseDisplay(gpDisplay);
        gpDisplay = NULL;
    }

    GLXContext currentContext = glXGetCurrentContext();
    if (currentContext && currentContext == glxContext) {
        glXMakeCurrent(gpDisplay, 0, 0);
    }
    if (glxContext) glXDestroyContext(gpDisplay, glxContext);
    if (visualInfo) {
        XFree(visualInfo);
        visualInfo = NULL;
    }

    if (gpFile) {
        fprintf(gpFile, "Program terminated successfully...!!!");
        fclose(gpFile);
        gpFile = NULL;
    }
}
