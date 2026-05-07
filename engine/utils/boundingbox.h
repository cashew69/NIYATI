#pragma once
#include "engine/core/gl/structs.h"

// ============================================================================
// AABB / Bounding Box Utilities  (C-style API)
//
// Local-space AABBs are computed at mesh-load time and stored in Mesh.aabbLocal.
// World-space AABBs are derived on the fly from a node's world matrix or, for
// lights, from position + radius.
//
// The visualizer draws the 12 edges of a box plus a 3-axis gizmo at its center
// using the existing line shader.  The same routine works for any AABB so it
// can be reused for anything new added to the engine later.
// ============================================================================

// --- Construction ---------------------------------------------------------
AABB bbox_Empty(void);                                  // returns inverted box
void bbox_Expand(AABB* box, vec3 p);                    // grow to include point
void bbox_Combine(AABB* dst, const AABB* other);        // union
AABB bbox_FromPoints(const float* xyz, int vertexCount);

// Transform a local-space AABB by a world matrix into a world-space AABB.
AABB bbox_Transform(AABB local, const mat4& world);

// Light source AABB: cube of radius around position.
AABB bbox_FromLight(vec3 position, float radius);

// Helpers
vec3 bbox_Center(AABB box);
vec3 bbox_Extent(AABB box);          // half-size
float bbox_SurfaceArea(AABB box);
bool  bbox_IsEmpty(AABB box);

// --- Visualizer -----------------------------------------------------------
// Globally toggled visualization (drawn at end of scene render).
extern bool g_DrawBoundingBoxes;

// One-shot draw routines — usable for any new entity that exposes an AABB.
// `drawAxes` adds an XYZ gizmo (red/green/blue) at the box center.
void bbox_DrawAABB(AABB box, vec3 color, mat4 view, mat4 proj);
void bbox_DrawAABBWithAxes(AABB box, vec3 color, mat4 view, mat4 proj);

// Draw bounding box for a scene node (model uses transformed local AABB,
// light uses radius-based AABB; other types are skipped).
void bbox_DrawForNode(SceneNode* node, mat4 view, mat4 proj);

// Walk the scene graph and draw a bbox for every model + light.
void bbox_DrawSceneGraph(SceneNode* root, mat4 view, mat4 proj);
