#include "engine/engine.h"
#include "engine/utils/scenegraph.h"
#include <stdio.h>
#include <string.h>

// External linkage to skybox.cpp functions
extern void initSkybox(const char* hdrPath);
extern void renderSkybox(mat4 viewMatrix, mat4 projectionMatrix);
extern void reloadSkyboxPreset(int presetIndex);
extern int currentSkyboxPreset;

void sg_InitSkyboxNode(SceneNode* node) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    SkyboxNodeData* data = &node->data.skybox;

    if (data->hdrPath[0] != '\0') {
        initSkybox(data->hdrPath);
    } else {
        reloadSkyboxPreset(data->currentPreset);
    }
}

void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    
    // In the future, we might want to sync node data to globals if we support multiple skyboxes
    // For now, we assume the one being rendered is the active one.
    
    renderSkybox(view, proj);
}
