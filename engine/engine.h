

// Standard header files.
#include <ctime>
#include <memory.h> // for memset
#include <stdio.h>  // for printf
#include <stdlib.h> // for exit
#include <string.h>

#include "dependancies/vmath.h"
#include <time.h>
using namespace vmath;
#include "platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include "dependancies/stb_image.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// Include transform system before structs
#include "core/gl/structs.h"
#include "transform.h"

// ============================================================================
// ENGINE GLOBALS
// ============================================================================

// Shader Programs
ShaderProgram *mainShaderProgram = NULL;
ShaderProgram *lineShaderProgram = NULL;
ShaderProgram *tessellationShaderProgram = NULL;
ShaderProgram *pbrShaderProgram = NULL;

// Scene Meshes
Mesh *sceneMeshes = NULL;
int meshCount = 0;
Mesh *helmetMeshes = NULL;
int helmetMeshCount = 0;
Mesh *terrainMesh = NULL;
Mesh *planeMesh = NULL;

// Matrices
mat4 perspectiveProjectionMatrix;
mat4 viewMatrix;

// File I/O
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

// ============================================================================
// ENGINE CORE IMPLEMENTATIONS
// ============================================================================
#include "core/gl/texture.cpp"
#include "core/gl/shaders.cpp"
#include "core/gl/camera.cpp"
#include "core/gl/modelloading.cpp"
#include "effects/perlin/perlin.c"
#include "transform.cpp"
