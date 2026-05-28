#include "engine/effects/sdf.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float sdf_Sphere(vmath::vec3 p, float r) {
    return vmath::length(p) - r;
}

vmath::vec2 sdf_SphericalMapping(vmath::vec3 p) {
    vmath::vec3 n = vmath::normalize(p);
    float u = 0.5f + (atan2(n[2], n[0])) / (2.0f * (float)M_PI);
    float v = 0.5f - (asin(n[1])) / (float)M_PI;
    return vmath::vec2(u, v);
}

float sdf_OpUnion(float d1, float d2) {
    return vmath::min(d1, d2);
}

float sdf_OpSmoothUnion(float d1, float d2, float k) {
    float h = vmath::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return vmath::mix(d2, d1, h) - k * h * (1.0f - h);
}
