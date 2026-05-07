#pragma once
#include "engine/core/gl/structs.h"
#include "engine/utils/boundingbox.h"

// ============================================================================
// BVH — Bounding Volume Hierarchy
// ============================================================================

typedef enum {
    BVH_ITEM_GENERIC = 0,
    BVH_ITEM_MODEL   = 1,   // SceneNode of type ENTITY_MODEL
    BVH_ITEM_LIGHT   = 2,   // SceneNode of type ENTITY_LIGHT
    BVH_ITEM_INSTANCE= 3,   // SceneNode of type ENTITY_INSTANCE
    BVH_ITEM_TERRAIN = 4    // SceneNode of type ENTITY_TERRAIN
    // ENTITY_SKYBOX is intentionally excluded — skybox always renders, not culled
} BVHItemType;

typedef struct BVHItem {
    AABB         bounds;     // world-space AABB
    BVHItemType  type;
    void*        userData;   // SceneNode* for scene-built BVHs
    int          instanceIndex; // Index of the specific instance matrix
} BVHItem;

typedef struct BVHNode {
    AABB bounds;
    int  left;        // child index, -1 if leaf
    int  right;       // child index, -1 if leaf
    int  firstItem;   // index into items[] (leaves only)
    int  itemCount;   // 0 for internal nodes
} BVHNode;

typedef struct BVH {
    BVHItem*  items;
    int       itemCount;
    int       itemCapacity;

    BVHNode*  nodes;
    int       nodeCount;
    int       nodeCapacity;

    int       rootIndex;
} BVH;

// --- Construction ---------------------------------------------------------
void  bvh_Init(BVH* bvh);
void  bvh_Free(BVH* bvh);
void  bvh_Clear(BVH* bvh);                          // keeps capacity, drops contents
void  bvh_AddItem(BVH* bvh, BVHItem item);          // grow items[]
void  bvh_Build(BVH* bvh);                          // build tree from current items
void  bvh_BuildFromScene(BVH* bvh, SceneNode* root); // collect scene + build

// --- Queries --------------------------------------------------------------
// Frustum-cull traversal — appends matching item indices into outIndices.
// `maxOut` is array capacity, `outCount` is updated in place.
// Items with no SceneNode/userData are still included by index.
void  bvh_QueryFrustum(const BVH* bvh, const cullfrustum* frustum,
                       int* outIndices, int maxOut, int* outCount);

// Convenience: walk the BVH and call cb for every item that passes the
// frustum test.  cb may be NULL.
typedef void (*BVHVisitFn)(const BVHItem* item, void* userPtr);
void  bvh_VisitFrustum(const BVH* bvh, const cullfrustum* frustum,
                       BVHVisitFn cb, void* userPtr);

// --- Debug ----------------------------------------------------------------
// Draw every internal+leaf node AABB as wireframe (uses line shader).
void  bvh_DebugDraw(const BVH* bvh, mat4 view, mat4 proj);
