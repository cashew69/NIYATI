#pragma once
#include "engine/dependancies/vmath.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculates the signed distance to a sphere.
 */
float sdf_Sphere(vmath::vec3 p, float r);

/**
 * @brief Calculates UV coordinates for a point on a sphere surface.
 */
vmath::vec2 sdf_SphericalMapping(vmath::vec3 p);

/**
 * @brief Standard boolean union.
 */
float sdf_OpUnion(float d1, float d2);

/**
 * @brief Smooth union (interpolation) of two SDF distances.
 */
float sdf_OpSmoothUnion(float d1, float d2, float k);

#ifdef __cplusplus
}
#endif
