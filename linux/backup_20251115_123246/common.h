// Standard header files.
#include <stdio.h> // for printf
#include <stdlib.h> // for exit
#include <memory.h> // for memset
#include <string.h>

// Xlib header files.
#include <X11/Xlib.h> // for all xlib api's
#include <X11/Xutil.h> // for visual info and related api.
#include <X11/XKBlib.h> // for Keyboard related.

#include "vmath.h"

using namespace vmath;

// OpenGL related header files. 
#include <GL/glew.h> // Must be before gl.h
#include <GL/gl.h>
#include <GL/glx.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Include transform system before structs
#include "core/gl/structs.h"
#include "engine/transform.h"

// Global scene data
ShaderProgram* mainShaderProgram = NULL;
ShaderProgram* tessellationShaderProgram = NULL;
Mesh* sceneMeshes = NULL;

// Matrices
mat4 perspectiveProjectionMatrix;
mat4 viewMatrix;
int meshCount = 0;

// variables related with file I/O
char gszLogFileName[] = "log.txt";
FILE *gpFile = NULL;

// Global Uniform Variables
GLint projLocUniform;
GLint viewLocUniform;
GLint modelLocUniform;
GLint lightPosLocUniform;
GLint lightColorLocUniform;
GLint viewPosLocUniform;
GLint colorTextureLocUniform;

Mesh* terrainMesh = NULL;
Mesh* planeMesh = NULL;


#include "engine/transform.cpp"
#include "core/gl/shaders.cpp"
#include "core/gl/texture.cpp"
#include "core/gl/modelloading.cpp"
#include "core/gl/camera.cpp"
#include "engine/perlin/perlin.c"
#include "user/terrain.cpp"
#include "user/userrendercalls.cpp"
#include "core/gl/renderer.cpp"
//#include "inputhandler.c"
