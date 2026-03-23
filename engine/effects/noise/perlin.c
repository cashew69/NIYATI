#include "perlin.h"

#define PERLINGRID 50

// Global seed for noise generation
int g_perlinSeed = 12345;

// Helpers
int hash(int x, int z)
{
    int h = (x + g_perlinSeed) * 374761393 + (z + g_perlinSeed * 7) * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

int hash3D(int x, int y, int z)
{
    int h = (x + g_perlinSeed) * 374761393 + (y + g_perlinSeed * 3) * 668265263 + (z + g_perlinSeed * 7) * 1274126177;
    h = (h ^ (h >> 13)) * 1103515245;
    return h ^ (h >> 16);
}

const vec3_perlin GRADIENTS3D[12] = {
    { 1.0f,  1.0f,  0.0f}, {-1.0f,  1.0f,  0.0f}, { 1.0f, -1.0f,  0.0f}, {-1.0f, -1.0f,  0.0f},
    { 1.0f,  0.0f,  1.0f}, {-1.0f,  0.0f,  1.0f}, { 1.0f,  0.0f, -1.0f}, {-1.0f,  0.0f, -1.0f},
    { 0.0f,  1.0f,  1.0f}, { 0.0f, -1.0f,  1.0f}, { 0.0f,  1.0f, -1.0f}, { 0.0f, -1.0f, -1.0f}
};

vec3_perlin getGradient3D(int x, int y, int z)
{
    int h = hash3D(x, y, z);
    return GRADIENTS3D[h % 12];
}

float dotproduct3D(vec3_perlin a, vec3_perlin b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}


vec2_perlin getDistanceVector(vec2_perlin corner, vec2_perlin point)
{
    vec2_perlin distanceVec;
    distanceVec.x = point.x - corner.x;
    distanceVec.z = point.z - corner.z;
    return distanceVec;
}

float dotproduct(vec2_perlin a, vec2_perlin b)
{
    return (a.x * b.x) + (a.z * b.z);
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float smootherstep(float t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// Optimized after suggestion from AI, earlier i was doing if else. to get gradiant
const vec2_perlin DIRECTION_LUT[4] = {
    { 1.0f,  0.0f},
    {-1.0f,  0.0f},
    { 0.0f,  1.0f},
    { 0.0f, -1.0f}
};

vec2_perlin getGradient(int x, int z)
{
    int hashedValue = hash(x, z);

    int direction = hashedValue & 3;

    return DIRECTION_LUT[direction];
}

// Noise
float perlinNoise(float x, float z)
{
    // Step 1: Find which grid square contains this point
    int x0 = (int)floor(x);
    int x1 = x0 + 1;
    int z0 = (int)floor(z);
    int z1 = z0 + 1;

    // Step 2: Get gradients at INTEGER grid corners
    vec2_perlin grad00 = getGradient(x0, z0);
    vec2_perlin grad10 = getGradient(x1, z0);
    vec2_perlin grad01 = getGradient(x0, z1);
    vec2_perlin grad11 = getGradient(x1, z1);

    // Step 3: Calculate position within cell
    float dx = x - x0;
    float dz = z - z0;

    // Step 4: Calculate distance vectors from each corner to sample point
    vec2_perlin dist00;
    dist00.x = dx;
    dist00.z = dz;

    vec2_perlin dist10;
    dist10.x = dx - 1;
    dist10.z = dz;

    vec2_perlin dist01;
    dist01.x = dx;
    dist01.z = dz - 1;

    vec2_perlin dist11;
    dist11.x = dx - 1;
    dist11.z = dz - 1;

    // Step 5: Calculate dot products (influence from each corner)
    float dot00 = dotproduct(grad00, dist00);
    float dot10 = dotproduct(grad10, dist10);
    float dot01 = dotproduct(grad01, dist01);
    float dot11 = dotproduct(grad11, dist11);

    // Step 6: Apply smootherstep to interpolation weights
    float u = smootherstep(dx);
    float v = smootherstep(dz);

    // Step 7: Bilinear interpolation
    // Interpolate along bottom edge (between dot00 and dot10)
    float lerpBottom = lerp(dot00, dot10, u);

    // Interpolate along top edge (between dot01 and dot11)
    float lerpTop = lerp(dot01, dot11, u);

    // Interpolate vertically between bottom and top
    float result = lerp(lerpBottom, lerpTop, v);


    return result;
}

// 3D Noise
float perlinNoise3D(float x, float y, float z)
{
    // Step 1: Find which grid cube contains this point
    int x0 = (int)floor(x);
    int x1 = x0 + 1;
    int y0 = (int)floor(y);
    int y1 = y0 + 1;
    int z0 = (int)floor(z);
    int z1 = z0 + 1;

    // Step 2: Get gradients at INTEGER grid corners (8 corners for a cube)
    // Note: You will need a vec3_perlin struct and a getGradient3D function
    vec3_perlin grad000 = getGradient3D(x0, y0, z0);
    vec3_perlin grad100 = getGradient3D(x1, y0, z0);
    vec3_perlin grad010 = getGradient3D(x0, y1, z0);
    vec3_perlin grad110 = getGradient3D(x1, y1, z0);
    vec3_perlin grad001 = getGradient3D(x0, y0, z1);
    vec3_perlin grad101 = getGradient3D(x1, y0, z1);
    vec3_perlin grad011 = getGradient3D(x0, y1, z1);
    vec3_perlin grad111 = getGradient3D(x1, y1, z1);

    // Step 3: Calculate position within cell
    float dx = x - x0;
    float dy = y - y0;
    float dz = z - z0;

    // Step 4: Calculate distance vectors from each corner to sample point
    vec3_perlin dist000; dist000.x = dx;     dist000.y = dy;     dist000.z = dz;
    vec3_perlin dist100; dist100.x = dx - 1; dist100.y = dy;     dist100.z = dz;
    vec3_perlin dist010; dist010.x = dx;     dist010.y = dy - 1; dist010.z = dz;
    vec3_perlin dist110; dist110.x = dx - 1; dist110.y = dy - 1; dist110.z = dz;
    vec3_perlin dist001; dist001.x = dx;     dist001.y = dy;     dist001.z = dz - 1;
    vec3_perlin dist101; dist101.x = dx - 1; dist101.y = dy;     dist101.z = dz - 1;
    vec3_perlin dist011; dist011.x = dx;     dist011.y = dy - 1; dist011.z = dz - 1;
    vec3_perlin dist111; dist111.x = dx - 1; dist111.y = dy - 1; dist111.z = dz - 1;

    // Step 5: Calculate dot products (influence from each corner)
    // Note: You will need a dotproduct3D function
    float dot000 = dotproduct3D(grad000, dist000);
    float dot100 = dotproduct3D(grad100, dist100);
    float dot010 = dotproduct3D(grad010, dist010);
    float dot110 = dotproduct3D(grad110, dist110);
    float dot001 = dotproduct3D(grad001, dist001);
    float dot101 = dotproduct3D(grad101, dist101);
    float dot011 = dotproduct3D(grad011, dist011);
    float dot111 = dotproduct3D(grad111, dist111);

    // Step 6: Apply smootherstep to interpolation weights
    float u = smootherstep(dx);
    float v = smootherstep(dy);
    float w = smootherstep(dz);

    // Step 7: Trilinear interpolation
    // 7a. Interpolate along X for all 4 edges along the Z axis
    float lerpX00 = lerp(dot000, dot100, u); // Bottom-front edge
    float lerpX10 = lerp(dot010, dot110, u); // Top-front edge
    float lerpX01 = lerp(dot001, dot101, u); // Bottom-back edge
    float lerpX11 = lerp(dot011, dot111, u); // Top-back edge

    // 7b. Interpolate along Y between the X-interpolated values
    float lerpY0 = lerp(lerpX00, lerpX10, v); // Front face
    float lerpY1 = lerp(lerpX01, lerpX11, v); // Back face

    // 7c. Interpolate along Z between the Y-interpolated values
    float result = lerp(lerpY0, lerpY1, w);

    return result;
}
