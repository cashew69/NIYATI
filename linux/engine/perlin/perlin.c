#include "perlin.h"

#define PERLINGRID 50

// Helpers
int hash(int x, int z)
{
    int h = x * 374761393 + z * 668265263;  
    h = (h ^ (h >> 13)) * 1274126177;      
    return h ^ (h >> 16);                 
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

vec2_perlin getGradient(int x, int z) 
{
    int hashedValue = hash(x, z);
    
    int direction = hashedValue & 3;  // Gets value 0, 1, 2, or 3
    
    vec2_perlin gradient;
    
    if (direction == 0) {
        gradient.x = 1.0;
        gradient.z = 0.0;
    }
    else if (direction == 1) {
        gradient.x = -1.0;
        gradient.z = 0.0;
    }
    else if (direction == 2) {
        gradient.x = 0.0;
        gradient.z = 1.0;
    }
    else { 
        gradient.x = 0.0;
        gradient.z = -1.0;
    }
    
    return gradient;
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

    printf("%f\n", result);
    
    return result;
}


