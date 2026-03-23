#include "engine/effects/noise/perlin.h"

// Global noise settings (usually driven by UI or project config)
float g_turbulence = 0.0f;
int g_terraceLevels = 1;
float g_powerCurve = 1.0f;

// helper for Worley noise
static float random3Df(int x, int y, int z) {
    unsigned int h = (unsigned int)hash3D(x, y, z);
    return (float)(h & 0xFFFFFF) / 16777215.0f;
}
float fbm(float x, float z, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        total += perlinNoise(x * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / maxValue;
}

void applyTurbulence(float* x, float* z) {
    if (g_turbulence > 0.001f) {
        float warpX = fbm(*x * 0.5f, *z * 0.5f, 3, 0.5f, 2.0f) * g_turbulence;
        float warpZ = fbm(*x * 0.5f + 100.0f, *z * 0.5f + 100.0f, 3, 0.5f, 2.0f) * g_turbulence;
        *x += warpX;
        *z += warpZ;
    }
}

float applyTerracing(float height) {
    if (g_terraceLevels > 1) {
        float levels = (float)g_terraceLevels;
        height = floorf(height * levels) / levels;
    }
    return height;
}

float applyPowerCurve(float height) {
    if (g_powerCurve != 1.0f && height > 0.0f) {
        height = powf(height, g_powerCurve);
    }
    return height;
}


// 3D version of your fBm loop
float fbm3D(float x, float y, float z, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        // Swap perlinNoise3D with your engine's actual 3D noise function name
        total += perlinNoise3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// 3D Worley Noise (Cellular Noise)
float worley3D(float x, float y, float z) {
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);
    int zi = (int)floorf(z);

    float minDist = 1.0e10;

    // Check surrounding 3x3x3 grid cells
    for (int offZ = -1; offZ <= 1; offZ++) {
        for (int offY = -1; offY <= 1; offY++) {
            for (int offX = -1; offX <= 1; offX++) {
                int cx = xi + offX;
                int cy = yi + offY;
                int cz = zi + offZ;

                // Feature point in this cell
                float px = (float)cx + random3Df(cx, cy, cz);
                float py = (float)cy + random3Df(cx + 10, cy + 20, cz + 30);
                float pz = (float)cz + random3Df(cx + 40, cy + 50, cz + 60);

                float dx = px - x;
                float dy = py - y;
                float dz = pz - z;
                float d2 = dx*dx + dy*dy + dz*dz;

                if (d2 < minDist) {
                    minDist = d2;
                }
            }
        }
    }
    return sqrtf(minDist);
}

// 3D Worley fBm
float worleyFbm3D(float x, float y, float z, int octaves, float persistence, float lacunarity) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++) {
        // Inverted Worley (1.0 - dist) is often used for billowy clouds
        float w = 1.0f - worley3D(x * frequency, y * frequency, z * frequency);
        if (w < 0.0f) w = 0.0f;
        total += w * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// Optional 3D Turbulence if you want domain warping in your clouds
void applyTurbulence3D(float* x, float* y, float* z) {
    if (g_turbulence > 0.001f) {
        float warpX = fbm3D(*x * 0.5f, *y * 0.5f, *z * 0.5f, 3, 0.5f, 2.0f) * g_turbulence;
        float warpY = fbm3D(*x * 0.5f + 100.0f, *y * 0.5f + 100.0f, *z * 0.5f + 100.0f, 3, 0.5f, 2.0f) * g_turbulence;
        float warpZ = fbm3D(*x * 0.5f + 200.0f, *y * 0.5f + 200.0f, *z * 0.5f + 200.0f, 3, 0.5f, 2.0f) * g_turbulence;

        *x += warpX;
        *y += warpY;
        *z += warpZ;
    }
}

// Main sampling function for your raymarcher
float threeDNoise(float x, float y, float z) {
    // 1. Optional: Warp the sampling coordinates for billowy, organic edges
    // applyTurbulence3D(&x, &y, &z);

    // 2. Generate the base 3D cloud structure using Worley Noise
    // Parameters: pos.x, pos.y, pos.z, octaves, persistence, lacunarity
    float density = worleyFbm3D(x, y, z, 3, 0.5f, 2.0f);

    // 3. Shape the clouds using your existing power curve
    // Higher power curve values (e.g., 2.0 - 4.0) create fluffier clouds
    // by eroding the low-density noise into empty sky.
    density = applyPowerCurve(density);

    // Note: applyTerracing() is generally skipped for volumetric clouds
    // as it creates unnatural flat layers, unless you are building a specific stylized effect.

    // Clamp to ensure we don't get negative density during raymarching
    if (density < 0.0f) {
        return 0.0f;
    }

    return density;
}
