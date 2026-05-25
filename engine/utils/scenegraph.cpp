#include "scenegraph.h"
#include "engine/utils/boundingbox.h"
#include "engine/utils/BVH.h"
#include "engine/utils/culling.h"
#include "engine/dependancies/vmath.h"
#include "engine/utils/catmulromspline.h"
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

extern unsigned int irradianceMap;

RenderDebugInfo g_LastFrameRenderOrder[256];
int             g_LastFrameRenderCount = 0;

static int  g_NextNodeId = 1;
static bool g_BVHDirty  = true;

void sg_MarkSceneDirty() { g_BVHDirty = true; }

void sg_SetAllVisible(SceneNode* node) {
    if (!node) return;
    if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->instanceCount > inst->visibleCapacity) {
            inst->visibleCapacity = inst->instanceCount;
            inst->visibleIndices = (int*)realloc(inst->visibleIndices, inst->visibleCapacity * sizeof(int));
        }
        for (int i = 0; i < inst->instanceCount; i++) inst->visibleIndices[i] = i;
        inst->visibleCount = inst->instanceCount;
    }
    for (int i = 0; i < node->num_children; i++) {
        sg_SetAllVisible(node->children[i]);
    }
}

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

    if (type == ENTITY_LIGHT) {
        LightData* ld = &node->data.light;
        ld->type = LIGHT_POINT;
        ld->color = vec3(1.0f, 1.0f, 1.0f);
        ld->intensity = 1.0f;
        ld->radius = 10.0f;
        ld->direction = vec3(0.0f, -1.0f, 0.0f);
        ld->innerCutoff = (float)cos(12.5 * 3.14159 / 180.0);
        ld->outerCutoff = (float)cos(17.5 * 3.14159 / 180.0);
        ld->shadowResolution = 2048;
        ld->shadowBias = 0.002f;
        ld->shadowOrthoSize = 200.0f;
        ld->shadowNear = 1.0f;
        ld->shadowFar = 400.0f;
        ld->shadowPolyFactor = 1.5f;
        ld->shadowPolyUnits = 4.0f;
    } else if (type == ENTITY_SKY_ATMOSPHERE) {
        SkyAtmosphereNodeData* ad = &node->data.skyAtmosphere;
        ad->bottomRadius = 6360.0f;
        ad->topRadius = 6460.0f;
        ad->groundAlbedo = vec3(0.3f, 0.3f, 0.3f);
        ad->rayleighScattering = vec3(5.802e-3f, 13.558e-3f, 33.1e-3f);
        ad->rayleighDensityExpScale = -0.125f;
        ad->mieScattering = 3.996e-3f;
        ad->mieAbsorption = 4.440e-4f;
        ad->mieAnisotropy = 0.8f;
        ad->mieDensityExpScale = -0.8333f;
        ad->absorptionExtinction = vec3(6.5e-4f, 1.881e-3f, 8.5e-5f);
        ad->sunDirection = vec3(0.0f, 0.342f, 0.940f);
        ad->sunColor = vec3(1.0f, 1.0f, 1.0f);
        ad->sunIntensity = 20.0f;
        ad->sunAngularRadius = 0.02f;
        ad->worldScale = 0.001f;
        ad->exposure = 20.0f;
        ad->lutsDirty = true;
        ad->shadowResolution = 2048;
        ad->shadowBias = 0.002f;
        ad->shadowOrthoSize = 200.0f;
        ad->shadowNear = 1.0f;
        ad->shadowFar = 400.0f;
        ad->shadowPolyFactor = 1.5f;
        ad->shadowPolyUnits = 4.0f;
    }

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

    if (node->type == ENTITY_VOLUMETRIC_CLOUD) {
        extern void sg_FreeVolumetricCloudNode(SceneNode* node);
        sg_FreeVolumetricCloudNode(node);
    }

    if (node->type == ENTITY_SKYBOX) {
        extern void sg_FreeSkyboxNode(SceneNode* node);
        sg_FreeSkyboxNode(node);
    }

    if (node->type == ENTITY_SKY_ATMOSPHERE) {
        extern void sg_FreeSkyAtmosphereNode(SceneNode* node);
        sg_FreeSkyAtmosphereNode(node);
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
// Fog Globals
vec3  g_FogColor(0.5f, 0.5f, 0.5f);
float g_FogDensity = 0.0f; // Default disabled
float g_FogStart = 10.0f;
float g_FogEnd = 1000.0f;
int   g_FogType = 0;
bool  g_FogEnabled = false;

// Aerial perspective globals — populated each frame from the sky atmosphere node
bool   g_AerialActive     = false;
GLuint g_AerialTransLUT   = 0;
GLuint g_AerialSkyViewLUT = 0;
float  g_AtmBotR          = 6360.0f;
float  g_AtmTopR          = 6420.0f;
float  g_AtmCamHeight     = 6360.001f;
float  g_AtmWorldScale    = 0.001f;
float  g_AtmExposure      = 10.0f;

static void setAerialPerspUniforms(ShaderProgram* program) {
    if (!program) return;
    glUniform1i(program->loc.uAerialPerspective, g_AerialActive ? 1 : 0);
    if (!g_AerialActive) return;
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, g_AerialTransLUT);
    glUniform1i(program->loc.uAerialTransmittanceLUT, 10);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, g_AerialSkyViewLUT);
    glUniform1i(program->loc.uAerialSkyViewLUT, 11);
    glUniform1f(program->loc.uAtmBotR,       g_AtmBotR);
    glUniform1f(program->loc.uAtmTopR,       g_AtmTopR);
    glUniform1f(program->loc.uAtmCamHeight,  g_AtmCamHeight);
    glUniform1f(program->loc.uAtmWorldScale, g_AtmWorldScale);
    glUniform1f(program->loc.uAtmExposure,   g_AtmExposure);
}

// Shadow globals — set each frame by the shadow pass, read by sg_DrawModelNode
bool   g_ShadowActive     = false;
mat4   g_ShadowSBPV;
GLuint g_ShadowDepthTexID = 0;
float  g_ShadowBias       = 0.002f;

void setFogUniforms(ShaderProgram* program) {
    if (!program) return;
    glUniform3fv(program->loc.uFogColor, 1, (float*)&g_FogColor);
    glUniform1f(program->loc.uFogDensity, g_FogDensity);
    glUniform1f(program->loc.uFogStart, g_FogStart);
    glUniform1f(program->loc.uFogEnd, g_FogEnd);
    glUniform1i(program->loc.uFogType, g_FogType);
    glUniform1i(program->loc.uFogEnabled, g_FogEnabled ? 1 : 0);
}

static void sg_DrawModelNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_MODEL) return;

    Mesh* mesh = &node->data.mesh;
    // Use PBR only when IBL is active (skybox present + irradiance map loaded).
    // Without IBL the PBR ambient is only 3% and the default point light is
    // outside its own radius, so models appear black. Lambert has a 10%
    // ambient that is always visible with just direct/default lighting.
    ShaderProgram* program = (useIBL && pbrShaderProgram) ? pbrShaderProgram : lambertShaderProgram;
    if (!program) program = pbrShaderProgram;
    if (!program || mesh->indexCount <= 0 || mesh->vao == 0) return;

    glUseProgram(program->id);

    glUniformMatrix4fv(program->loc.uProjection, 1, GL_FALSE, proj);
    glUniformMatrix4fv(program->loc.uView,       1, GL_FALSE, view);
    glUniformMatrix4fv(program->loc.uModel,      1, GL_FALSE, node->world_matrix);

    // Set default UV scale for models to 1.0 to avoid invisible textures
    glUniform1f(program->loc.uUVScale, 1.0f);
    glUniform1i(program->loc.uEnableStochastic, 0); // Always off for standard models

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

    setFogUniforms(program);
    setAerialPerspUniforms(program);

    extern void setDebugUniforms(ShaderProgram* program);
    setDebugUniforms(program);

    setMaterialUniforms(program, &mesh->material);

    // Shadow
    if (program->loc.uShadowEnabled >= 0) {
        glUniform1i(program->loc.uShadowEnabled, g_ShadowActive ? 1 : 0);
        if (g_ShadowActive) {
            glUniformMatrix4fv(program->loc.uShadowMatrix, 1, GL_FALSE, (const float*)g_ShadowSBPV);
            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_2D, g_ShadowDepthTexID);
            // Re-enable depth comparison for sampler2DShadow. We leave the texture
            // in GL_NONE compare mode during the depth pass (AMD/Mesa workaround)
            // and turn it on here, right before sampling.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glUniform1i(program->loc.uShadowMap, 9);
            glUniform1f(program->loc.uShadowBias, g_ShadowBias);

            static int s_dbgDraws = 0;
            if (s_dbgDraws < 2) {
                s_dbgDraws++;
                extern FILE* gpFile;
                fprintf(gpFile,
                    "[Shadow DBG] sg_DrawModelNode: prog=%u uShadowEnabled_loc=%d "
                    "uShadowMatrix_loc=%d uShadowMap_loc=%d texID=%u "
                    "sbpv[0][0]=%.3f\n",
                    program->id,
                    program->loc.uShadowEnabled,
                    program->loc.uShadowMatrix,
                    program->loc.uShadowMap,
                    g_ShadowDepthTexID,
                    g_ShadowSBPV[0][0]);
            }
        }
    }

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

// Renders only geometry nodes that receive direct lighting (skips skybox, splines, etc.).
// Used for additive extra-light passes so the skybox is not brightened per-light.
static void sg_DrawLitGeo(SceneNode* node, mat4 view, mat4 proj) {
    if (!node) return;
    if (node->type == ENTITY_MODEL) {
        sg_DrawModelNode(node, view, proj);
    } else if (node->type == ENTITY_TERRAIN) {
        sg_DrawTerrainNode(node, view, proj);
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->modelPath[0] != '\0' && inst->instanceMeshes != nullptr) {
            extern void instance_UploadToGPU(InstanceData* inst);
            extern void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj);
            instance_UploadToGPU(inst);
            for (int i = 0; i < inst->instanceMeshCount; i++)
                instance_DrawInstances(node, &inst->instanceMeshes[i], view, proj);
        }
    }
    for (int i = 0; i < node->num_children; i++)
        sg_DrawLitGeo(node->children[i], view, proj);
}

static void sg_DrawChildrenOrdered(SceneNode* parent, mat4 view, mat4 proj, int* nodesDrawn);

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
    } else if (node->type == ENTITY_VOLUMETRIC_CLOUD) {
        extern void sg_RenderVolumetricCloudNode(SceneNode* node, mat4 view, mat4 proj);
        sg_RenderVolumetricCloudNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    } else if (node->type == ENTITY_SKY_ATMOSPHERE) {
        extern void sg_RenderSkyAtmosphereNode(SceneNode* node, mat4 view, mat4 proj);
        sg_RenderSkyAtmosphereNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    } else if (node->type == ENTITY_OCEAN) {
        extern void sg_DrawOceanNode(SceneNode* node, mat4 view, mat4 proj);
        sg_DrawOceanNode(node, view, proj);
        if (nodesDrawn) (*nodesDrawn)++;
    }
    sg_DrawChildrenOrdered(node, view, proj, nodesDrawn);
}

static void sg_DrawChildrenOrdered(SceneNode* parent, mat4 view, mat4 proj, int* nodesDrawn) {
    int n = parent->num_children;
    if (n == 0) return;

    extern vec3 GetActiveCameraPosition();
    vec3 cam = GetActiveCameraPosition();

    auto fired = [&](SceneNode* c) -> bool {
        const RenderOrderRule& r = c->renderRule;
        if (!r.enabled) return false;
        switch (r.condition) {
            case RENDER_COND_ALWAYS:         return true;
            case RENDER_COND_CAMERA_ABOVE_Y: return cam[1] > r.threshold;
            case RENDER_COND_CAMERA_BELOW_Y: return cam[1] < r.threshold;
            case RENDER_COND_CAMERA_NEAR: {
                vec3 cp(c->world_matrix[3][0], c->world_matrix[3][1], c->world_matrix[3][2]);
                return length(cam - cp) < r.threshold;
            }
            case RENDER_COND_CAMERA_FAR: {
                vec3 cp(c->world_matrix[3][0], c->world_matrix[3][1], c->world_matrix[3][2]);
                return length(cam - cp) > r.threshold;
            }
        }
        return false;
    };

    // Build render order: start with non-fired nodes in original order,
    // then splice fired nodes in at their requested target index.
    SceneNode* order[256];
    int        cnt = 0;
    for (int i = 0; i < n && cnt < 256; i++)
        if (!fired(parent->children[i]))
            order[cnt++] = parent->children[i];

    for (int i = 0; i < n; i++) {
        SceneNode* c = parent->children[i];
        if (!fired(c)) continue;
        int t = c->renderRule.targetIndex;
        if (t < 0 || t >= cnt) t = cnt;          // -1 or out-of-range → append last
        for (int j = cnt; j > t; j--)             // shift right to make room
            order[j] = order[j - 1];
        order[t] = c;
        cnt++;
    }

    for (int i = 0; i < cnt; i++)
        sg_DrawNode(order[i], view, proj, nodesDrawn);
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

void sg_DrawDepthNodes(SceneNode* node, mat4 vp, ShaderProgram* shader, ShaderProgram* instancedShader) {
    if (!node || !shader) return;

    if (node->type == ENTITY_MODEL) {
        Mesh* mesh = &node->data.mesh;
        if (mesh->indexCount > 0 && mesh->vao != 0) {
            glUseProgram(shader->id);
            glUniformMatrix4fv(glGetUniformLocation(shader->id, "mvp"), 1, GL_FALSE, vp * node->world_matrix);
            glBindVertexArray(mesh->vao);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh->indexCount, GL_UNSIGNED_INT, 0);
        }
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->modelPath[0] != '\0' && inst->instanceMeshes != nullptr) {
            ShaderProgram* targetShader = instancedShader ? instancedShader : shader;
            extern void instance_UploadToGPU(InstanceData* inst);
            extern void instance_DrawDepthOnly(SceneNode* node, Mesh* mesh, mat4 vp, ShaderProgram* shader);
            instance_UploadToGPU(inst);
            for (int i = 0; i < inst->instanceMeshCount; i++)
                instance_DrawDepthOnly(node, &inst->instanceMeshes[i], vp, targetShader);
        }
    }

    for (int i = 0; i < node->num_children; i++) {
        sg_DrawDepthNodes(node->children[i], vp, shader, instancedShader);
    }
}

void sg_SyncFirstLight(SceneNode* root) {
    if (!root) return;
    sg_UpdateWorldMatrix(root, vmath::mat4::identity());

    auto find = [](auto& self, SceneNode* n) -> SceneNode* {
        if (!n) return nullptr;
        if (n->type == ENTITY_LIGHT) return n;
        for (int i = 0; i < n->num_children; i++) {
            SceneNode* f = self(self, n->children[i]);
            if (f) return f;
        }
        return nullptr;
    };
    SceneNode* ln = find(find, root);
    if (!ln) return;

    LightData* l = &ln->data.light;
    lightPos   = vmath::vec3(ln->world_matrix[3][0], ln->world_matrix[3][1], ln->world_matrix[3][2]);
    lightColor = vmath::vec3(l->color[0], l->color[1], l->color[2]);
    lightType  = l->type;

    vmath::mat4 rot = ln->world_matrix;
    rot[3][0] = rot[3][1] = rot[3][2] = 0.0f;
    vmath::vec3 wDir;
    wDirManualMult(l->direction, rot, wDir);
    lightDir = (vmath::length(wDir) > 0.001f) ? vmath::normalize(wDir) : l->direction;
}

void RenderSceneModels(mat4 view, mat4 proj) {
    g_LastFrameRenderCount = 0;
    
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

    // Collect all lights in scene (up to 16)
    const int MAX_SCENE_LIGHTS = 16;
    SceneNode* sceneLights[MAX_SCENE_LIGHTS];
    int sceneLightCount = 0;
    auto collectLights = [&](auto& self, SceneNode* n) -> void {
        if (!n || sceneLightCount >= MAX_SCENE_LIGHTS) return;
        if (n->type == ENTITY_LIGHT) sceneLights[sceneLightCount++] = n;
        for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
    };
    collectLights(collectLights, g_SceneRoot);

    auto syncLightGlobals = [&](SceneNode* ln) {
        LightData* l   = &ln->data.light;
        lightPos       = vec3(ln->world_matrix[3][0], ln->world_matrix[3][1], ln->world_matrix[3][2]);
        lightColor     = vec3(l->color[0], l->color[1], l->color[2]);
        lightIntensity = l->intensity;
        lightType      = l->type;
        lightRadius    = l->radius;
        mat4 rot = ln->world_matrix;
        rot[3][0] = 0; rot[3][1] = 0; rot[3][2] = 0;
        vec3 wDir;
        wDirManualMult(l->direction, rot, wDir);
        lightDir         = (vmath::length(wDir) > 0.001f) ? normalize(wDir) : l->direction;
        lightInnerCutoff = l->innerCutoff;
        lightOuterCutoff = l->outerCutoff;
    };

    if (sceneLightCount > 0)
        syncLightGlobals(sceneLights[0]);

    auto findSkyProvider = [](auto& self, SceneNode* n) -> SceneNode* {
        if (!n) return nullptr;
        if (n->type == ENTITY_SKYBOX || n->type == ENTITY_SKY_ATMOSPHERE) return n;
        for (int i = 0; i < n->num_children; i++) {
            SceneNode* found = self(self, n->children[i]);
            if (found) return found;
        }
        return nullptr;
    };
    SceneNode* skyNode = findSkyProvider(findSkyProvider, g_SceneRoot);
    useIBL = (irradianceMap != 0); // If we have baked IBL maps, use them.

    // Update aerial perspective globals from atmosphere node
    g_AerialActive = false;
    if (skyNode && skyNode->type == ENTITY_SKY_ATMOSPHERE) {
        SkyAtmosphereNodeData* atmo = &skyNode->data.skyAtmosphere;
        if (atmo->transmittanceLUT && atmo->skyViewLUT) {
            g_AerialActive     = true;
            g_AerialTransLUT   = atmo->transmittanceLUT;
            g_AerialSkyViewLUT = atmo->skyViewLUT;
            g_AtmBotR          = atmo->bottomRadius;
            g_AtmTopR          = atmo->topRadius;
            g_AtmWorldScale    = atmo->worldScale;
            g_AtmExposure      = atmo->exposure;
            // Camera height in atmosphere space
            extern SceneNode* g_ActiveCameraNode;
            vec3 camPos = vec3(0.0f, 0.0f, 0.0f);
            if (g_ActiveCameraNode) {
                Camera* cam = sg_Camera(g_ActiveCameraNode);
                if (cam) camPos = cam->position;
            }
            g_AtmCamHeight = atmo->bottomRadius + fmaxf(0.0f, camPos[1] * atmo->worldScale);
        }
    }

    // Sync fog from fog node if it exists
    auto findFogNode = [](auto& self, SceneNode* n) -> SceneNode* {
        if (!n) return nullptr;
        if (n->type == ENTITY_FOG) return n;
        for (int i = 0; i < n->num_children; i++) {
            SceneNode* found = self(self, n->children[i]);
            if (found) return found;
        }
        return nullptr;
    };
    SceneNode* fogNode = findFogNode(findFogNode, g_SceneRoot);
    if (fogNode) {
        FogNodeData* fd = &fogNode->data.fog;
        g_FogColor = fd->color;
        g_FogDensity = fd->density;
        g_FogStart = fd->start;
        g_FogEnd = fd->end;
        g_FogType = fd->type;
        g_FogEnabled = fd->enabled;
    } else {
        g_FogEnabled = false;
    }

    // Synchronize sun parameters from atmosphere to clouds
    if (skyNode && skyNode->type == ENTITY_SKY_ATMOSPHERE) {
        SkyAtmosphereNodeData* atmo = &skyNode->data.skyAtmosphere;
        auto syncClouds = [&](auto& self, SceneNode* n) -> void {
            if (!n) return;
            if (n->type == ENTITY_VOLUMETRIC_CLOUD) {
                VolumetricCloudNodeData* c = &n->data.volumetricCloud;
                c->sunDirection = atmo->sunDirection;
                c->sunColor     = atmo->sunColor;
                c->sunIntensity = atmo->sunIntensity;
            }
            for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
        };
        syncClouds(syncClouds, g_SceneRoot);
    }

    // Shadow pass — render depth from light's POV before main camera pass
    g_ShadowActive = false;

    // Clear any pre-existing GL errors to avoid confusing them with shadow pass errors
    while(glGetError() != GL_NO_ERROR);

    // Helper: runs the actual depth-only draw for a given shadow map
    auto runShadowPass = [&](ShadowMap* sm) {
        // shadow_BeginPass / shadow_EndPass now snapshot & restore all relevant
        // GL state (FBO, viewport, scissor, depth state, color mask). No need
        // to wrap them here.
        shadow_BeginPass(sm);

        int drawCount = 0;
        if (sm->depthProgram && sm->depthProgram->id) {
            glUseProgram(sm->depthProgram->id);
            auto drawDepth = [&](auto& self, SceneNode* n) -> void {
                if (!n) return;
                if (n->type == ENTITY_MODEL) {
                    Mesh* m = &n->data.mesh;
                    if (m->vao && m->indexCount > 0) {
                        glUseProgram(sm->depthProgram->id);
                        drawCount += shadow_DrawDepth(sm, m->vao, (int)m->indexCount, n->world_matrix);
                    }
                } else if (n->type == ENTITY_INSTANCE) {
                    InstanceData* inst = &n->data.instance;
                    extern void instance_UploadToGPU(InstanceData* inst);
                    extern void instance_DrawDepthOnly(SceneNode* node, Mesh* mesh, mat4 vp, ShaderProgram* shader);
                    if (instancedShadowProgram) {
                        inst->visibleCount = inst->instanceCount; // For shadow pass, use all instances
                        instance_UploadToGPU(inst);
                        for (int i = 0; i < inst->instanceMeshCount; i++) {
                            instance_DrawDepthOnly(n, &inst->instanceMeshes[i], sm->lightProj * sm->lightView, instancedShadowProgram);
                            drawCount++;
                        }
                    }
                }
                for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
            };
            drawDepth(drawDepth, g_SceneRoot);
        }

        shadow_EndPass(sm);

        // One-time per-enable diagnostic
        static int s_dbgFrames = 0;
        if (s_dbgFrames < 3) {
            s_dbgFrames++;
            extern FILE* gpFile;
            fprintf(gpFile,
                "[Shadow DBG] frame=%d depthProg=%u locMVP=%d drawCalls=%d "
                "fboID=%u texID=%u sbpv[0][0]=%.3f\n",
                s_dbgFrames,
                sm->depthProgram ? sm->depthProgram->id : 0,
                sm->locDepthMVP,
                drawCount,
                sm->fboID, sm->depthTexID,
                sm->sbpv[0][0]);

            // Check GL error after shadow pass
            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
                fprintf(gpFile, "[Shadow DBG] GL error after pass: 0x%x\n", err);
        }

        g_ShadowActive     = true;
        g_ShadowDepthTexID = sm->depthTexID;
        g_ShadowSBPV       = sm->sbpv;
        g_ShadowBias       = sm->bias;
    };

    // Priority 1: explicit shadow-casting light
    if (sceneLightCount > 0) {
        LightData* ld = &sceneLights[0]->data.light;
        if (ld->castShadow && ld->shadow && ld->type != LIGHT_POINT) {
            shadow_UpdateMatrices(ld->shadow, ld->type, lightPos, lightDir, ld->outerCutoff);
            runShadowPass(ld->shadow);
        }
    }

    // Priority 2: atmospheric sky — sun drives a directional shadow
    if (!g_ShadowActive && skyNode && skyNode->type == ENTITY_SKY_ATMOSPHERE) {
        SkyAtmosphereNodeData* atmo = &skyNode->data.skyAtmosphere;
        if (atmo->castShadow && atmo->shadow) {
            vec3 sd = atmo->sunDirection;
            float sdLen = sqrtf(sd[0]*sd[0] + sd[1]*sd[1] + sd[2]*sd[2]);
            if (sdLen > 0.001f) {  // skip if sun hasn't been set yet (would produce NaN matrix)
                vec3 sunLightDir = vec3(-sd[0]/sdLen, -sd[1]/sdLen, -sd[2]/sdLen);
                extern vec3 GetActiveCameraPosition();
                shadow_UpdateMatrices(atmo->shadow, LIGHT_DIRECTIONAL,
                                      GetActiveCameraPosition(), sunLightDir, 0.0f);
                runShadowPass(atmo->shadow);
            }
        }
    }

    // Ensure correct GL state (ImGui may have changed these)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    g_VisibleModelCount = 0;
    g_VisibleLightCount = 0;
    g_CulledModelCount  = 0;

    sg_ClearVisibleInstances(g_SceneRoot);

    // ---- Sorting Logic ----
    extern vec3 GetActiveCameraPosition();
    extern SceneNode* sg_GetActiveCameraNode();
    vec3 camPos = GetActiveCameraPosition();
    
    // Check if current camera wants distance sorting
    bool doSort = false;
    SceneNode* activeCamNode = sg_GetActiveCameraNode();
    if (activeCamNode && activeCamNode->data.camera.useDistanceSorting) {
        doSort = true;
    } else {
        extern Camera* g_EditorCamera;
        if (g_EditorCamera && g_EditorCamera->useDistanceSorting) doSort = true;
    }

    if (doSort) {
        struct SortItem {
            SceneNode* node;
            float distSq;
        };

        // Three separate buckets — sky renders first (background), transparent
        // objects must be sorted back-to-front for correct alpha compositing,
        // and volumetric effects composite over everything at the end.
        std::vector<SortItem> skyItems;
        std::vector<SortItem> opaqueItems;
        std::vector<SortItem> transparentItems;
        std::vector<SortItem> effectItems;

        auto collect = [&](auto& self, SceneNode* n) -> void {
            if (!n) return;

            vec3 pos  = vec3(n->world_matrix[3][0], n->world_matrix[3][1], n->world_matrix[3][2]);
            vec3 diff = pos - camPos;
            float d2  = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];

            if (n->type == ENTITY_SKYBOX || n->type == ENTITY_SKY_ATMOSPHERE) {
                skyItems.push_back({n, d2});
            } else if (n->type == ENTITY_VOLUMETRIC_CLOUD) {
                // Decide bucket based on camera's position relative to the cloud volume.
                // Inside or above → render as a full-screen overlay (effectItems, drawn last).
                // Outside/below   → sort back-to-front with scene so geometry in front occludes it.
                VolumetricCloudNodeData* c = &n->data.volumetricCloud;
                bool insideOrAbove = false;
                if (c->useSphereField) {
                    insideOrAbove = camPos[1] >= c->cloudBaseHeight;
                } else {
                    float sy = vmath::length(vec3(n->world_matrix[1][0], n->world_matrix[1][1], n->world_matrix[1][2]));
                    float halfY = c->boxSize[1] * (sy > 0.0001f ? sy : 1.0f) * 0.5f;
                    float cloudBottom = n->world_matrix[3][1] - halfY;
                    insideOrAbove = camPos[1] >= cloudBottom;
                }
                if (insideOrAbove)
                    effectItems.push_back({n, d2});
                else
                    transparentItems.push_back({n, d2});
            } else if (n->type == ENTITY_MODEL) {
                bool isTransparent = n->data.mesh.material.opacity < 1.0f;
                if (isTransparent)
                    transparentItems.push_back({n, d2});
                else
                    opaqueItems.push_back({n, d2});
            } else if (n->type == ENTITY_INSTANCE || n->type == ENTITY_TERRAIN ||
                       n->type == ENTITY_CATMULLROMSPLINE) {
                opaqueItems.push_back({n, d2});
            } else if (n->type == ENTITY_OCEAN) {
                transparentItems.push_back({n, d2});
            }

            for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
        };
        collect(collect, g_SceneRoot);

        // Opaque: Front-to-Back for early-z rejection
        std::sort(opaqueItems.begin(), opaqueItems.end(), [](const SortItem& a, const SortItem& b) {
            return a.distSq < b.distSq;
        });
        // Transparent: Back-to-Front so farther fragments are under closer ones
        std::sort(transparentItems.begin(), transparentItems.end(), [](const SortItem& a, const SortItem& b) {
            return a.distSq > b.distSq;
        });

        auto drawSortedNode = [&](SceneNode* n) {
            if (n->type == ENTITY_MODEL) {
                sg_DrawModelNode(n, view, proj);
                g_VisibleModelCount++;
            } else if (n->type == ENTITY_INSTANCE) {
                InstanceData* inst = &n->data.instance;
                extern void instance_UploadToGPU(InstanceData* inst);
                extern void instance_DrawInstances(SceneNode* node, Mesh* mesh, mat4 view, mat4 proj);
                inst->visibleCount = inst->instanceCount;
                instance_UploadToGPU(inst);
                for (int i = 0; i < inst->instanceMeshCount; i++)
                    instance_DrawInstances(n, &inst->instanceMeshes[i], view, proj);
                g_VisibleModelCount += inst->instanceCount;
            } else if (n->type == ENTITY_TERRAIN) {
                extern void sg_RenderTerrainNode(SceneNode* node, mat4 view, mat4 proj);
                sg_RenderTerrainNode(n, view, proj);
                g_VisibleModelCount++;
            } else if (n->type == ENTITY_SKYBOX) {
                extern void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj);
                sg_RenderSkyboxNode(n, view, proj);
            } else if (n->type == ENTITY_SKY_ATMOSPHERE) {
                extern void sg_RenderSkyAtmosphereNode(SceneNode* node, mat4 view, mat4 proj);
                sg_RenderSkyAtmosphereNode(n, view, proj);
            } else if (n->type == ENTITY_VOLUMETRIC_CLOUD) {
                extern void sg_RenderVolumetricCloudNode(SceneNode* node, mat4 view, mat4 proj);
                sg_RenderVolumetricCloudNode(n, view, proj);
            } else if (n->type == ENTITY_CATMULLROMSPLINE) {
                sg_RenderCatmullRomNode(n, view, proj);
            } else if (n->type == ENTITY_OCEAN) {
                extern void sg_DrawOceanNode(SceneNode* node, mat4 view, mat4 proj);
                sg_DrawOceanNode(n, view, proj);
            }
        };

        auto recordDebug = [&](const SortItem& item) {
            if (g_LastFrameRenderCount < 256) {
                RenderDebugInfo& d = g_LastFrameRenderOrder[g_LastFrameRenderCount++];
                strncpy(d.name, item.node->name ? item.node->name : "Unnamed", 63);
                d.name[63] = '\0';
                d.dist = sqrtf(item.distSq);
                d.type = (int)item.node->type;
            }
        };

        // 1. Sky / background (no distance sort needed — always fills the background)
        for (auto& item : skyItems)        { recordDebug(item); drawSortedNode(item.node); }
        // 2. Opaque geometry, Front-to-Back
        for (auto& item : opaqueItems)     { recordDebug(item); drawSortedNode(item.node); }
        // 3. Transparent geometry, Back-to-Front; don't write depth so layers stack correctly
        glDepthMask(GL_FALSE);
        for (auto& item : transparentItems) { recordDebug(item); drawSortedNode(item.node); }
        glDepthMask(GL_TRUE);
        // 4. Volumetric effects composite over the rest
        for (auto& item : effectItems)     { recordDebug(item); drawSortedNode(item.node); }
    } else {
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

                inst->visibleCount = 0;
                for (int j = 0; j < inst->instanceCount; j++) {
                    // Transform instance local AABB to world space for culling
                    AABB worldAABB = bbox_Transform(inst->instanceLocalBounds[j], node->world_matrix);
                    if (cull_TestAABB(&g_RenderFrustum, worldAABB)) {
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
        if (skyNode && skyNode->type == ENTITY_SKYBOX) {
            extern void sg_RenderSkyboxNode(SceneNode* node, mat4 view, mat4 proj);
            sg_RenderSkyboxNode(skyNode, view, proj);
        }

        // Volumetric clouds are not in the BVH — walk the tree and render all of them
        {
            extern void sg_RenderVolumetricCloudNode(SceneNode* node, mat4 view, mat4 proj);
            auto drawClouds = [&](auto& self, SceneNode* n) -> void {
                if (!n) return;
                if (n->type == ENTITY_VOLUMETRIC_CLOUD)
                    sg_RenderVolumetricCloudNode(n, view, proj);
                for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
            };
            drawClouds(drawClouds, g_SceneRoot);
        }

        // Sky Atmosphere — not in BVH, walk tree and render all instances
        {
            extern void sg_RenderSkyAtmosphereNode(SceneNode* node, mat4 view, mat4 proj);
            auto drawAtmo = [&](auto& self, SceneNode* n) -> void {
                if (!n) return;
                if (n->type == ENTITY_SKY_ATMOSPHERE)
                    sg_RenderSkyAtmosphereNode(n, view, proj);
                for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
            };
            drawAtmo(drawAtmo, g_SceneRoot);
        }

        // Ocean is not in BVH — walk the tree and render all ocean nodes
        {
            extern void sg_DrawOceanNode(SceneNode* node, mat4 view, mat4 proj);
            auto drawOceans = [&](auto& self, SceneNode* n) -> void {
                if (!n) return;
                if (n->type == ENTITY_OCEAN)
                    sg_DrawOceanNode(n, view, proj);
                for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
            };
            drawOceans(drawOceans, g_SceneRoot);
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

    // Additive passes for each extra light (lights[1..N])
    if (sceneLightCount > 1) {
        bool savedIBL = useIBL;
        useIBL = false;
        g_forceAdditiveBlend = true;
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthFunc(GL_EQUAL);
        glDepthMask(GL_FALSE);
        for (int li = 1; li < sceneLightCount; li++) {
            syncLightGlobals(sceneLights[li]);
            sg_DrawLitGeo(g_SceneRoot, view, proj);
        }
        g_forceAdditiveBlend = false;
        useIBL = savedIBL;
        glDisable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

    if (g_DrawBoundingBoxes) {
        bbox_DrawSceneGraph(g_SceneRoot, view, proj);
    }

    // Reset shadow texture compare mode back to GL_NONE so other systems
    // (e.g., the ImGui debug viewer) can sample it as a plain sampler2D.
    // The lit pass re-enables GL_COMPARE_REF_TO_TEXTURE per draw.
    if (g_ShadowActive && g_ShadowDepthTexID) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, g_ShadowDepthTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

    glUseProgram(0);
}
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
    } else if (node->type == ENTITY_LIGHT) {
        LightData* ld = &node->data.light;
        if (ld->castShadow && !ld->shadow) {
            if (ld->shadowResolution <= 0) ld->shadowResolution = 2048;
            extern ShadowMap* shadow_Create(int resolution);
            ld->shadow = shadow_Create(ld->shadowResolution);
            if (ld->shadow) {
                if (ld->shadowBias != 0.0f)      ld->shadow->bias = ld->shadowBias;
                if (ld->shadowOrthoSize != 0.0f) ld->shadow->orthoSize = ld->shadowOrthoSize;
                if (ld->shadowNear != 0.0f)      ld->shadow->nearPlane = ld->shadowNear;
                if (ld->shadowFar != 0.0f)       ld->shadow->farPlane = ld->shadowFar;
                if (ld->shadowPolyFactor != 0.0f) ld->shadow->polyOffsetFactor = ld->shadowPolyFactor;
                if (ld->shadowPolyUnits != 0.0f)  ld->shadow->polyOffsetUnits = ld->shadowPolyUnits;
            }
        }
    } else if (node->type == ENTITY_VOLUMETRIC_CLOUD) {
        extern void sg_InitVolumetricCloudNode(SceneNode* node);
        sg_InitVolumetricCloudNode(node);
    } else if (node->type == ENTITY_SKY_ATMOSPHERE) {
        extern void sg_InitSkyAtmosphereNode(SceneNode* node);
        sg_InitSkyAtmosphereNode(node);
        SkyAtmosphereNodeData* ad = &node->data.skyAtmosphere;
        if (ad->castShadow && !ad->shadow) {
            if (ad->shadowResolution <= 0) ad->shadowResolution = 2048;
            extern ShadowMap* shadow_Create(int resolution);
            ad->shadow = shadow_Create(ad->shadowResolution);
            if (ad->shadow) {
                if (ad->shadowBias != 0.0f)      ad->shadow->bias = ad->shadowBias;
                if (ad->shadowOrthoSize != 0.0f) ad->shadow->orthoSize = ad->shadowOrthoSize;
                if (ad->shadowNear != 0.0f)      ad->shadow->nearPlane = ad->shadowNear;
                if (ad->shadowFar != 0.0f)       ad->shadow->farPlane = ad->shadowFar;
                if (ad->shadowPolyFactor != 0.0f) ad->shadow->polyOffsetFactor = ad->shadowPolyFactor;
                if (ad->shadowPolyUnits != 0.0f)  ad->shadow->polyOffsetUnits = ad->shadowPolyUnits;
            }
        }
    } else if (node->type == ENTITY_FOG) {
        FogNodeData* fd = &node->data.fog;
        fd->color = vec3(0.5f, 0.5f, 0.5f);
        fd->density = 0.01f;
        fd->start = 10.0f;
        fd->end = 1000.0f;
        fd->type = 0;
        fd->enabled = true;
    } else if (node->type == ENTITY_OCEAN) {
        extern void sg_InitOceanNode(SceneNode* node);
        sg_InitOceanNode(node);
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
OceanNodeData*   sg_Ocean(SceneNode* n)   { return (n && n->type == ENTITY_OCEAN)     ? &n->data.ocean          : nullptr; }

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
