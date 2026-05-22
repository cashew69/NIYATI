#pragma once
#include "engine/effects/effects_config.h"
#include "engine/utils/scenegraph.h"

struct EffectsManager {
    EffectsConfig config;
};

// Create effects manager
EffectsManager* effects_Create(int width, int height);

// Destroy effects manager
void effects_Destroy(EffectsManager* manager);

// Resize when viewport changes
void effects_Resize(EffectsManager* manager, int newWidth, int newHeight);

// Main orchestrator: Renders the scene
void effects_RenderAll(EffectsManager* manager, SceneNode* root, vmath::mat4 view, vmath::mat4 proj);

// Update configuration from UI
void effects_UpdateConfig(EffectsManager* manager, const EffectsConfig* config);

// Get current configuration
EffectsConfig* effects_GetConfig(EffectsManager* manager);

// Scene persistence
void effects_SaveToJSON(EffectsManager* manager, const char* filepath);
void effects_LoadFromJSON(EffectsManager* manager, const char* filepath);
