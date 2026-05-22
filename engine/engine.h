#pragma once

// Standard header files.
#include <ctime>
#include <memory.h> // for memset
#include <stdio.h>  // for printf
#include <stdlib.h> // for exit
#include <string.h>

#include "dependancies/vmath.h"
#include <time.h>
using namespace vmath;
#include <GL/glew.h>
#include <GL/gl.h>
#include "platform.h"


#define STB_IMAGE_IMPLEMENTATION
#include "dependancies/stb_image.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// Include transform system before structs
#include "core/gl/structs.h"
#include "transform.h"

// Standard shader attribute slots — used by all projects for buildShaderProgram / createVAO_VBO
extern const char* attribNames[4];
extern GLint attribIndices[4];

enum SceneSelectedType {
    SEL_NONE = 0,
    SEL_CUSTOM_CAM = 1,
    SEL_MODEL = 3,
    SEL_LIGHT = 4,
    SEL_REF_OBJ = 5,
    SEL_SCENENODE = 6
};

enum SceneSelectedType g_SceneSelectedType;
int g_SceneSelectedIndex;

// ============================================================================
// ENGINE GLOBALS
// ============================================================================

// Shader Programs
ShaderProgram *lambertShaderProgram = NULL;
ShaderProgram *lineShaderProgram = NULL;
ShaderProgram *tessellationShaderProgram = NULL;
ShaderProgram *pbrShaderProgram = NULL;
ShaderProgram *VolumeRenderingProgram = NULL;
ShaderProgram *instancedProgram = NULL;
ShaderProgram *iconShaderProgram = NULL;
ShaderProgram *instancedShadowProgram = NULL;

extern bool   g_ShadowActive;
extern mat4   g_ShadowSBPV;
extern GLuint g_ShadowDepthTexID;
extern float  g_ShadowBias;

#include "core/logger.h"

// Scene Content
Model *sceneModels = NULL;
int sceneModelCount = 0;


SceneNode* g_SceneRoot = NULL;
SceneNode* g_SelectedSceneNode = NULL;

// Matrices
mat4 perspectiveProjectionMatrix;
mat4 viewMatrix;

// File I/O
char gszLogFileName[] = "log.txt";

// Global Uniform Variables
GLint projLocUniform;
GLint viewLocUniform;
GLint modelLocUniform;
GLint lightPosLocUniform;
GLint lightColorLocUniform;
GLint viewPosLocUniform;
GLint colorTextureLocUniform;

// Common Rendering Globals (used by Terrain, PBR, etc.)
vec3 lightPos = vec3(0.0f, 500.0f, 0.0f);
vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
vec3 lightDir = vec3(0.0f, -1.0f, 0.0f);
int lightType = 1; // Default to Point
float lightRadius = 100.0f;
float lightInnerCutoff = 0.9f;
float lightOuterCutoff = 0.8f;
float lightIntensity = 500.0f;
bool useIBL = false;
float iblIntensity = 1.0f;
bool g_wireframeMode = false;

// ============================================================================
// ENGINE CORE IMPLEMENTATIONS
// ============================================================================
#include "core/gl/texture.cpp"
#include "core/gl/shaders.cpp"
#include "effects/shadow/shadow_map.cpp"
#include "utils/primitives.cpp"
#include "utils/pbr.cpp"
#include "core/gl/camera.cpp"
#include "core/gl/modelloading.cpp"
#include "effects/noise/perlin.c"
#include "effects/terrain/terrain.cpp"
#include "transform.cpp"
#include "editor/model_controller.h"
#include "utils/editor_utils.h"
#include "utils/camera_utils/camera_base.h"
#include "utils/camera_utils/mouseboard.cpp"
#include "utils/camera_utils/custom_camera.cpp"
#include "utils/camera_utils/camera_manager.cpp"
#include "utils/attrdesc.cpp"
#include "utils/entity_defs.cpp"
#include "utils/boundingbox.cpp"
#include "utils/culling.cpp"
#include "utils/BVH.cpp"
#include "utils/scenegraph.cpp"
#include "utils/scenegraph_readwrite.cpp"
#include "utils/shadermanager.cpp"
#include "effects/instance/instance.cpp"
#include "utils/terrain_node.cpp"
#include "utils/skybox.cpp"
#include "utils/skybox_node.cpp"
#include "utils/catmulromspline.cpp"
#include "utils/dynamic_cubemap.cpp"
#include "effects/clouds/volumetricCloudOnCompute.cpp"
#include "effects/clouds/nvdf_generator.cpp"
#include "effects/vclouds/vcloud_texture.cpp"
#include "effects/vclouds/vcloud_noise.cpp"
#include "effects/vclouds/nvdf_compressor.cpp"
#include "effects/vclouds/nvdf_editor_v2.cpp"
#include "effects/atmospheric_Scattering/sky_atmosphere_node.cpp"
