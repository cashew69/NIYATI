#include "boundingbox.h"

bool g_DrawBoundingBoxes = false;

// --- Construction ---------------------------------------------------------

AABB bbox_Empty(void) {
    AABB b;
    b.min = vec3( 1e30f,  1e30f,  1e30f);
    b.max = vec3(-1e30f, -1e30f, -1e30f);
    return b;
}

bool bbox_IsEmpty(AABB box) {
    return box.min[0] > box.max[0] || box.min[1] > box.max[1] || box.min[2] > box.max[2];
}

void bbox_Expand(AABB* box, vec3 p) {
    if (!box) return;
    if (p[0] < box->min[0]) box->min[0] = p[0];
    if (p[1] < box->min[1]) box->min[1] = p[1];
    if (p[2] < box->min[2]) box->min[2] = p[2];
    if (p[0] > box->max[0]) box->max[0] = p[0];
    if (p[1] > box->max[1]) box->max[1] = p[1];
    if (p[2] > box->max[2]) box->max[2] = p[2];
}

void bbox_Combine(AABB* dst, const AABB* other) {
    if (!dst || !other) return;
    if (other->min[0] < dst->min[0]) dst->min[0] = other->min[0];
    if (other->min[1] < dst->min[1]) dst->min[1] = other->min[1];
    if (other->min[2] < dst->min[2]) dst->min[2] = other->min[2];
    if (other->max[0] > dst->max[0]) dst->max[0] = other->max[0];
    if (other->max[1] > dst->max[1]) dst->max[1] = other->max[1];
    if (other->max[2] > dst->max[2]) dst->max[2] = other->max[2];
}

AABB bbox_FromPoints(const float* xyz, int vertexCount) {
    AABB b = bbox_Empty();
    if (!xyz || vertexCount <= 0) {
        b.min = vec3(0,0,0);
        b.max = vec3(0,0,0);
        return b;
    }
    for (int i = 0; i < vertexCount; i++) {
        bbox_Expand(&b, vec3(xyz[i*3+0], xyz[i*3+1], xyz[i*3+2]));
    }
    return b;
}

// Multiply mat4 (column-major) by a vec4 — vmath has no built-in overload.
static vec4 mat4_mul_vec4(const mat4& m, vec4 v) {
    vec4 r;
    r[0] = m[0][0]*v[0] + m[1][0]*v[1] + m[2][0]*v[2] + m[3][0]*v[3];
    r[1] = m[0][1]*v[0] + m[1][1]*v[1] + m[2][1]*v[2] + m[3][1]*v[3];
    r[2] = m[0][2]*v[0] + m[1][2]*v[1] + m[2][2]*v[2] + m[3][2]*v[3];
    r[3] = m[0][3]*v[0] + m[1][3]*v[1] + m[2][3]*v[2] + m[3][3]*v[3];
    return r;
}

// Transform 8 corners and rebuild a tight world-space AABB.
AABB bbox_Transform(AABB local, const mat4& world) {
    AABB out = bbox_Empty();
    if (bbox_IsEmpty(local)) {
        out.min = vec3(0,0,0); out.max = vec3(0,0,0);
        return out;
    }
    vec3 c[8] = {
        vec3(local.min[0], local.min[1], local.min[2]),
        vec3(local.max[0], local.min[1], local.min[2]),
        vec3(local.min[0], local.max[1], local.min[2]),
        vec3(local.max[0], local.max[1], local.min[2]),
        vec3(local.min[0], local.min[1], local.max[2]),
        vec3(local.max[0], local.min[1], local.max[2]),
        vec3(local.min[0], local.max[1], local.max[2]),
        vec3(local.max[0], local.max[1], local.max[2]),
    };
    for (int i = 0; i < 8; i++) {
        vec4 wp = mat4_mul_vec4(world, vec4(c[i][0], c[i][1], c[i][2], 1.0f));
        bbox_Expand(&out, vec3(wp[0], wp[1], wp[2]));
    }
    return out;
}

AABB bbox_FromLight(vec3 position, float radius) {
    AABB b;
    if (radius <= 0.0f) radius = 1.0f;
    b.min = vec3(position[0] - radius, position[1] - radius, position[2] - radius);
    b.max = vec3(position[0] + radius, position[1] + radius, position[2] + radius);
    return b;
}

vec3 bbox_Center(AABB box) {
    return vec3(
        (box.min[0] + box.max[0]) * 0.5f,
        (box.min[1] + box.max[1]) * 0.5f,
        (box.min[2] + box.max[2]) * 0.5f);
}

vec3 bbox_Extent(AABB box) {
    return vec3(
        (box.max[0] - box.min[0]) * 0.5f,
        (box.max[1] - box.min[1]) * 0.5f,
        (box.max[2] - box.min[2]) * 0.5f);
}

float bbox_SurfaceArea(AABB box) {
    vec3 d = vec3(box.max[0]-box.min[0], box.max[1]-box.min[1], box.max[2]-box.min[2]);
    return 2.0f * (d[0]*d[1] + d[1]*d[2] + d[2]*d[0]);
}

// --- Visualizer -----------------------------------------------------------

static GLuint s_bboxVAO = 0;
static GLuint s_bboxVBO = 0;

// Upload a 24-vertex (12 edges) line buffer for the unit cube [-1,1]^3.
static void bbox_InitGL(void) {
    if (s_bboxVAO != 0) return;

    // 12 edges of a cube — 24 vertices (lines)
    float v[] = {
        // bottom rectangle
        -1,-1,-1,   1,-1,-1,
         1,-1,-1,   1,-1, 1,
         1,-1, 1,  -1,-1, 1,
        -1,-1, 1,  -1,-1,-1,
        // top rectangle
        -1, 1,-1,   1, 1,-1,
         1, 1,-1,   1, 1, 1,
         1, 1, 1,  -1, 1, 1,
        -1, 1, 1,  -1, 1,-1,
        // verticals
        -1,-1,-1,  -1, 1,-1,
         1,-1,-1,   1, 1,-1,
         1,-1, 1,   1, 1, 1,
        -1,-1, 1,  -1, 1, 1,
    };

    glGenVertexArrays(1, &s_bboxVAO);
    glGenBuffers(1, &s_bboxVBO);
    glBindVertexArray(s_bboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_bboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glDisableVertexAttribArray(ATTRIB_COLOR);

    glBindVertexArray(0);
}

void bbox_DrawAABB(AABB box, vec3 color, mat4 view, mat4 proj) {
    if (!lineShaderProgram) return;
    if (bbox_IsEmpty(box)) return;
    bbox_InitGL();

    vec3 c = bbox_Center(box);
    vec3 e = bbox_Extent(box);
    if (e[0] <= 0.0f) e[0] = 0.001f;
    if (e[1] <= 0.0f) e[1] = 0.001f;
    if (e[2] <= 0.0f) e[2] = 0.001f;

    mat4 model = vmath::translate(c[0], c[1], c[2]) * vmath::scale(e[0], e[1], e[2]);

    glUseProgram(lineShaderProgram->id);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "view"),       1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "projection"), 1, GL_FALSE, proj);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "model"),      1, GL_FALSE, model);

    // The line shader pulls color from a vertex attribute; supply a constant.
    glDisableVertexAttribArray(ATTRIB_COLOR);
    glVertexAttrib3f(ATTRIB_COLOR, color[0], color[1], color[2]);

    glBindVertexArray(s_bboxVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
    glUseProgram(0);
}

void bbox_DrawAABBWithAxes(AABB box, vec3 color, mat4 view, mat4 proj) {
    bbox_DrawAABB(box, color, view, proj);

    // 3-axis gizmo at box center, length = max extent
    vec3 c = bbox_Center(box);
    vec3 e = bbox_Extent(box);
    float L = e[0];
    if (e[1] > L) L = e[1];
    if (e[2] > L) L = e[2];
    if (L <= 0.0f) L = 1.0f;

    drawDebugLine(c, vec3(c[0] + L, c[1], c[2]), vec3(1, 0, 0), view, proj);
    drawDebugLine(c, vec3(c[0], c[1] + L, c[2]), vec3(0, 1, 0), view, proj);
    drawDebugLine(c, vec3(c[0], c[1], c[2] + L), vec3(0, 0, 1), view, proj);
}

void bbox_DrawForNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node) return;
    if (node->type == ENTITY_MODEL) {
        AABB world = bbox_Transform(node->data.mesh.aabbLocal, node->world_matrix);
        bbox_DrawAABBWithAxes(world, vec3(0.2f, 1.0f, 0.4f), view, proj);
    } else if (node->type == ENTITY_LIGHT) {
        AABB world = bbox_FromLight(node->position, node->data.light.radius);
        bbox_DrawAABBWithAxes(world, vec3(1.0f, 0.85f, 0.2f), view, proj);
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->instanceCount > 0 && inst->instanceMeshes != nullptr) {
            // Get combined local mesh AABB
            AABB meshAABB = bbox_Empty();
            for (int m = 0; m < inst->instanceMeshCount; m++) {
                bbox_Combine(&meshAABB, &inst->instanceMeshes[m].aabbLocal);
            }
            // Draw AABB for each visible instance
            for (int i = 0; i < inst->visibleCount; i++) {
                int idx = inst->visibleIndices[i];
                AABB instLocal = bbox_Transform(meshAABB, inst->instanceMatrices[idx]);
                AABB world = bbox_Transform(instLocal, node->world_matrix);
                bbox_DrawAABBWithAxes(world, vec3(1.0f, 0.5f, 0.2f), view, proj);
            }
        }
    }
}

void bbox_DrawSceneGraph(SceneNode* root, mat4 view, mat4 proj) {
    if (!root) return;
    bbox_DrawForNode(root, view, proj);
    for (int i = 0; i < root->num_children; i++) {
        bbox_DrawSceneGraph(root->children[i], view, proj);
    }
}
