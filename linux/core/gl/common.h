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
#include "structs.h"

// Global scene data
ShaderProgram* mainShaderProgram = NULL;
Mesh* sceneMeshes = NULL;

// Matrices
mat4 perspectiveProjectionMatrix;
mat4 viewMatrix;
int meshCount = 0;

// variables related with file I/O
char gszLogFileName[] = "log.txt";
FILE *gpFile = NULL;

#include "shaders.cpp"
#include "texture.cpp"
#include "modelloading.cpp"
#include "renderer.cpp"
