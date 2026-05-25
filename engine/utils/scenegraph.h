#pragma once

#include "engine/core/gl/structs.h"

// Scenegraph Helper Functions
SceneNode* sg_CreateNode(NodeType type, const char* name);
void sg_AddChild(SceneNode* parent, SceneNode* child);
void sg_RemoveChild(SceneNode* parent, SceneNode* child);
void sg_FreeNode(SceneNode* node);
void sg_MarkSceneDirty(); // call after any transform change on animated nodes
void sg_SetAllVisible(SceneNode* node);
void sg_UpdateWorldMatrix(SceneNode* node, mat4 parentWorldMatrix);
void sg_InitNode(SceneNode* node);
SceneNode* sg_AddCameraNode(const char* name); // creates ENTITY_CAMERA node with default values
void sg_DrawNode(SceneNode* node, mat4 view, mat4 proj, int* nodesDrawn = nullptr);

void RenderSceneModels(mat4 view, mat4 proj);
void sg_DrawDepthNodes(SceneNode* node, mat4 vp, ShaderProgram* shader, ShaderProgram* instancedShader = nullptr);
// Syncs lightPos/lightDir/lightType globals from the first light in the scene tree.
// Must be called before shadow rendering since RenderSceneModels hasn't run yet.
void sg_SyncFirstLight(SceneNode* root);
void RenderLightIcons(mat4 view, mat4 proj);  // editor only: billboard icon at each light
bool sg_SaveScene(SceneNode* root, const char* filename);
SceneNode* sg_LoadScene(const char* filename);

// ---- Scene query -----------------------------------------------------------
// Find by stable integer ID (persisted across save/load) or by name.
// Combine with the data accessors below for type-safe access:
//   sg_Light(sg_FindByName("Sun"))   → PointLightData* or null if wrong type
//   sg_Mesh(sg_FindById(3))          → Mesh* or null if wrong type
SceneNode* sg_FindById(int id);
SceneNode* sg_FindByName(const char* name);

// Null-safe typed data accessors — return nullptr if node is null or the wrong type.
// Pattern: sg_Light(sg_FindByName("Sun"))->color = …
LightData*       sg_Light(SceneNode* node);
Camera*      sg_Camera(SceneNode* node);
Mesh*            sg_Mesh(SceneNode* node);
Material*        sg_Material(SceneNode* node);
InstanceData*    sg_Instance(SceneNode* node);
TerrainNodeData* sg_Terrain(SceneNode* node);
CatmullRomNodeData* sg_CatmullRom(SceneNode* node);
OceanNodeData*   sg_Ocean(SceneNode* node);

// ---- Camera helpers for project code ---------------------------------------
// Returns the Camera for a camera node — edit position/target/fov directly.
// Use this to position cameras before or during rendering.
//   Camera* cam = sg_GetCamera(myNode);
//   cam->position = vec3(0, 10, 50);
//   cam->target   = vec3(0, 0, 0);
Camera*     sg_GetCamera(SceneNode* node);

// Activates a camera node for rendering.
void            sg_SetActiveCamera(SceneNode* node);

// ---- Render Debug / Visualization ------------------------------------------
struct RenderDebugInfo {
    char  name[64];
    float dist;
    int   type;
};
extern RenderDebugInfo g_LastFrameRenderOrder[256];
extern int             g_LastFrameRenderCount;
