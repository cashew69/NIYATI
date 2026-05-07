#pragma once
#include "engine/core/gl/structs.h"
#include "engine/utils/boundingbox.h"
#include "engine/utils/BVH.h"

// ============================================================================
// Culling — frustum extraction + AABB-vs-frustum tests, BVH integration.
//
// Forward+ rendering: cull lights against the camera frustum once per frame
// using the BVH; then bin them on the GPU into screen tiles.  This module
// gives us the camera-space culling stage.
// ============================================================================

// Globals ------------------------------------------------------------------
// Master toggle.  Default OFF; project's projectInit() can flip it on, or the
// editor's UtilBar exposes a button that toggles it at runtime.
extern bool g_FrustumCullingEnabled;

// Master BVH toggle.  Default ON for all loaded models — the BVH is rebuilt
// every frame.  Lives here so the editor can show a status indicator.
extern bool g_BVHEnabled;

// Most recent built scene BVH (rebuilt by RenderSceneModels each frame).
extern BVH  g_SceneBVH;

// Frustum extracted from the most recent (view, proj) pair used for rendering.
extern cullfrustum g_RenderFrustum;

// Stats — overwritten on every frame.
extern int  g_CulledModelCount;
extern int  g_VisibleModelCount;
extern int  g_VisibleLightCount;

// API ----------------------------------------------------------------------

// Public toggle helpers (callable from project or editor).
void cull_SetFrustumCulling(bool enabled);
bool cull_IsFrustumCullingEnabled(void);

// Build a 6-plane frustum from a view-projection matrix.
void cull_ExtractFrustum(cullfrustum* out, const mat4& viewProj);

// Box vs. frustum.  Returns true if the box is at least partially inside
// (i.e. should be drawn).
bool cull_TestAABB(const cullfrustum* f, AABB box);

// Create a version of the frustum transformed into the local space of a node.
// worldToLocal should be the inverse of the node's world_matrix.
void cull_TransformFrustum(cullfrustum* out, const cullfrustum* in, const mat4& worldToLocal);
