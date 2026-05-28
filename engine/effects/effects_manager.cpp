#include "effects_manager.h"
#include <cstdlib>
#include <cstdio>

extern FILE* gpFile;
extern void RenderSceneModels(vmath::mat4 view, vmath::mat4 proj);

EffectsManager* effects_Create(int width, int height) {
    EffectsManager* manager = (EffectsManager*)malloc(sizeof(EffectsManager));
    if (!manager) return nullptr;

    manager->config = g_DefaultEffectsConfig;

    fprintf(gpFile, "[Effects] Created effects manager: %dx%d\n", width, height);
    return manager;
}

void effects_Destroy(EffectsManager* manager) {
    if (!manager) return;

    fprintf(gpFile, "[Effects] Destroyed effects manager\n");
    free(manager);
}

void effects_Resize(EffectsManager* manager, int newWidth, int newHeight) {
    // No-op for now
}

void effects_RenderAll(EffectsManager* manager, SceneNode* root, vmath::mat4 view, vmath::mat4 proj) {
    if (!manager) return;

    // Ensure lighting globals are updated from the first light in the scene tree
    extern void sg_SyncFirstLight(SceneNode* root);
    sg_SyncFirstLight(root);

    // Direct scene rendering without post-processing (SDF rendering is handled inside)
    RenderSceneModels(view, proj);
}

void effects_UpdateConfig(EffectsManager* manager, const EffectsConfig* config) {
    if (!manager || !config) return;

    manager->config = *config;
}

EffectsConfig* effects_GetConfig(EffectsManager* manager) {
    return manager ? &manager->config : nullptr;
}

void effects_SaveToJSON(EffectsManager* manager, const char* filepath) {
    if (!manager || !filepath) return;
    fprintf(gpFile, "[Effects] SaveToJSON: No effects to save\n");
}

void effects_LoadFromJSON(EffectsManager* manager, const char* filepath) {
    if (!manager || !filepath) return;
    fprintf(gpFile, "[Effects] LoadFromJSON: No effects to load\n");
}
