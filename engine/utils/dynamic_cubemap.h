#ifndef DYNAMIC_CUBEMAP_H
#define DYNAMIC_CUBEMAP_H

#include <GL/glew.h>
#include "engine/dependancies/vmath.h"
#include "engine/core/gl/structs.h"

/**
 * Renders the entire scene into a cubemap from the specified position.
 * This cubemap can then be used for PBR IBL (Image-Based Lighting).
 *
 * @param root The root of the scene graph to render.
 * @param center The world-space position to render from.
 * @param resolution The resolution of each cubemap face (e.g., 128).
 * @return The OpenGL texture handle for the generated cubemap.
 */
void sg_UpdateDynamicCubemap(SceneNode* root, vec3 center, int resolution);

#endif // DYNAMIC_CUBEMAP_H
