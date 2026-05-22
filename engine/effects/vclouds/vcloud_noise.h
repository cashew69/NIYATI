#ifndef VCLOUD_NOISE_H
#define VCLOUD_NOISE_H

#include "engine/dependancies/vmath.h"

/**
 * Standard Worley (cellular) noise.
 * Returns inverted distance (1 at center, 0 at edges).
 */
float vcVCloud_Worley(float x, float y, float z, int freq);

/**
 * Alligator Noise (Ridged Worley).
 * Useful for billowy cloud shapes.
 */
float vcVCloud_Alligator(float x, float y, float z, int freq);

/**
 * Curl Noise generator.
 * Returns a 3D vector representing the curl of a potential field.
 * Useful for wispy, turbulent distortion.
 */
vec3 vcVCloud_Curl(float x, float y, float z, float frequency, int octaves);

/**
 * Curly Alligator Noise.
 * Alligator noise distorted by a high-frequency curl field.
 * Excellent for high-detail cloud erosion.
 */
float vcVCloud_CurlyAlligator(float x, float y, float z, int freq, float curlAmount);

#endif // VCLOUD_NOISE_H
