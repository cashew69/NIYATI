#ifndef PERLIN_H
#define PERLIN_H

typedef struct 
{
    float x;
    float z;
} vec2_perlin;

typedef struct 
{
    float x;
    float y;
    float z;
} vec3_perlin;

// Main Perlin noise function
// Returns noise value in range approximately [-1.0, 1.0]
float perlinNoise(float x, float z);
float perlinNoise3D(float x, float y, float z);

#endif // PERLIN_H
