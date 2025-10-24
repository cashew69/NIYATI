
#define PERLINGRID 50

// Helpers
hash
vec2 getDistanceVector(vec2 corner, vec2 point)
{
    vec2 distanceVec;
    distanceVec.x = point.x - corner.x;
    distanceVec.z = point.z - corner.z;
    return distanceVec;
}

float dotproduct(vec2 a, vec2 b)
{
    return (a.x * b.x) + (a.z * b.z) 
}

float lerp(float a, float b, float t)  // t is a single float from 0 to 1
{
    return a + (b - a) * t;
}


// Noise
perlinNoise(int x, int z)
{}



