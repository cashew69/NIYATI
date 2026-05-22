#include "dynamic_cubemap.h"
#include "engine/utils/primitives.h"
#include "engine/core/gl/camera.h"
#include "engine/core/logger.h"
#include "engine/utils/scenegraph.h"
#include "engine/utils/camera_utils/camera_base.h"

// Externs from skybox.cpp and pbr.cpp
extern unsigned int envCubemap;
extern void generateIBLMaps(unsigned int envCubemap);
extern SceneNode* g_SceneRoot;
extern SceneNode* g_ActiveCameraNode;

static GLuint s_DynamicFBO = 0;
static GLuint s_DynamicRBO = 0;
static SceneNode* s_MockCamNode = nullptr;

bool g_IsBakingCubemap = false;

void sg_UpdateDynamicCubemap(SceneNode* root, vec3 center, int resolution) {
    if (!root) root = g_SceneRoot;
    if (!root) return;

    g_IsBakingCubemap = true;

    // 1. Ensure resources are initialized
    if (s_DynamicFBO == 0) {
        glGenFramebuffers(1, &s_DynamicFBO);
        glGenRenderbuffers(1, &s_DynamicRBO);
        
        // Create a persistent mock camera node
        s_MockCamNode = sg_CreateNode(ENTITY_CAMERA, "Dynamic Cubemap Cam");
        s_MockCamNode->data.camera.fov = 90.0f;
        s_MockCamNode->data.camera.near = 0.1f;
        s_MockCamNode->data.camera.far = 10000.0f;
    }

    // Update mock camera position
    s_MockCamNode->position = center;
    s_MockCamNode->data.camera.position = center;

    // 2. Ensure envCubemap is initialized for dynamic use (RGBA16F for high dynamic range)
    if (envCubemap == 0) {
        glGenTextures(1, &envCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, resolution, resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }

    // 3. Setup capture matrices
    mat4 captureProjection = perspective(90.0f, 1.0f, 0.1f, 10000.0f);
    mat4 captureViews[6];
    getCubemapCaptureMatrices(&captureProjection, captureViews); 
    // Wait, getCubemapCaptureMatrices expects 0,0,0 as origin. We need to apply 'center'.
    // The primitives.cpp version:
    // viewsOut[0] = lookat(vec3(0,0,0), vec3( 1, 0, 0), vec3(0,-1, 0));
    // So we just need to add the translation.
    
    mat4 translateMat = translate(-center);
    for(int i=0; i<6; ++i) {
        captureViews[i] = captureViews[i] * translateMat;
    }

    // 4. Render to each face
    glBindFramebuffer(GL_FRAMEBUFFER, s_DynamicFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, s_DynamicRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_DynamicRBO);
    
    glViewport(0, 0, resolution, resolution);

    SceneNode* prevCam = g_ActiveCameraNode;
    CameraMode prevMode = currentCameraMode;

    g_ActiveCameraNode = s_MockCamNode;
    currentCameraMode = CAM_MODE_CUSTOM;
    
    // Ensure all objects are rendered into the cubemap (ignore frustum culling)
    sg_SetAllVisible(root);

    for (int i = 0; i < 6; ++i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        sg_DrawNode(root, captureViews[i], captureProjection, nullptr);
    }

    g_ActiveCameraNode = prevCam;
    currentCameraMode = prevMode;

    g_IsBakingCubemap = false;

    // 5. Finalize
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // 6. Regenerate PBR IBL maps
    generateIBLMaps(envCubemap);

    LOG_I("Dynamic Cubemap updated at [%.1f, %.1f, %.1f]", center[0], center[1], center[2]);
}
