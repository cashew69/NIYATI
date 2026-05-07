#include "BVH.h"
#include <stdlib.h>
#include <string.h>

// Forward decl — implemented in culling.cpp.
extern bool cull_TestAABB(const cullfrustum* f, AABB box);

// --- Internal helpers -----------------------------------------------------

static int bvh_AllocNode(BVH* bvh) {
    if (bvh->nodeCount >= bvh->nodeCapacity) {
        int newCap = bvh->nodeCapacity == 0 ? 16 : bvh->nodeCapacity * 2;
        bvh->nodes = (BVHNode*)realloc(bvh->nodes, newCap * sizeof(BVHNode));
        bvh->nodeCapacity = newCap;
    }
    int idx = bvh->nodeCount++;
    BVHNode* n = &bvh->nodes[idx];
    n->bounds    = bbox_Empty();
    n->left      = -1;
    n->right     = -1;
    n->firstItem = -1;
    n->itemCount = 0;
    return idx;
}

static AABB bvh_BoundsOfRange(const BVH* bvh, int first, int count) {
    AABB b = bbox_Empty();
    for (int i = 0; i < count; i++) {
        bbox_Combine(&b, &bvh->items[first + i].bounds);
    }
    return b;
}

// In-place partition by chosen axis around the midpoint of the centroid
// AABB.  Returns the count that ended up on the left side.
static int bvh_Partition(BVH* bvh, int first, int count, int axis, float mid) {
    int left = first;
    int right = first + count - 1;
    while (left <= right) {
        vec3 c = bbox_Center(bvh->items[left].bounds);
        if (c[axis] < mid) {
            left++;
        } else {
            BVHItem tmp = bvh->items[left];
            bvh->items[left] = bvh->items[right];
            bvh->items[right] = tmp;
            right--;
        }
    }
    int leftCount = left - first;
    return leftCount;
}

#define BVH_LEAF_THRESHOLD 4

static int bvh_BuildRec(BVH* bvh, int first, int count) {
    int idx = bvh_AllocNode(bvh);
    AABB nodeBounds = bvh_BoundsOfRange(bvh, first, count);
    bvh->nodes[idx].bounds = nodeBounds;

    if (count <= BVH_LEAF_THRESHOLD) {
        bvh->nodes[idx].firstItem = first;
        bvh->nodes[idx].itemCount = count;
        bvh->nodes[idx].left  = -1;
        bvh->nodes[idx].right = -1;
        return idx;
    }

    // Build a centroid AABB to choose split axis (longest extent).
    AABB centroidB = bbox_Empty();
    for (int i = 0; i < count; i++) {
        bbox_Expand(&centroidB, bbox_Center(bvh->items[first + i].bounds));
    }
    vec3 cExt = vec3(centroidB.max[0] - centroidB.min[0],
                     centroidB.max[1] - centroidB.min[1],
                     centroidB.max[2] - centroidB.min[2]);
    int axis = 0;
    if (cExt[1] > cExt[0]) axis = 1;
    if (cExt[2] > cExt[axis]) axis = 2;

    float mid = (centroidB.min[axis] + centroidB.max[axis]) * 0.5f;
    int leftCount = bvh_Partition(bvh, first, count, axis, mid);

    // Degenerate split — fall back to even halves.
    if (leftCount == 0 || leftCount == count) {
        leftCount = count / 2;
    }

    int leftChild  = bvh_BuildRec(bvh, first, leftCount);
    int rightChild = bvh_BuildRec(bvh, first + leftCount, count - leftCount);

    // Re-fetch — bvh->nodes may have been reallocated during recursion.
    bvh->nodes[idx].left  = leftChild;
    bvh->nodes[idx].right = rightChild;
    bvh->nodes[idx].firstItem = -1;
    bvh->nodes[idx].itemCount = 0;
    return idx;
}

// --- Public API -----------------------------------------------------------

void bvh_Init(BVH* bvh) {
    if (!bvh) return;
    memset(bvh, 0, sizeof(*bvh));
    bvh->rootIndex = -1;
}

void bvh_Free(BVH* bvh) {
    if (!bvh) return;
    free(bvh->items);
    free(bvh->nodes);
    memset(bvh, 0, sizeof(*bvh));
    bvh->rootIndex = -1;
}

void bvh_Clear(BVH* bvh) {
    if (!bvh) return;
    bvh->itemCount = 0;
    bvh->nodeCount = 0;
    bvh->rootIndex = -1;
}

void bvh_AddItem(BVH* bvh, BVHItem item) {
    if (!bvh) return;
    if (bvh->itemCount >= bvh->itemCapacity) {
        int newCap = bvh->itemCapacity == 0 ? 16 : bvh->itemCapacity * 2;
        bvh->items = (BVHItem*)realloc(bvh->items, newCap * sizeof(BVHItem));
        bvh->itemCapacity = newCap;
    }
    bvh->items[bvh->itemCount++] = item;
}

void bvh_Build(BVH* bvh) {
    if (!bvh) return;
    bvh->nodeCount = 0;
    bvh->rootIndex = -1;
    if (bvh->itemCount <= 0) return;
    bvh->rootIndex = bvh_BuildRec(bvh, 0, bvh->itemCount);
}

// Walk the scene graph adding every model/light as a BVH item.
static void bvh_CollectFromNode(BVH* bvh, SceneNode* node) {
    if (!node) return;

    if (node->type == ENTITY_MODEL) {
        BVHItem it;
        it.bounds   = bbox_Transform(node->data.mesh.aabbLocal, node->world_matrix);
        it.type     = BVH_ITEM_MODEL;
        it.userData = node;
        bvh_AddItem(bvh, it);
    } else if (node->type == ENTITY_LIGHT) {
        BVHItem it;
        vec3 worldPos = vec3(node->world_matrix[3][0], node->world_matrix[3][1], node->world_matrix[3][2]);
        it.bounds   = bbox_FromLight(worldPos, node->data.light.radius);
        it.type     = BVH_ITEM_LIGHT;
        it.userData = node;
        bvh_AddItem(bvh, it);
    } else if (node->type == ENTITY_INSTANCE) {
        InstanceData* inst = &node->data.instance;
        if (inst->instanceCount > 0 && inst->instanceMeshes != nullptr) {
            BVHItem it;
            // Add a single item for the entire instance group in the global BVH.
            // Fine-grained culling will be performed during the traversal or render pass.
            it.bounds = bbox_Transform(inst->clusterAABB, node->world_matrix);
            it.type = BVH_ITEM_INSTANCE;
            it.userData = node;
            it.instanceIndex = -1; // -1 marks this as a group item
            bvh_AddItem(bvh, it);
        }
    } else if (node->type == ENTITY_TERRAIN) {
        if (node->data.terrain.mesh) {
            BVHItem it;
            it.bounds = bbox_Transform(node->data.terrain.mesh->aabbLocal, node->world_matrix);
            it.type = BVH_ITEM_TERRAIN;
            it.userData = node;
            bvh_AddItem(bvh, it);
        }
    }
    // ENTITY_SKYBOX is not added — skybox always renders regardless of frustum.

    for (int i = 0; i < node->num_children; i++) {
        bvh_CollectFromNode(bvh, node->children[i]);
    }
}

void bvh_BuildFromScene(BVH* bvh, SceneNode* root) {
    if (!bvh) return;
    bvh_Clear(bvh);
    if (!root) return;
    bvh_CollectFromNode(bvh, root);
    bvh_Build(bvh);
}

// --- Queries --------------------------------------------------------------

static void bvh_QueryRec(const BVH* bvh, int nodeIdx, const cullfrustum* f,
                         int* outIndices, int maxOut, int* outCount) {
    if (nodeIdx < 0 || *outCount >= maxOut) return;
    const BVHNode* n = &bvh->nodes[nodeIdx];
    if (!cull_TestAABB(f, n->bounds)) return;

    if (n->itemCount > 0) {
        for (int i = 0; i < n->itemCount; i++) {
            if (*outCount >= maxOut) return;
            int itemIdx = n->firstItem + i;
            if (cull_TestAABB(f, bvh->items[itemIdx].bounds)) {
                outIndices[(*outCount)++] = itemIdx;
            }
        }
        return;
    }

    bvh_QueryRec(bvh, n->left,  f, outIndices, maxOut, outCount);
    bvh_QueryRec(bvh, n->right, f, outIndices, maxOut, outCount);
}

void bvh_QueryFrustum(const BVH* bvh, const cullfrustum* frustum,
                      int* outIndices, int maxOut, int* outCount) {
    if (!bvh || !frustum || !outCount) return;
    *outCount = 0;
    if (bvh->rootIndex < 0 || !outIndices || maxOut <= 0) return;
    bvh_QueryRec(bvh, bvh->rootIndex, frustum, outIndices, maxOut, outCount);
}

static void bvh_VisitRec(const BVH* bvh, int nodeIdx, const cullfrustum* f,
                         BVHVisitFn cb, void* userPtr) {
    if (nodeIdx < 0) return;
    const BVHNode* n = &bvh->nodes[nodeIdx];
    if (!cull_TestAABB(f, n->bounds)) return;
    if (n->itemCount > 0) {
        for (int i = 0; i < n->itemCount; i++) {
            const BVHItem* it = &bvh->items[n->firstItem + i];
            if (cull_TestAABB(f, it->bounds) && cb) cb(it, userPtr);
        }
        return;
    }
    bvh_VisitRec(bvh, n->left,  f, cb, userPtr);
    bvh_VisitRec(bvh, n->right, f, cb, userPtr);
}

void bvh_VisitFrustum(const BVH* bvh, const cullfrustum* frustum,
                      BVHVisitFn cb, void* userPtr) {
    if (!bvh || !frustum) return;
    if (bvh->rootIndex < 0) return;
    bvh_VisitRec(bvh, bvh->rootIndex, frustum, cb, userPtr);
}

// --- Debug draw -----------------------------------------------------------

void bvh_DebugDraw(const BVH* bvh, mat4 view, mat4 proj) {
    if (!bvh || bvh->rootIndex < 0) return;
    for (int i = 0; i < bvh->nodeCount; i++) {
        const BVHNode* n = &bvh->nodes[i];
        // Internal nodes drawn dim cyan, leaves drawn magenta.
        vec3 col = n->itemCount > 0 ? vec3(1.0f, 0.2f, 1.0f) : vec3(0.2f, 0.6f, 0.8f);
        bbox_DrawAABB(n->bounds, col, view, proj);
    }
}
