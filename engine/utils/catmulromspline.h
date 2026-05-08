#pragma once
#include "engine/core/gl/structs.h"

// Initialize the spline data structure
void sg_InitCatmullRomNode(SceneNode* node);

// Evaluate a single point on a 4-point segment
vec3 sg_EvaluateCatmullRom(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t, float tension);

// Generate the full curve given control points
void sg_UpdateCatmullRomCurve(SceneNode* node);

// Draw the spline and optionally control points
void sg_RenderCatmullRomNode(SceneNode* node, mat4 view, mat4 proj);

// sg_GetSplinePoint (Absolute / Stateless)
// Best for Cinematics or exactly-timed events. 
// You manage the clock/time yourself and simply ask: "Where is the 
// position on this spline exactly at progress 't' (0.0 to 1.0)?"
// Example: sg_GetSplinePoint(spline, totalTime / 5.0f); // Finishes in 5s
vec3 sg_GetSplinePoint(SceneNode* splineNode, float progress);

// sg_AdvanceSpline (Relative / Stateful)
// Best for Gameplay and interactive movement.
// You pass a pointer to a variable that tracks the state, and a speed delta.
// It automatically updates your variable, handles clamping/looping based on  
// the Spline's editor settings, and returns the new position. 
// Example: sg_AdvanceSpline(spline, &myProgress, speed * deltaTime);
vec3 sg_AdvanceSpline(SceneNode* splineNode, float* progressInOut, float speedDelta);

// Automatically move any node (Model/Camera) along the spline using node->pathProgress
void sg_AnimateNodeAlongSpline(SceneNode* splineNode, SceneNode* targetNode, float speedDelta, float lookAhead = 0.05f);

// Free resources
void sg_FreeCatmullRomNode(SceneNode* node);
