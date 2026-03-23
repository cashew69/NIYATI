#include "terrain.h"
#include "../engine/effects/noise/perlin.h"
#include "../engine/effects/noise/noise.c"
#include <GL/gl.h>
#include <cmath>
#include <cstdlib>

// ============================================================================
// TERRAIN GENERATION PARAMETERS
// ============================================================================

int g_terrainOctaves = 12;
float g_terrainPersistence = 0.5f;
float g_terrainLacunarity = 2.0f;

float g_mountainThreshold = 0.3f;
float g_desertThreshold = -0.2f;
float g_mountainHeightScale = 50.0f;
float g_desertHeightScale = 600.0f;
float g_plainsHeightScale = 20.0f;
bool g_useIslandMask = false;
int g_biomeMode = 2;

float g_ridgeStrength = 1.0f;
float g_heightOffset = 0.0f;
float g_islandFalloff = 2.0f;

// ============================================================================
// TERRAIN MESH PARAMETERS
// ============================================================================

int g_terrainMeshWidth = 256;
int g_terrainMeshDepth = 256;
float g_terrainScale = 10.0f;
int g_tessellationInner = 4;
int g_tessellationOuter = 4;
float g_displacementScale = 20.0f;
bool g_wireframeMode = false;
int g_lodBias = 0;
int g_heightmapSource = 0;
GLuint g_loadedHeightmapTexture = 0;
char g_heightmapFilePath[256] = "";
GLuint g_terrainDisplacementMap = 0;

// ============================================================================
// TERRAIN PBR SETTINGS
// ============================================================================

bool g_enableTerrainDiffuse = true;
bool g_enableTerrainNormalMap = true;
bool g_enableTerrainARM = true;
bool g_enableTerrainDisplacement = true;
float g_terrainUVScale = 100.0f;
int g_terrainMaterialIndex = 0;

struct TerrainMaterialDef {
    const char* name;
    const char* diffusePath;
    const char* normalPath;
    const char* armPath;
    const char* dispPath;
};

TerrainMaterialDef g_terrainMaterials[] = {
    {"Aerial Beach", "user/models/moon_assets/aerial_beach_01_diff_1k.png", "user/models/moon_assets/aerial_beach_01_nor_gl_1k.png", "user/models/moon_assets/aerial_beach_01_arm_1k.png", "user/models/moon_assets/aerial_beach_01_disp_1k.png"},
    {"Gray Rocks", "user/models/khadbad/gray_rocks_diff_1k.png", "user/models/khadbad/gray_rocks_nor_gl_1k.png", "user/models/khadbad/gray_rocks_arm_1k.png", "user/models/khadbad/gray_rocks_disp_1k.png"},
};
int g_terrainMaterialCount = sizeof(g_terrainMaterials) / sizeof(g_terrainMaterials[0]);

#define PLANE_WIDTH 256
#define PLANE_DEPTH 256

// ============================================================================
// BIOME HEIGHT FUNCTIONS
// ============================================================================

float getMountainHeight(float x, float z) {
    applyTurbulence(&x, &z);
    float n = fbm(x, z, g_terrainOctaves, g_terrainPersistence, g_terrainLacunarity);
    float smooth = (n * 0.5f + 0.5f);
    float ridged = 1.0f - fabsf(n);
    ridged = ridged * ridged * ridged;
    return smooth * (1.0f - g_ridgeStrength) + ridged * g_ridgeStrength;
}

float getDesertHeight(float x, float z) {
    applyTurbulence(&x, &z);
    float n = fbm(x * 0.5f, z * 2.0f, 4, 0.3f, g_terrainLacunarity);
    return (n * 0.5f + 0.5f) * 0.2f;
}

float getPlainsHeight(float x, float z) {
    applyTurbulence(&x, &z);
    float n = fbm(x, z, 4, g_terrainPersistence, g_terrainLacunarity);
    return (n * 0.5f + 0.5f);
}

float applyIslandMask(float height, int x, int z, int mapWidth, int mapDepth) {
    float centerX = mapWidth / 2.0f;
    float centerZ = mapDepth / 2.0f;
    float dx = (x - centerX) / centerX;
    float dz = (z - centerZ) / centerZ;
    float distance = sqrtf(dx * dx + dz * dz);
    float mask = 1.0f - distance;
    if (mask < 0.0f) mask = 0.0f;
    mask = powf(mask, g_islandFalloff);
    return height * mask;
}

float getTerrainHeight(float nx, float nz, int x, int z, int width, int depth) {
    float finalHeight = 0.0f;

    if (g_biomeMode == 0) {
        float biomeSelector = perlinNoise(nx * 0.1f, nz * 0.1f);
        if (biomeSelector > g_mountainThreshold) {
            finalHeight = getMountainHeight(nx, nz) * g_mountainHeightScale;
        } else if (biomeSelector < g_desertThreshold) {
            finalHeight = getDesertHeight(nx, nz) * g_desertHeightScale;
        } else {
            finalHeight = getPlainsHeight(nx, nz) * g_plainsHeightScale;
        }
    } else if (g_biomeMode == 1) {
        finalHeight = getMountainHeight(nx, nz) * g_mountainHeightScale;
    } else if (g_biomeMode == 2) {
        finalHeight = getDesertHeight(nx, nz) * g_desertHeightScale;
    } else {
        finalHeight = getPlainsHeight(nx, nz) * g_plainsHeightScale;
    }

    finalHeight = applyPowerCurve(finalHeight);
    finalHeight = applyTerracing(finalHeight);
    finalHeight += g_heightOffset;

    if (g_useIslandMask) {
        finalHeight = applyIslandMask(finalHeight, x, z, width, depth);
    }
    return finalHeight;
}

// ============================================================================
// UNIFIED TERRAIN DATA GENERATION
// ============================================================================

struct TerrainData {
    unsigned char* heightPixels;
    unsigned char* normalPixels;
    int width;
    int depth;
};

TerrainData generateTerrainData(int width, int depth, float frequency) {
    TerrainData result = {0};
    result.width = width;
    result.depth = depth;

    float* rawHeights = (float*)malloc(width * depth * sizeof(float));
    if (!rawHeights) return result;

    float minH = 999999.0f, maxH = -999999.0f;
    for (int z = 0; z < depth; z++) {
        for (int x = 0; x < width; x++) {
            float h = getTerrainHeight(x * frequency, z * frequency, x, z, width, depth);
            rawHeights[z * width + x] = h;
            if (h < minH) minH = h;
            if (h > maxH) maxH = h;
        }
    }

    float range = maxH - minH;
    if (range < 0.001f) range = 1.0f;

    result.heightPixels = (unsigned char*)malloc(width * depth * 4);
    result.normalPixels = (unsigned char*)malloc(width * depth * 4);

    if (!result.heightPixels || !result.normalPixels) {
        free(rawHeights);
        free(result.heightPixels);
        free(result.normalPixels);
        result.heightPixels = NULL;
        result.normalPixels = NULL;
        return result;
    }

    for (int z = 0; z < depth; z++) {
        for (int x = 0; x < width; x++) {
            int idx = z * width + x;
            int pixelOffset = idx * 4;

            float normalized = (rawHeights[idx] - minH) / range;
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;
            unsigned char byteVal = (unsigned char)(normalized * 255.0f);
            result.heightPixels[pixelOffset + 0] = byteVal;
            result.heightPixels[pixelOffset + 1] = byteVal;
            result.heightPixels[pixelOffset + 2] = byteVal;
            result.heightPixels[pixelOffset + 3] = 255;

            float heightL = (x > 0) ? rawHeights[idx - 1] : rawHeights[idx];
            float heightR = (x < width - 1) ? rawHeights[idx + 1] : rawHeights[idx];
            float heightD = (z > 0) ? rawHeights[idx - width] : rawHeights[idx];
            float heightU = (z < depth - 1) ? rawHeights[idx + width] : rawHeights[idx];

            float nx = heightL - heightR;
            float ny = 2.0f;
            float nz = heightD - heightU;
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) { nx /= length; ny /= length; nz /= length; }
            else { nx = 0.0f; ny = 1.0f; nz = 0.0f; }

            result.normalPixels[pixelOffset + 0] = (unsigned char)((nx * 0.5f + 0.5f) * 255.0f);
            result.normalPixels[pixelOffset + 1] = (unsigned char)((ny * 0.5f + 0.5f) * 255.0f);
            result.normalPixels[pixelOffset + 2] = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f);
            result.normalPixels[pixelOffset + 3] = 255;
        }
    }

    free(rawHeights);
    return result;
}

void freeTerrainData(TerrainData* data) {
    free(data->heightPixels);
    free(data->normalPixels);
    data->heightPixels = NULL;
    data->normalPixels = NULL;
}

// ============================================================================
// TEXTURE CREATION FROM TERRAIN DATA
// ============================================================================

GLuint createTextureFromPixels(unsigned char* pixels, int width, int height) {
    if (!pixels) return 0;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return textureID;
}

GLuint createHeightMapTexture(int width, int depth, float frequency, float heightScale) {
    TerrainData data = generateTerrainData(width, depth, frequency);
    GLuint tex = createTextureFromPixels(data.heightPixels, width, depth);
    freeTerrainData(&data);
    return tex;
}

GLuint createNormalMapTexture(int width, int depth, float frequency, float heightScale) {
    TerrainData data = generateTerrainData(width, depth, frequency);
    GLuint tex = createTextureFromPixels(data.normalPixels, width, depth);
    freeTerrainData(&data);
    return tex;
}

// ============================================================================
// PNG EXPORT
// ============================================================================

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../engine/dependancies/stb_image_write.h"

bool exportHeightmapToPNG(const char* filename, int width, int depth, float frequency) {
    TerrainData data = generateTerrainData(width, depth, frequency);
    if (!data.heightPixels) return false;
    int result = stbi_write_png(filename, width, depth, 4, data.heightPixels, width * 4);
    freeTerrainData(&data);
    return result != 0;
}

bool exportNormalmapToPNG(const char* filename, int width, int depth, float frequency) {
    TerrainData data = generateTerrainData(width, depth, frequency);
    if (!data.normalPixels) return false;
    int result = stbi_write_png(filename, width, depth, 4, data.normalPixels, width * 4);
    freeTerrainData(&data);
    return result != 0;
}

// ============================================================================
// TERRAIN MESH CREATION
// ============================================================================

Mesh* createTerrainMesh() {
    fprintf(gpFile, "Generating terrain mesh %dx%d...\n", PLANE_WIDTH, PLANE_DEPTH);

    int vertexCount = PLANE_WIDTH * PLANE_DEPTH;
    int patchCount = (PLANE_WIDTH - 1) * (PLANE_DEPTH - 1);
    int indexCount = patchCount * 4;

    float* positions = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* normals = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* texCoords = (float*)malloc(vertexCount * 2 * sizeof(float));
    unsigned int* indices = (unsigned int*)malloc(indexCount * sizeof(unsigned int));

    if (!positions || !normals || !texCoords || !indices) {
        fprintf(gpFile, "Error: Failed to allocate terrain mesh data\n");
        free(positions);
        free(normals);
        free(texCoords);
        free(indices);
        return NULL;
    }

    float spacing = 10.0f;
    float halfWidth = (PLANE_WIDTH * spacing) / 2.0f;
    float halfDepth = (PLANE_DEPTH * spacing) / 2.0f;

    int idx = 0;
    for (int z = 0; z < PLANE_DEPTH; z++) {
        for (int x = 0; x < PLANE_WIDTH; x++) {
            positions[idx * 3 + 0] = (float)x * spacing - halfWidth;
            positions[idx * 3 + 1] = 0.0f;
            positions[idx * 3 + 2] = (float)z * spacing - halfDepth;

            normals[idx * 3 + 0] = 0.0f;
            normals[idx * 3 + 1] = 1.0f;
            normals[idx * 3 + 2] = 0.0f;

            texCoords[idx * 2 + 0] = (float)x / (float)(PLANE_WIDTH - 1);
            texCoords[idx * 2 + 1] = (float)z / (float)(PLANE_DEPTH - 1);

            idx++;
        }
    }

    idx = 0;
    for (int z = 0; z < PLANE_DEPTH - 1; z++) {
        for (int x = 0; x < PLANE_WIDTH - 1; x++) {
            int bottomLeft = z * PLANE_WIDTH + x;
            int bottomRight = bottomLeft + 1;
            int topLeft = (z + 1) * PLANE_WIDTH + x;
            int topRight = topLeft + 1;

            indices[idx++] = bottomLeft;
            indices[idx++] = bottomRight;
            indices[idx++] = topLeft;
            indices[idx++] = topRight;
        }
    }

    ModelVertexData vertexData = {0};
    vertexData.positions = positions;
    vertexData.normals = normals;
    vertexData.colors = NULL;
    vertexData.texCoords = texCoords;
    vertexData.indices = indices;
    vertexData.vertexCount = vertexCount;
    vertexData.indexCount = indexCount;

    Material material = {0};
    material.diffuseColor[0] = 0.8f;
    material.diffuseColor[1] = 0.8f;
    material.diffuseColor[2] = 0.8f;
    material.specularColor[0] = 0.2f;
    material.specularColor[1] = 0.2f;
    material.specularColor[2] = 0.2f;
    material.shininess = 32.0f;
    material.opacity = 1.0f;
    material.isEmissive = false;

    TerrainMaterialDef* matDef = &g_terrainMaterials[g_terrainMaterialIndex];

    loadPNGTexture(&material.diffuseTexture, const_cast<char*>(matDef->diffusePath), 1);
    fprintf(gpFile, "  Loaded diffuse texture: %u\n", material.diffuseTexture);

    loadPNGTexture(&material.normalTexture, const_cast<char*>(matDef->normalPath), 1);
    fprintf(gpFile, "  Loaded normal texture: %u\n", material.normalTexture);

    loadPNGTexture(&material.metallicRoughnessTexture, const_cast<char*>(matDef->armPath), 1);
    fprintf(gpFile, "  Loaded ARM texture: %u\n", material.metallicRoughnessTexture);

    loadPNGTexture(&g_terrainDisplacementMap, const_cast<char*>(matDef->dispPath), 1);
    fprintf(gpFile, "  Loaded displacement texture: %u\n", g_terrainDisplacementMap);

    Mesh* mesh = createMesh(&vertexData, &material);

    free(positions);
    free(normals);
    free(texCoords);
    free(indices);

    if (mesh) {
        fprintf(gpFile, "Terrain mesh created: %d vertices, %d patches (%d indices)\n",
                vertexCount, patchCount, indexCount);
    } else {
        fprintf(gpFile, "Error: Failed to create terrain mesh\n");
    }

    return mesh;
}

// ============================================================================
// TERRAIN MESH REGENERATION
// ============================================================================

void regenerateTerrainMesh() {
    if (terrainMesh != NULL) {
        freeMesh(terrainMesh);
        terrainMesh = NULL;
    }

    terrainMesh = createTerrainMesh();

    if (terrainMesh) {
        fprintf(gpFile, "Terrain mesh regenerated successfully\n");
    } else {
        fprintf(gpFile, "Error: Failed to regenerate terrain mesh\n");
    }
}

void switchTerrainMaterial(int materialIndex) {
    if (materialIndex < 0 || materialIndex >= g_terrainMaterialCount) return;
    if (terrainMesh == NULL) return;

    g_terrainMaterialIndex = materialIndex;
    TerrainMaterialDef* matDef = &g_terrainMaterials[materialIndex];

    if (terrainMesh->material.diffuseTexture) glDeleteTextures(1, &terrainMesh->material.diffuseTexture);
    if (terrainMesh->material.normalTexture) glDeleteTextures(1, &terrainMesh->material.normalTexture);
    if (terrainMesh->material.metallicRoughnessTexture) glDeleteTextures(1, &terrainMesh->material.metallicRoughnessTexture);
    if (g_terrainDisplacementMap) glDeleteTextures(1, &g_terrainDisplacementMap);

    loadPNGTexture(&terrainMesh->material.diffuseTexture, const_cast<char*>(matDef->diffusePath), 1);
    loadPNGTexture(&terrainMesh->material.normalTexture, const_cast<char*>(matDef->normalPath), 1);
    loadPNGTexture(&terrainMesh->material.metallicRoughnessTexture, const_cast<char*>(matDef->armPath), 1);
    loadPNGTexture(&g_terrainDisplacementMap, const_cast<char*>(matDef->dispPath), 1);

    fprintf(gpFile, "Switched terrain material to: %s\n", matDef->name);
}

// ============================================================================
// TERRAIN RENDERING
// ============================================================================

extern vec3 lightPos;
extern vec3 lightColor;
extern float lightIntensity;
extern bool useIBL;
extern float iblIntensity;

void renderTerrain(GLint HeightMap) {
    if (terrainMesh == NULL || tessellationShaderProgram == NULL) {
        return;
    }

    if (g_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    glUseProgram(tessellationShaderProgram->id);

    GLint projLoc = glGetUniformLocation(tessellationShaderProgram->id, "uProjection");
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, perspectiveProjectionMatrix);

    GLint viewLoc = glGetUniformLocation(tessellationShaderProgram->id, "uView");
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix);

    mat4 modelMatrix = mat4::identity();
    GLint modelLoc = glGetUniformLocation(tessellationShaderProgram->id, "uModel");
    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMatrix);

    GLint tessInnerLoc = glGetUniformLocation(tessellationShaderProgram->id, "uTessLevelInner");
    GLint tessOuterLoc = glGetUniformLocation(tessellationShaderProgram->id, "uTessLevelOuter");
    if (tessInnerLoc != -1) glUniform1f(tessInnerLoc, (float)g_tessellationInner);
    if (tessOuterLoc != -1) glUniform1f(tessOuterLoc, (float)g_tessellationOuter);

    GLint dispLoc = glGetUniformLocation(tessellationShaderProgram->id, "uDisplacementScale");
    if (dispLoc != -1) glUniform1f(dispLoc, g_displacementScale);

    vec3 viewPosFromCamera = mainCamera ? mainCamera->position : vec3(0.0f, 50.0f, 100.0f);

    GLint lightPosLoc = glGetUniformLocation(tessellationShaderProgram->id, "uLightPos");
    if (lightPosLoc != -1) glUniform3fv(lightPosLoc, 1, lightPos);

    GLint lightColorLoc = glGetUniformLocation(tessellationShaderProgram->id, "uLightColor");
    if (lightColorLoc != -1) glUniform3fv(lightColorLoc, 1, lightColor * lightIntensity);

    GLint viewPosLoc = glGetUniformLocation(tessellationShaderProgram->id, "uViewPos");
    if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, viewPosFromCamera);

    GLint hasIBLLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasIBL");
    if (hasIBLLoc != -1) glUniform1i(hasIBLLoc, useIBL);

    GLint iblIntensityLoc = glGetUniformLocation(tessellationShaderProgram->id, "uIBLIntensity");
    if (iblIntensityLoc != -1) glUniform1f(iblIntensityLoc, iblIntensity);

    if (useIBL) {
        bindIBL(tessellationShaderProgram);
    }

    setMaterialUniforms(tessellationShaderProgram, &terrainMesh->material);

    // Override texture enable flags based on GUI toggles
    GLint hasDiffLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasDiffuseTexture");
    if (hasDiffLoc != -1 && !g_enableTerrainDiffuse) glUniform1i(hasDiffLoc, 0);

    GLint hasNormLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasNormalTexture");
    if (hasNormLoc != -1 && !g_enableTerrainNormalMap) glUniform1i(hasNormLoc, 0);

    GLint hasMetLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasMetallicMap");
    GLint hasRghLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasRoughnessMap");
    GLint hasAOLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasAOMap");
    if (!g_enableTerrainARM) {
        if (hasMetLoc != -1) glUniform1i(hasMetLoc, 0);
        if (hasRghLoc != -1) glUniform1i(hasRghLoc, 0);
        if (hasAOLoc != -1) glUniform1i(hasAOLoc, 0);
    }

    // UV tiling scale
    GLint uvScaleLoc = glGetUniformLocation(tessellationShaderProgram->id, "uUVScale");
    if (uvScaleLoc != -1) glUniform1f(uvScaleLoc, g_terrainUVScale);

    GLint heightLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHeightMap");
    if (heightLoc != -1) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, HeightMap);
        glUniform1i(heightLoc, 9);
    }

    GLint dispMapLoc = glGetUniformLocation(tessellationShaderProgram->id, "uDisplacementMap");
    if (dispMapLoc != -1 && g_terrainDisplacementMap != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, g_terrainDisplacementMap);
        glUniform1i(dispMapLoc, 10);
    }

    GLint hasDispLoc = glGetUniformLocation(tessellationShaderProgram->id, "uHasDisplacementMap");
    if (hasDispLoc != -1) glUniform1i(hasDispLoc, g_enableTerrainDisplacement && g_terrainDisplacementMap != 0);

    glBindVertexArray(terrainMesh->vao);

    if (terrainMesh->ibo && terrainMesh->indexCount > 0) {
        glPatchParameteri(GL_PATCH_VERTICES, 4);
        glDrawElements(GL_PATCHES, terrainMesh->indexCount, GL_UNSIGNED_INT, NULL);

    }

    glBindVertexArray(0);
    glUseProgram(0);

    if (g_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}
