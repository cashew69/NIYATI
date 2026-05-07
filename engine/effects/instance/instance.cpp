#include "instance.h"
#include "engine/effects/noise/perlin.h"
#include "engine/utils/boundingbox.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

void instance_Init(InstanceData* inst) {
    if (!inst) return;
    inst->modelPath[0] = '\0';
    inst->instanceVBO = 0;
    inst->instanceCount = 0;
    inst->pattern = INSTANCE_PATTERN_GRID;
    inst->gridCountX = 5;
    inst->gridCountZ = 5;
    inst->spacingX = 2.0f;
    inst->spacingZ = 2.0f;
    inst->noiseScale = 0.1f;
    inst->noiseThreshold = 0.5f;
    inst->areaWidth = 20.0f;
    inst->areaDepth = 20.0f;
    inst->minScale = 0.8f;
    inst->maxScale = 1.2f;
    inst->randomYRotation = 1.0f;
    inst->instanceMatrices = NULL;
    inst->matricesCapacity = 0;
    inst->instanceMeshes = NULL;
    inst->instanceMeshCount = 0;
    
    inst->visibleIndices = NULL;
    inst->visibleCount = 0;
    inst->visibleCapacity = 0;
    
    inst->instanceLocalBounds = NULL;
    
    // Will be computed properly in instance_UploadToGPU or instance_GenerateInstances
    inst->clusterAABB.min = vec3(0, 0, 0);
    inst->clusterAABB.max = vec3(0, 0, 0);
}

static void instance_GenerateGrid(InstanceData* inst) {
    int totalCount = inst->gridCountX * inst->gridCountZ;

    if (totalCount > inst->matricesCapacity) {
        inst->matricesCapacity = (int)(totalCount * 1.5f + 10);
        inst->instanceMatrices = (mat4*)realloc(inst->instanceMatrices,
                                                  inst->matricesCapacity * sizeof(mat4));
    }

    inst->instanceCount = totalCount;
    int idx = 0;

    for (int z = 0; z < inst->gridCountZ; z++) {
        for (int x = 0; x < inst->gridCountX; x++) {
            float posX = x * inst->spacingX;
            float posZ = z * inst->spacingZ;

            // Random scale
            float scale = inst->minScale + (rand() / (float)RAND_MAX) *
                         (inst->maxScale - inst->minScale);

            // Random Y rotation
            float rotY = inst->randomYRotation > 0.5f ?
                        (rand() / (float)RAND_MAX * 2.0f * 3.14159265f) : 0.0f;

            mat4 scaleM = vmath::scale(scale, scale, scale);
            mat4 rotM = vmath::rotate(rotY, vmath::normalize(vec3(0, 1, 0)));
            mat4 transM = vmath::translate(posX, 0.0f, posZ);

            inst->instanceMatrices[idx] = transM * rotM * scaleM;
            idx++;
        }
    }
}

static void instance_GeneratePerlinNoise(InstanceData* inst) {
    int maxCount = (int)(inst->areaWidth * inst->areaDepth / 0.25f);

    if (maxCount > inst->matricesCapacity) {
        inst->matricesCapacity = (int)(maxCount * 1.5f + 10);
        inst->instanceMatrices = (mat4*)realloc(inst->instanceMatrices,
                                                  inst->matricesCapacity * sizeof(mat4));
    }

    inst->instanceCount = 0;
    float gridStep = 0.5f;

    for (float x = -inst->areaWidth * 0.5f; x < inst->areaWidth * 0.5f; x += gridStep) {
        for (float z = -inst->areaDepth * 0.5f; z < inst->areaDepth * 0.5f; z += gridStep) {
            float noiseVal = perlinNoise(x * inst->noiseScale, z * inst->noiseScale);

            if (noiseVal > inst->noiseThreshold) {
                if (inst->instanceCount >= inst->matricesCapacity) break;

                float scale = inst->minScale + (rand() / (float)RAND_MAX) *
                             (inst->maxScale - inst->minScale);

                float rotY = inst->randomYRotation > 0.5f ?
                            (rand() / (float)RAND_MAX * 2.0f * 3.14159265f) : 0.0f;

                mat4 scaleM = vmath::scale(scale, scale, scale);
                mat4 rotM = vmath::rotate(rotY, vmath::normalize(vec3(0, 1, 0)));
                mat4 transM = vmath::translate(x, 0.0f, z);

                inst->instanceMatrices[inst->instanceCount] = transM * rotM * scaleM;
                inst->instanceCount++;
            }
        }
    }
}

void instance_GenerateInstances(InstanceData* inst) {
    if (!inst) return;

    if (inst->pattern == INSTANCE_PATTERN_GRID) {
        instance_GenerateGrid(inst);
    } else if (inst->pattern == INSTANCE_PATTERN_PERLIN_NOISE) {
        instance_GeneratePerlinNoise(inst);
    }
    instance_UpdateAABB(inst);
}

void instance_UploadToGPU(InstanceData* inst) {
    if (!inst || inst->instanceCount <= 0 || !inst->instanceMatrices) return;

    if (inst->instanceVBO == 0) {
        glGenBuffers(1, &inst->instanceVBO);
    }

    glBindBuffer(GL_COPY_WRITE_BUFFER, inst->instanceVBO);
    glBufferData(GL_COPY_WRITE_BUFFER,
                 inst->instanceCount * sizeof(mat4),
                 inst->instanceMatrices,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void instance_UploadVisibleToGPU(InstanceData* inst) {
    if (!inst || inst->visibleCount <= 0 || !inst->instanceMatrices) return;

    if (inst->instanceVBO == 0) {
        glGenBuffers(1, &inst->instanceVBO);
    }

    glBindBuffer(GL_ARRAY_BUFFER, inst->instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, inst->visibleCount * sizeof(mat4), NULL, GL_DYNAMIC_DRAW);
    mat4* mappedData = (mat4*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (mappedData) {
        for (int i = 0; i < inst->visibleCount; i++) {
            mappedData[i] = inst->instanceMatrices[inst->visibleIndices[i]];
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_INSTANCE || !mesh) return;

    InstanceData* inst = &node->data.instance;
    if (inst->visibleCount <= 0 || mesh->vao == 0 || mesh->indexCount <= 0) return;

    if (inst->instanceVBO == 0) return;

    ShaderProgram* program = instancedProgram;
    if (!program) return;

    glUseProgram(program->id);

    // Set standard uniforms (non-instanced)
    glUniformMatrix4fv(glGetUniformLocation(program->id, "uProjection"), 1, GL_FALSE, (float*)&proj);
    glUniformMatrix4fv(glGetUniformLocation(program->id, "uView"), 1, GL_FALSE, (float*)&view);

    // Set node's world matrix for uModel so instances are local to the node
    glUniformMatrix4fv(glGetUniformLocation(program->id, "uModel"), 1, GL_FALSE, node->world_matrix);

    // Light setup (using centralized globals)
    extern vec3  lightPos;
    extern vec3  lightColor;
    extern float lightIntensity;
    extern vec3  lightDir;
    extern int   lightType;
    extern float lightRadius;
    extern float lightInnerCutoff;
    extern float lightOuterCutoff;

    glUniform3fv(program->loc.uLightPos,      1, (float*)&lightPos);
    glUniform3fv(program->loc.uLightColor,    1, (float*)&lightColor);
    glUniform1f(program->loc.uLightIntensity, lightIntensity);
    glUniform1i(program->loc.uLightType,      lightType);
    glUniform3fv(program->loc.uLightDir,      1, (float*)&lightDir);
    glUniform1f(program->loc.uLightRadius,    lightRadius);
    glUniform1f(program->loc.uInnerCutoff,    lightInnerCutoff);
    glUniform1f(program->loc.uOuterCutoff,    lightOuterCutoff);

    // Ensure PBR uniforms are initialized to safe defaults
    glUniform1i(glGetUniformLocation(program->id, "uHasIBL"), useIBL ? 1 : 0);
    glUniform1f(glGetUniformLocation(program->id, "uIBLIntensity"), iblIntensity);
    glUniform1f(glGetUniformLocation(program->id, "uRoughness"), mesh->material.roughness);
    glUniform1f(glGetUniformLocation(program->id, "uMetalness"), mesh->material.metalness);
    if (useIBL) {
        extern void bindIBL(ShaderProgram* program);
        bindIBL(program);
    }
    glUniform1i(glGetUniformLocation(program->id, "uDebugDisableDiffuseTex"), 0);
    glUniform1i(glGetUniformLocation(program->id, "uDebugDisableNormalTex"), 0);
    glUniform1i(glGetUniformLocation(program->id, "uDebugDisableORMMap"), 0);
    glUniform1i(glGetUniformLocation(program->id, "uDebugDisableAOTex"), 0);
    glUniform1i(glGetUniformLocation(program->id, "uDebugDisableEmissiveTex"), 0);
    
    extern void setDebugUniforms(ShaderProgram* program);
    setDebugUniforms(program);

    vec3 viewPos = vec3(0, 0, 0);
    if (g_EditorCamera) viewPos = g_EditorCamera->position;
    glUniform3fv(glGetUniformLocation(program->id, "uViewPos"), 1, (float*)&viewPos);

    setMaterialUniforms(program, &mesh->material);

    // Bind mesh VAO
    glBindVertexArray(mesh->vao);

    // Bind instance VBO to available attribute slots (4, 5, 6, 7 for mat4 columns)
    glBindBuffer(GL_ARRAY_BUFFER, inst->instanceVBO);

    // mat4 takes 4 attribute slots; use locations 4-7
    for (int i = 0; i < 4; i++) {
        GLint loc = 4 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(mat4),
                             (void*)(i * sizeof(vec4)));
        glVertexAttribDivisor(loc, 1);
    }

    // Draw instanced
    glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh->indexCount,
                           GL_UNSIGNED_INT, 0, inst->visibleCount);

    // Cleanup
    for (int i = 0; i < 4; i++) {
        glDisableVertexAttribArray(4 + i);
        glVertexAttribDivisor(4 + i, 0);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void instance_Cleanup(InstanceData* inst) {
    if (!inst) return;
    if (inst->instanceVBO != 0) {
        glDeleteBuffers(1, &inst->instanceVBO);
        inst->instanceVBO = 0;
    }
    if (inst->instanceMatrices) {
        free(inst->instanceMatrices);
        inst->instanceMatrices = NULL;
    }
    if (inst->instanceMeshes) {
        free(inst->instanceMeshes);
        inst->instanceMeshes = NULL;
    }
    if (inst->visibleIndices) {
        free(inst->visibleIndices);
        inst->visibleIndices = NULL;
    }
    if (inst->instanceLocalBounds) {
        free(inst->instanceLocalBounds);
        inst->instanceLocalBounds = NULL;
    }
    inst->instanceCount = 0;
    inst->instanceMeshCount = 0;
    inst->visibleCount = 0;
    inst->visibleCapacity = 0;
}

void instance_UpdateAABB(InstanceData* inst) {
    if (!inst) return;
    
    inst->clusterAABB = bbox_Empty();
    if (inst->instanceCount <= 0 || inst->instanceMeshCount <= 0 || !inst->instanceMeshes) {
        if (inst->instanceLocalBounds) { free(inst->instanceLocalBounds); inst->instanceLocalBounds = NULL; }
        return;
    }
    
    // Allocate/reallocate cached local bounds
    inst->instanceLocalBounds = (AABB*)realloc(inst->instanceLocalBounds, inst->instanceCount * sizeof(AABB));

    AABB meshAABB = bbox_Empty();
    for (int i = 0; i < inst->instanceMeshCount; i++) {
        bbox_Combine(&meshAABB, &inst->instanceMeshes[i].aabbLocal);
    }
    
    for (int i = 0; i < inst->instanceCount; i++) {
        // Transform mesh local AABB by instance matrix to get node-local AABB
        AABB transformed = bbox_Transform(meshAABB, inst->instanceMatrices[i]);
        inst->instanceLocalBounds[i] = transformed;
        bbox_Combine(&inst->clusterAABB, &transformed);
    }
}
