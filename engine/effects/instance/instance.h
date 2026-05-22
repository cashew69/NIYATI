#pragma once
#include "engine/core/gl/structs.h"

// Initialize instance data with default parameters
void instance_Init(InstanceData* inst);

// Generate instance transforms based on pattern
// Populates inst->instanceMatrices and inst->instanceCount
void instance_GenerateInstances(InstanceData* inst);

// Upload transforms to GPU VBO; call before each draw if data changes
void instance_UploadToGPU(InstanceData* inst);

// Draw all instances of a specific mesh using glDrawElementsInstanced
void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj);
void instance_DrawDepthOnly(SceneNode* node, Mesh* mesh, mat4 vp, ShaderProgram* shader);

// Free GPU resources
void instance_Cleanup(InstanceData* inst);

// Update bounding box
void instance_UpdateAABB(InstanceData* inst);
