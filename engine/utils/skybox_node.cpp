#include "engine/engine.h"
#include "engine/utils/scenegraph.h"
#include <stdio.h>
#include <string.h>

// External linkage to skybox.cpp functions
extern void initSkybox(const char* hdrPath);
extern void freeSkybox();
extern void renderSkybox(mat4 viewMatrix, mat4 projectionMatrix);
extern void reloadSkyboxPreset(int presetIndex);
extern int currentSkyboxPreset;

extern SceneNode* g_SceneRoot;

void sg_InitSkyboxNode(SceneNode* node) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    SkyboxNodeData* data = &node->data.skybox;

    if (data->hdrPath[0] != '\0') {
        initSkybox(data->hdrPath);
    } else {
        reloadSkyboxPreset(data->currentPreset);
    }
}

void sg_FreeSkyboxNode(SceneNode* node) {
    if (!node || node->type != ENTITY_SKYBOX) return;

    // Delete the skybox cubemap and its IBL maps.
    freeSkybox();

    // If an atmosphere node exists, kick it to re-bake IBL now that skybox is gone.
    auto resetAtmo = [](auto& self, SceneNode* n) -> void {
        if (!n) return;
        if (n->type == ENTITY_SKY_ATMOSPHERE)
            n->data.skyAtmosphere.prevIBLSunDir = vec3(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
    };
    resetAtmo(resetAtmo, g_SceneRoot);
}

void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    renderSkybox(view, proj);
}
