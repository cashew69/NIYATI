

// Standard header files.
#include <stdio.h> // for printf
#include <stdlib.h> // for exit
#include <memory.h> // for memset
#include <string.h>
#include <ctime>

#include <time.h>
#include "vmath.h"
using namespace vmath;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Include transform system before structs
#include "engine/transform.h"
#include "core/gl/structs.h"

// Global scene data
ShaderProgram* mainShaderProgram = NULL;
ShaderProgram* lineShaderProgram = NULL;

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

// Ship State (Externs)
vec3 shipPosition;
vec3 shipRotation;
vec3 shipForward;
vec3 shipUp;
vec3 shipRight;



#include "engine/transform.cpp"
#include "core/gl/shaders.cpp"
#include "core/gl/texture.cpp"
#include "core/gl/modelloading.cpp"
#include "core/gl/camera.cpp"
#include "engine/perlin/perlin.c"
#include "user/userrendercalls.cpp"
#include "core/gl/renderer.cpp"
//#include "inputhandler.c"
