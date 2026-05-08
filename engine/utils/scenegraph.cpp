#include "scenegraph.h"
#include "engine/utils/boundingbox.h"
#include "engine/utils/BVH.h"
#include "engine/utils/culling.h"
#include "engine/dependancies/vmath.h"
#include "engine/utils/catmulromspline.h"
#include <stdlib.h>
#include <string.h>

extern unsigned int irradianceMap;

static int  g_NextNodeId = 1;
static bool g_BVHDirty  = true;

void sg_MarkSceneDirty() { g_BVHDirty = true; }

SceneNode* sg_CreateNode(NodeType type, const char* name) {
    SceneNode* node = (SceneNode*)calloc(1, sizeof(SceneNode));
    node->type = type;

    if (name) {
        node->name = strdup(name);
    } else {
        node->name = strdup("Unnamed Node");
    }

    node->position = vec3(0.0f, 0.0f, 0.0f);
    node->rotation_euler = vec3(0.0f, 0.0f, 0.0f);
    node->scale = vec3(1.0f, 1.0f, 1.0f);

    node->local_matrix = mat4::identity();
    node->world_matrix = mat4::identity();

    node->parent = nullptr;
    node->children = nullptr;
    node->num_children = 0;
    node->capacity_children = 0;

    node->sourcePath[0] = '\0';
    node->meshIndex = -1;

    node->ID = g_NextNodeId++;

    return node;
}

void sg_AddChild(SceneNode* parent, SceneNode* child) {
    if (!parent || !child) return;

    if (parent->num_children >= parent->capacity_children) {
        parent->capacity_children = parent->capacity_children == 0 ? 4 : parent->capacity_children * 2;
        parent->children = (SceneNode**)realloc(parent->children, parent->capacity_children * sizeof(SceneNode*));
    }

    parent->children[parent->num_children++] = child;
    child->parent = parent;
    g_BVHDirty = true;
}

void sg_RemoveChild(SceneNode* parent, SceneNode* child) {
    if (!parent || !child) return;

    for (int i = 0; i < parent->num_children; ++i) {
        if (parent->children[i] == child) {
            for (int j = i; j < parent->num_children - 1; ++j) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->num_children--;
            child->parent = nullptr;
            g_BVHDirty = true;
            break;
        }
    }
}

void sg_FreeNode(SceneNode* node) {
    if (!node) return;
    g_BVHDirty = true;

    for (int i = 0; i < node->num_children; ++i)
        sg_FreeNode(node->children[i]);

    if (node->type == ENTITY_CAMERA && g_ActiveCameraNode == node) {
        g_ActiveCameraNode = nullptr;
        currentCameraMode  = CAM_MODE_MOUSE_BOARD;
    }

    if (node->type == ENTITY_CATMULLROMSPLINE) {
        sg_FreeCatmullRomNode(node);
    }

    if (node->children)  free(node->children);
    if (node->name)      free((void*)node->name);
    free(node);
}

SceneNode* sg_AddCameraNode(const char* name) {
    SceneNode* n = sg_CreateNode(ENTITY_CAMERA, name);
    n->position             = vec3(10.0f, 10.0f, 10.0f);
    n->data.camera.position = vec3(10.0f, 10.0f, 10.0f);
    n->data.camera.target   = vec3(0.0f,  0.0f,  0.0f);
    n->data.camera.up       = vec3(0.0f,  1.0f,  0.0f);
    n->data.camera.roll     = 0.0f;
    n->data.camera.fov      = 45.0f;
    n->data.camera.near     = 0.1f;
    n->data.camera.far      = 10000.0f;
    return n;
}

// ---- Editor Gizmos ---------------------------------------------------------

static float s_LightBatch[4096]; // flat array: pos.xyz, col.rgb * 2 (start/end)
static int s_LightLineCount = 0;

static void sg_AddLightLine(vec3 s, vec3 e, vec3 c) {
    if (s_LightLineCount >= (4096 / 12)) return;
    float* p = &s_LightBatch[s_LightLineCount * 12];
    p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = c[0]; p[4] = c[1]; p[5] = c[2];
    p[6] = e[0]; p[7] = e[1]; p[8] = e[2]; p[9] = c[0]; p[10] = c[1]; p[11] = c[2];
    s_LightLineCount++;
}

static void wDirManualMult(vec3 lDir, mat4 rot, vec3& wDir) {
    wDir[0] = lDir[0] * rot[0][0] + lDir[1] * rot[1][0] + lDir[2] * rot[2][0];
    wDir[1] = lDir[0] * rot[0][1] + lDir[1] * rot[1][1] + lDir[2] * rot[2][1];
    wDir[2] = lDir[0] * rot[0][2] + lDir[1] * rot[1][2] + lDir[2] * rot[2][2];
}

// Returns the light's direction transformed into world space.
static vec3 sg_LightWorldDir(SceneNode* node) {
    mat4 rot = node->world_matrix;
    rot[3][0] = 0; rot[3][1] = 0; rot[3][2] = 0;
    vec3 wD;
    wDirManualMult(node->data.light.direction, rot, wD);
    return (vmath::length(wD) > 0.001f) ? normalize(wD) : vec3(0, -1, 0);
}

// Draws a circle (segs segments) centred at `center` in the `right`/`up` plane.
static void sg_DrawCircle(vec3 center, vec3 right, vec3 up, float r, vec3 col, int segs = 16) {
    vec3 prev;
    for (int i = 0; i <= segs; i++) {
        float theta = ((float)i / (float)segs) * 2.0f * 3.14159265f;
        vec3 cur = center + (right * cosf(theta) + up * sinf(theta)) * r;
        if (i > 0) sg_AddLightLine(prev, cur, col);
        prev = cur;
    }
}

static void sg_CollectLightGizmos(SceneNode* node) {
    if (!node) return;
    if (node->type == ENTITY_LIGHT) {
        vec3 p = vec3(node->world_matrix[3][0], node->world_matrix[3][1], node->world_matrix[3][2]);
        vec3 c = node->data.light.color;

        if (node->data.light.type == LIGHT_POINT) {
            float r = node->data.light.radius * 0.5f;
            for (int i = 0; i < 2; i++) {
                float y = (i == 0) ? -r : r;
                sg_AddLightLine(p + vec3(-r, y, -r), p + vec3( r, y, -r), c);
                sg_AddLightLine(p + vec3( r, y, -r), p + vec3( r, y,  r), c);
                sg_AddLightLine(p + vec3( r, y,  r), p + vec3(-r, y,  r), c);
                sg_AddLightLine(p + vec3(-r, y,  r), p + vec3(-r, y, -r), c);
            }
            sg_AddLightLine(p + vec3(-r, -r, -r), p + vec3(-r,  r, -r), c);
            sg_AddLightLine(p + vec3( r, -r, -r), p + vec3( r,  r, -r), c);
            sg_AddLightLine(p + vec3( r, -r,  r), p + vec3( r,  r,  r), c);
            sg_AddLightLine(p + vec3(-r, -r,  r), p + vec3(-r,  r,  r), c);
        }
        else if (node->data.light.type == LIGHT_SPOT) {
            vec3 dir    = sg_LightWorldDir(node);
            vec3 up     = (fabsf(dir[1]) < 0.9f) ? vec3(0,1,0) : vec3(1,0,0);
            vec3 right  = normalize(cross(up, dir));
            vec3 realUp = cross(dir, right);

            sg_DrawCircle(p, right, realUp, 0.4f, c);

            float r   = node->data.light.radius;
            vec3  end = p + dir * r;
            auto drawCone = [&](float angleCos, vec3 col) {
                float discR = r * tanf(acosf(angleCos));
                sg_DrawCircle(end, right, realUp, discR, col);
                sg_AddLightLine(p, end + right  * discR, col);
                sg_AddLightLine(p, end - right  * discR, col);
                sg_AddLightLine(p, end + realUp * discR, col);
                sg_AddLightLine(p, end - realUp * discR, col);
            };
            drawCone(node->data.light.innerCutoff, c);
            drawCone(node->data.light.outerCutoff, c * 0.5f);
        }
        else if (node->data.light.type == LIGHT_DIRECTIONAL) {
            float s = 0.5f;
            sg_AddLightLine(p + vec3(-s, 0, 0), p + vec3(s, 0, 0), c);
            sg_AddLightLine(p + vec3(0, -s, 0), p + vec3(0, s, 0), c);
            sg_AddLightLine(p + vec3(0, 0, -s), p + vec3(0, 0, s), c);
            sg_AddLightLine(p, p + sg_LightWorldDir(node) * 10.0f, vec3(1,1,1));
        }
    }
    for (int i = 0; i < node->num_children; i++)
        sg_CollectLightGizmos(node->children[i]);
}

void RenderLightHelpers(mat4 view, mat4 proj) {
    if (!g_SceneRoot) return;
    s_LightLineCount = 0;
    sg_CollectLightGizmos(g_SceneRoot);
    if (s_LightLineCount > 0) {
        extern void drawDebugLinesBatch(float* verts, int lineCount, mat4 view, mat4 proj);
        drawDebugLinesBatch(s_LightBatch, s_LightLineCount, view, proj);
    }
}

// ---- Light icon billboards -------------------------------------------------

static const int MAX_ICON_LIGHTS = 64;
// Per icon: 6 verts × 5 floats (pos.xyz + uv.xy)
static float s_IconVerts[MAX_ICON_LIGHTS * 6 * 5];
static vec3  s_IconColors[MAX_ICON_LIGHTS];
static int   s_IconCount = 0;
static GLuint s_IconVAO  = 0;
static GLuint s_IconVBO  = 0;
static GLuint s_IconTex  = 0;

static void sg_CollectIcons(SceneNode* node, vec3 camRight, vec3 camUp, float half) {
    if (!node) return;
    if (node->type == ENTITY_LIGHT && s_IconCount < MAX_ICON_LIGHTS) {
        vec3 p  = vec3(node->world_matrix[3][0],
                       node->world_matrix[3][1],
                       node->world_matrix[3][2]);
        vec3 bl = p + (-camRight - camUp) * half;
        vec3 br = p + ( camRight - camUp) * half;
        vec3 tl = p + (-camRight + camUp) * half;
        vec3 tr = p + ( camRight + camUp) * half;

        float* v = &s_IconVerts[s_IconCount * 6 * 5];
        v[ 0]=bl[0]; v[ 1]=bl[1]; v[ 2]=bl[2]; v[ 3]=0; v[ 4]=0;
        v[ 5]=br[0]; v[ 6]=br[1]; v[ 7]=br[2]; v[ 8]=1; v[ 9]=0;
        v[10]=tl[0]; v[11]=tl[1]; v[12]=tl[2]; v[13]=0; v[14]=1;
        v[15]=br[0]; v[16]=br[1]; v[17]=br[2]; v[18]=1; v[19]=0;
        v[20]=tr[0]; v[21]=tr[1]; v[22]=tr[2]; v[23]=1; v[24]=1;
        v[25]=tl[0]; v[26]=tl[1]; v[27]=tl[2]; v[28]=0; v[29]=1;

        s_IconColors[s_IconCount] = node->data.light.color;
        s_IconCount++;
    }
    for (int i = 0; i < node->num_children; i++)
        sg_CollectIcons(node->children[i], camRight, camUp, half);
}

void RenderLightIcons(mat4 view, mat4 proj) {
    if (!g_SceneRoot) return;
    extern ShaderProgram* iconShaderProgram;
    if (!iconShaderProgram) return;

    if (s_IconTex == 0) {
        extern bool loadPNGTexture(GLuint* texture, char* file, int repeat);
        char path[] = "engine/utils/light_icon.png";
        loadPNGTexture(&s_IconTex, path, 0);
        if (s_IconTex == 0) return;
    }

    if (s_IconVAO == 0) {
        glGenVertexArrays(1, &s_IconVAO);
        glGenBuffers(1, &s_IconVBO);
        glBindVertexArray(s_IconVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_IconVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s_IconVerts), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    // Camera right/up extracted from view matrix rows
    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);

    s_IconCount = 0;
    sg_CollectIcons(g_SceneRoot, camRight, camUp, 1.0f);
    if (s_IconCount == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, s_IconVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, s_IconCount * 6 * 5 * sizeof(float), s_IconVerts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(iconShaderProgram->id);
    GLint locView  = glGetUniformLocation(iconShaderProgram->id, "uView");
    GLint locProj  = glGetUniformLocation(iconShaderProgram->id, "uProjection");
    GLint locTex   = glGetUniformLocation(iconShaderProgram->id, "uIconTex");
    GLint locTint  = glGetUniformLocation(iconShaderProgram->id, "uTintColor");
    glUniformMatrix4fv(locView, 1, GL_FALSE, view);
    glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
    glUniform1i(locTex, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_IconTex);
    glBindVertexArray(s_IconVAO);

    for (int i = 0; i < s_IconCount; i++) {
        glUniform3fv(locTint, 1, s_IconColors[i]);
        glDrawArrays(GL_TRIANGLES, i * 6, 6);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void sg_UpdateWorldMatrix(SceneNode* node, mat4 parentWorldMatrix) {
    if (!node) return;

    mat4 smScale = vmath::scale(node->scale[0], node->scale[1], node->scale[2]);

    mat4 smRot = mat4::identity();
    smRot = smRot * vmath::rotate(node->rotation_euler[2], vec3(0.0f, 0.0f, 1.0f));
    smRot = smRot * vmath::rotate(node->rotation_euler[1], vec3(0.0f, 1.0f, 0.0f));
    smRot = smRot * vmath::rotate(node->rotation_euler[0], vec3(1.0f, 0.0f, 0.0f));

    mat4 smTrans = vmath::translate(node->position[0], node->position[1], node->position[2]);

    node->local_matrix = smTrans * smRot * smScale;
    node->world_matrix = parentWorldMatrix * node->local_matrix;

    if (node->terrainYOffset && node->selectedTerrainID != 0) {
        extern SceneNode* sg_FindById(int id);
        SceneNode* terrainNode = sg_FindById(node->selectedTerrainID);
        if (terrainNode && terrainNode->type == ENTITY_TERRAIN) {
            TerrainNodeData* tData = &terrainNode->data.terrain;
            if (tData->cpuHeightMap && tData->cpuHeightMapWidth > 0 && tData->cpuHeightMapHeight > 0) {

                auto getSnappedY = [&](float wX, float wZ) -> float {
                    float tPosX = terrainNode->world_matrix[3][0];
                    float tPosY = terrainNode->world_matrix[3][1];
                    float tPosZ = terrainNode->world_matrix[3][2];

                    float tScaleX = vmath::length(vmath::vec3(terrainNode->world_matrix[0][0], terrainNode->world_matrix[0][1], terrainNode->world_matrix[0][2]));
                    float tScaleY = vmath::length(vmath::vec3(terrainNode->world_matrix[1][0], terrainNode->world_matrix[1][1], terrainNode->world_matrix[1][2]));
                    float tScaleZ = vmath::length(vmath::vec3(terrainNode->world_matrix[2][0], terrainNode->world_matrix[2][1], terrainNode->world_matrix[2][2]));

                    float lX = (wX - tPosX) / (tScaleX > 0.0001f ? tScaleX : 1.0f);
                    float lZ = (wZ - tPosZ) / (tScaleZ > 0.0001f ? tScaleZ : 1.0f);

                    float spacing = tData->worldScale;
                    float hW = (tData->meshWidth * spacing) * 0.5f;
                    float hD = (tData->meshDepth * spacing) * 0.5f;

                    float uu = (lX + hW) / (tData->meshWidth * spacing);
                    float vv = (lZ + hD) / (tData->meshDepth * spacing);

                    if (uu >= 0.0f && uu <= 1.0f && vv >= 0.0f && vv <= 1.0f) {
                        int tx = (int)(uu * (tData->cpuHeightMapWidth - 1));
                        int ty = (int)(vv * (tData->cpuHeightMapHeight - 1));
                        float h = tData->cpuHeightMap[ty * tData->cpuHeightMapWidth + tx];
                        return tPosY + (h * tData->displacementScale) * tScaleY;
                    }
                    return tPosY;
                };

                if (node->type == ENTITY_INSTANCE) {
                    InstanceData* inst = &node->data.instance;
                    float nodeWorldY = node->world_matrix[3][1];
                    float nodeScaleY = vmath::length(vec3(node->world_matrix[1][0], node->world_matrix[1][1], node->world_matrix[1][2]));
                    if (nodeScaleY < 0.0001f) nodeScaleY = 1.0f;

                    for (int i = 0; i < inst->instanceCount; i++) {
                        float lx = inst->instanceMatrices[i][3][0];
                        float lz = inst->instanceMatrices[i][3][2];

                        // Current world position of instance (assuming current matrix is okay for X/Z)
                        vec4 worldPos = vec4(lx, 0.0f, lz, 1.0f) * node->world_matrix;
                        float terrainY = getSnappedY(worldPos[0], worldPos[2]);

                        // We want: worldY = terrainY + node->position[1]
                        // nodeWorldY + instLocalY * nodeScaleY = terrainY + node->position[1]
                        inst->instanceMatrices[i][3][1] = (terrainY + node->position[1] - nodeWorldY) / nodeScaleY;
                    }
                } else {
                    float terrainY = getSnappedY(node->world_matrix[3][0], node->world_matrix[3][2]);
                    node->world_matrix[3][1] = terrainY + node->position[1];
                }
            }
        }
    }

    for (int i = 0; i < node->num_children; ++i) {
        sg_UpdateWorldMatrix(node->children[i], node->world_matrix);
    }
}

// ============================================================================
// RENDERING
// ============================================================================

extern void setMaterialUniforms(ShaderProgram* program, Material* material);
extern GLint getUniformLocation(ShaderProgram* program, const char* name);

// Draw exactly one model node (no recursion).  Shared between the recursive
// walk and the BVH-culled draw path.
static void sg_DrawModelNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_MODEL) return;

    Mesh* mesh = &node->data.mesh;
    ShaderProgram* program = pbrShaderProgram;
    if (!program) program = lambertShaderProgram;
    if (!program || mesh->indexCount <= 0 || mesh->vao == 0) return;

    glUseProgram(program->id);

    glUniformMatrix4fv(program->loc.uProjection, 1, GL_FALSE, proj);
    glUniformMatrix4fv(program->loc.uView,       1, GL_FALSE, view);
    glUniformMatrix4fv(program->loc.uModel,      1, GL_FALSE, node->world_matrix);

    vec3 viewPos = GetActiveCameraPosition();
    glUniform3fv(program->loc.uViewPos,    1, (float*)&viewPos);

    // lightPos/lightColor are synced once per frame by RenderSceneModels
    glUniform3fv(program->loc.uLightPos,   1, (float*)&lightPos);
    glUniform3fv(program->loc.uLightColor, 1, (float*)&lightColor);
    glUniform1f(program->loc.uLightIntensity, lightIntensity);
    glUniform1i(program->loc.uLightType,    lightType);

    // Transform local light direction to world space
    vec3 worldLightDir = lightDir;
    if (lightType == 0 || lightType == 2) { // Directional or Spot
        // We need the first light node again to get its rotation,
        // or we could have synced worldLightDir in RenderSceneModels.
        // Let's assume lightDir is already world-space for now,
        // but ideally we should have calculated it in RenderSceneModels.
    }
    glUniform3fv(program->loc.uLightDir,    1, (float*)&lightDir);
    glUniform1f(program->loc.uLightRadius,  lightRadius);
    glUniform1f(program->loc.uInnerCutoff,  lightInnerCutoff);
    glUniform1f(program->loc.uOuterCutoff,  lightOuterCutoff);

    glUniform1i(program->loc.uHasIBL,       useIBL ? 1 : 0);
    glUniform1f(program->loc.uIBLIntensity, iblIntensity);
    if (useIBL) {
        extern void bindIBL(ShaderProgram* program);
        bindIBL(program);
    }

    extern void setDebugUniforms(ShaderProgram* program);
    setDebugUniforms(program);

    setMaterialUniforms(program, &mesh->material);

    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

static void sg_DrawTerrainNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    extern void sg_RenderTerrainNode(SceneNode* node, mat4 view, mat4 proj);
    sg_RenderTerrainNode(node, view, proj);
}

static void sg_DrawSkyboxNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_SKYBOX) return;
    extern void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj);
    sg_RenderSkyboxNode(node, view, proj);
}

void sg_DrawNode(SceneNode* node, mat4 view, mat4 proj, int* nodesDrawn) {
    if (!node) return;
    if (node->type == ENTITY_MODEL) {
        sg_DrawModelNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    } else if (node->type == ENTITY_INSTANCE) {
        extern void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj);
        InstanceData* inst = &node->data.instance;
        if (inst->modelPath[0] != '\0' && inst->instanceMeshes != nullptr) {
            extern void instance_UploadToGPU(InstanceData* inst);
            instance_UploadToGPU(inst);

            for (int i = 0; i < inst->instanceMeshCount; i++) {
                instance_DrawInstances(node, &inst->instanceMeshes[i], view, proj);
            }
            if (nodesDrawn) (*nodesDrawn) += inst->instanceCount;
        }
    } else if (node->type == ENTITY_TERRAIN) {
        sg_DrawTerrainNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    } else if (node->type == ENTITY_SKYBOX) {
        sg_DrawSkyboxNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    } else if (node->type == ENTITY_CATMULLROMSPLINE) {
        sg_RenderCatmullRomNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    }
    for (int i = 0; i < node->num_children; i++) {
        sg_DrawNode(node->children[i], view, proj, nodesDrawn);
    }
}

// Per-frame transient buffer for BVH visibility queries.
static int* s_visBuf      = nullptr;
static int  s_visBufCap   = 0;
static void sg_EnsureVisBuf(int capacity) {
    if (capacity <= s_visBufCap) return;
    int newCap = s_visBufCap == 0 ? 64 : s_visBufCap;
    while (newCap < capacity) newCap *= 2;
    s_visBuf = (int*)realloc(s_visBuf, newCap * sizeof(int));
    s_visBufCap = newCap;
}

static void sg_ClearVisibleInstances(SceneNode* node) {
    if (!node) return;
    if (node->type == ENTITY_INSTANCE) {
        node->data.instance.visibleCount = 0;
    }
    for (int i = 0; i < node->num_children; i++) {
        sg_ClearVisibleInstances(node->children[i]);
    }
}

static void sg_DrawVisibleInstances(SceneNode* node, mat4 view, mat4 proj) {
    if (!node) return;
    if (node->type == ENTITY_INSTANCE && node->data.instance.visibleCount > 0) {
        InstanceData* inst = &node->data.instance;
        if (inst->modelPath[0] != '\0' && inst->instanceMeshes != nullptr) {
            extern void instance_UploadVisibleToGPU(InstanceData* inst);
            instance_UploadVisibleToGPU(inst);

            extern void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj);
            for (int m = 0; m < inst->instanceMeshCount; m++) {
                instance_DrawInstances(node, &inst->instanceMeshes[m], view, proj);
            }
            g_VisibleModelCount += inst->visibleCount;
        }
    }
    for (int i = 0; i < node->num_children; i++) {
        sg_DrawVisibleInstances(node->children[i], view, proj);
    }
}

void RenderSceneModels(mat4 view, mat4 proj) {
    // Sync globals for legacy systems (like terrain)
    viewMatrix = view;
    perspectiveProjectionMatrix = proj;

    if (!g_SceneRoot) {
        glUseProgram(0);
        return;
    }

    // 1. Update transforms first so we use fresh world matrices for lighting and culling
    sg_UpdateWorldMatrix(g_SceneRoot, mat4::identity());

    // 2. Sync lights using the updated world matrices
    extern vec3 lightPos;
    extern vec3 lightColor;
    extern vec3 lightDir;
    extern int lightType;
    extern float lightRadius;
    extern float lightInnerCutoff;
    extern float lightOuterCutoff;
    extern float lightIntensity;

    // Find first active light in scene
    auto findLight = [](auto& self, SceneNode* n) -> SceneNode* {
        if (!n) return nullptr;
        if (n->type == ENTITY_LIGHT) return n;
        for (int i = 0; i < n->num_children; i++) {
            SceneNode* found = self(self, n->children[i]);
            if (found) return found;
        }
        return nullptr;
    };

    SceneNode* lightNode = findLight(findLight, g_SceneRoot);
    if (lightNode) {
        LightData* l = &lightNode->data.light;
        lightPos = vec3(lightNode->world_matrix[3][0], lightNode->world_matrix[3][1], lightNode->world_matrix[3][2]);
        lightColor = vec3(l->color[0], l->color[1], l->color[2]);
        lightIntensity = l->intensity;
        lightType = l->type;
        lightRadius = l->radius;

        // Transform local direction to world space
        mat4 rot = lightNode->world_matrix;
        rot[3][0] = 0; rot[3][1] = 0; rot[3][2] = 0;
        vec3 wDir;
        wDirManualMult(l->direction, rot, wDir);
        lightDir = (vmath::length(wDir) > 0.001f) ? normalize(wDir) : l->direction;

        lightInnerCutoff = l->innerCutoff;
        lightOuterCutoff = l->outerCutoff;
    }

    auto findSkybox = [](auto& self, SceneNode* n) -> SceneNode* {
        if (!n) return nullptr;
        if (n->type == ENTITY_SKYBOX) return n;
        for (int i = 0; i < n->num_children; i++) {
            SceneNode* found = self(self, n->children[i]);
            if (found) return found;
        }
        return nullptr;
    };
    SceneNode* skyboxNode = findSkybox(findSkybox, g_SceneRoot);
    useIBL = (skyboxNode != nullptr && irradianceMap != 0);

    // Ensure correct GL state (ImGui may have changed these)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    g_VisibleModelCount = 0;
    g_VisibleLightCount = 0;
    g_CulledModelCount  = 0;

    sg_ClearVisibleInstances(g_SceneRoot);

    // Rebuild BVH only when the scene structure or transforms have changed.
    // Call sg_MarkSceneDirty() after any transform change on animated nodes.
    if (g_BVHEnabled && g_BVHDirty) {
        bvh_BuildFromScene(&g_SceneBVH, g_SceneRoot);
        g_BVHDirty = false;
    }

    if (g_FrustumCullingEnabled && g_BVHEnabled && g_SceneBVH.itemCount > 0) {
        mat4 vp = proj * view;
        cull_ExtractFrustum(&g_RenderFrustum, vp);

        sg_EnsureVisBuf(g_SceneBVH.itemCount);
        int visibleCount = 0;
        bvh_QueryFrustum(&g_SceneBVH, &g_RenderFrustum, s_visBuf, s_visBufCap, &visibleCount);

        int drawnBVHModels = 0;
        for (int i = 0; i < visibleCount; i++) {
            BVHItem* it = &g_SceneBVH.items[s_visBuf[i]];
            if (it->type == BVH_ITEM_MODEL && it->userData) {
                sg_DrawModelNode((SceneNode*)it->userData, view, proj);
                g_VisibleModelCount++;
                drawnBVHModels++;
            } else if (it->type == BVH_ITEM_LIGHT) {
                g_VisibleLightCount++;
            } else if (it->type == BVH_ITEM_TERRAIN && it->userData) {
                extern void sg_RenderTerrainNode(SceneNode* node, mat4 view, mat4 proj);
                sg_RenderTerrainNode((SceneNode*)it->userData, view, proj);
                g_VisibleModelCount++;
            } else if (it->type == BVH_ITEM_INSTANCE && it->userData) {
                SceneNode* node = (SceneNode*)it->userData;
                InstanceData* inst = &node->data.instance;

                // Fine-grained sub-culling for the instance group.
                // We transform the frustum into node-local space to avoid per-instance world transforms.
                cullfrustum localFrustum;
                cull_TransformFrustum(&localFrustum, &g_RenderFrustum, node->world_matrix);

                for (int j = 0; j < inst->instanceCount; j++) {
                    if (cull_TestAABB(&localFrustum, inst->instanceLocalBounds[j])) {
                        if (inst->visibleCount >= inst->visibleCapacity) {
                            inst->visibleCapacity = inst->visibleCapacity == 0 ? 16 : inst->visibleCapacity * 2;
                            inst->visibleIndices = (int*)realloc(inst->visibleIndices, inst->visibleCapacity * sizeof(int));
                        }
                        inst->visibleIndices[inst->visibleCount++] = j;
                    }
                }
            }
        }

        sg_DrawVisibleInstances(g_SceneRoot, view, proj);

        // Skybox is not in the BVH — always render it directly
        if (skyboxNode) {
            extern void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj);
            sg_RenderSkyboxNode(skyboxNode, view, proj);
        }

        int totalBVHModels = 0;
        for (int i = 0; i < g_SceneBVH.itemCount; i++) {
            if (g_SceneBVH.items[i].type == BVH_ITEM_MODEL) totalBVHModels++;
        }
        g_CulledModelCount = totalBVHModels - drawnBVHModels;
    } else {
        // If no culling, all instances are visible
        auto setAllVisible = [](auto& self, SceneNode* n) -> void {
            if (!n) return;
            if (n->type == ENTITY_INSTANCE) {
                InstanceData* inst = &n->data.instance;
                if (inst->instanceCount > inst->visibleCapacity) {
                    inst->visibleCapacity = inst->instanceCount;
                    inst->visibleIndices = (int*)realloc(inst->visibleIndices, inst->visibleCapacity * sizeof(int));
                }
                for (int i = 0; i < inst->instanceCount; i++) inst->visibleIndices[i] = i;
                inst->visibleCount = inst->instanceCount;
            }
            for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
        };
        setAllVisible(setAllVisible, g_SceneRoot);

        sg_DrawNode(g_SceneRoot, view, proj, &g_VisibleModelCount);
    }

    if (g_DrawBoundingBoxes) {
        bbox_DrawSceneGraph(g_SceneRoot, view, proj);
    }

    glUseProgram(0);
}

void sg_InitNode(SceneNode* node) {
    if (!node) return;

    if (node->type == ENTITY_MODEL) {
        if (node->sourcePath[0] != '\0') {
            Mesh* meshes = nullptr;
            int meshCount = 0;
            extern Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale);
            if (loadModel(node->sourcePath, &meshes, &meshCount, 1.0f)) {
                if (node->meshIndex >= 0 && node->meshIndex < meshCount) {
                    node->data.mesh = meshes[node->meshIndex];
                }
            }
        }
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->modelPath[0] != '\0') {
            if (inst->instanceMeshes == nullptr) {
                extern Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale);
                loadModel(inst->modelPath, &inst->instanceMeshes, &inst->instanceMeshCount, 1.0f);
            }
        }
        if (inst->instanceCount == 0 && inst->matricesCapacity == 0) {
            extern void instance_GenerateInstances(InstanceData* inst);
            instance_GenerateInstances(inst);
        }
    } else if (node->type == ENTITY_TERRAIN) {
        extern void sg_InitTerrainNode(SceneNode* node);
        sg_InitTerrainNode(node);
    } else if (node->type == ENTITY_SKYBOX) {
        extern void sg_InitSkyboxNode(SceneNode* node);
        sg_InitSkyboxNode(node);
    } else if (node->type == ENTITY_CATMULLROMSPLINE) {
        sg_InitCatmullRomNode(node);
    }

    for (int i = 0; i < node->num_children; i++) {
        sg_InitNode(node->children[i]);
    }
}

// ============================================================================
// SCENE QUERY API
// ============================================================================

static SceneNode* sg_FindByIdRec(SceneNode* node, int id) {
    if (!node) return nullptr;
    if (node->ID == id) return node;
    for (int i = 0; i < node->num_children; i++) {
        SceneNode* found = sg_FindByIdRec(node->children[i], id);
        if (found) return found;
    }
    return nullptr;
}

static SceneNode* sg_FindByNameRec(SceneNode* node, const char* name) {
    if (!node) return nullptr;
    if (node->name && strcmp(node->name, name) == 0) return node;
    for (int i = 0; i < node->num_children; i++) {
        SceneNode* found = sg_FindByNameRec(node->children[i], name);
        if (found) return found;
    }
    return nullptr;
}

SceneNode* sg_FindById(int id)               { return sg_FindByIdRec(g_SceneRoot, id); }
SceneNode* sg_FindByName(const char* name)   { return sg_FindByNameRec(g_SceneRoot, name); }

// Null-safe typed data accessors.
LightData*       sg_Light(SceneNode* n)    { return (n && n->type == ENTITY_LIGHT)    ? &n->data.light          : nullptr; }
Camera*      sg_Camera(SceneNode* n)   { return (n && n->type == ENTITY_CAMERA)   ? &n->data.camera         : nullptr; }
Mesh*            sg_Mesh(SceneNode* n)     { return (n && n->type == ENTITY_MODEL)     ? &n->data.mesh           : nullptr; }
Material*        sg_Material(SceneNode* n) { return (n && n->type == ENTITY_MODEL)     ? &n->data.mesh.material  : nullptr; }
InstanceData*    sg_Instance(SceneNode* n) { return (n && n->type == ENTITY_INSTANCE)  ? &n->data.instance       : nullptr; }
TerrainNodeData* sg_Terrain(SceneNode* n)  { return (n && n->type == ENTITY_TERRAIN)   ? &n->data.terrain        : nullptr; }

// Returns Camera* for a camera node — edit position/target/fov directly.
Camera* sg_GetCamera(SceneNode* n) {
    if (!n || n->type != ENTITY_CAMERA) return nullptr;
    return &n->data.camera;
}

// Activates a camera node for rendering.
void sg_SetActiveCamera(SceneNode* n) {
    if (!n || n->type != ENTITY_CAMERA) return;
    g_ActiveCameraNode = n;
    currentCameraMode  = CAM_MODE_CUSTOM;
}
