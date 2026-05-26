#pragma once
#include <GL/glew.h>
struct SceneNode;

// Stamps footprints along a spline into overlayTex (must be a GL_RGBA8 texture).
// Texture dimensions are queried from the texture itself — any resolution works.
// The overlay is applied automatically in renderTerrain after this call.
// Call again to re-stamp; old footprints are cleared first.
void terrain_overlay_ApplySpline(GLuint overlayTex, SceneNode* splineNode);

// Returns the currently registered overlay texture (0 if none).
GLuint terrain_overlay_GetTexture();

// Detach the overlay — terrain renders without it.
void terrain_overlay_Detach();
