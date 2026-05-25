#ifndef TERRAIN_CPP
#define TERRAIN_CPP

#include "terrain.h"
#include "../engine/effects/noise/perlin.h"
#include "../engine/effects/noise/noise.c"
#include <GL/gl.h>
#include <cmath>
#include <cstdlib>

// Owned here; terrain_node.cpp accesses via extern.
Mesh* terrainMesh = NULL;

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
// g_wireframeMode now in engine.h
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
bool g_enableTerrainStochastic = false;
float g_stochasticContrast = 8.0f;
float g_stochasticScale = 1.0f;
float g_terrainUVScale = 100.0f;
float g_terrainRoughness = 1.0f;
float g_terrainMetalness = 0.0f;
int g_terrainMaterialIndex = 0;

char g_terrainDiffusePath[256] = "";
char g_terrainNormalPath[256] = "";
char g_terrainARMPath[256] = "";
char g_terrainDispPath[256] = "";

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
    {"Red Mud Stones", "user/models/Kurukshetra/red_mud_stones_diff_2k.png", "user/models/Kurukshetra/red_mud_stones_nor_gl_2k.png", "user/models/Kurukshetra/red_mud_stones_arm_2k.png", "user/models/Kurukshetra/red_mud_stones_disp_2k.png"},
};
int g_terrainMaterialCount = sizeof(g_terrainMaterials) / sizeof(g_terrainMaterials[0]);

// PLANE_WIDTH and PLANE_DEPTH macros removed in favor of g_terrainMeshWidth/Depth

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

void createTerrainTextures(int width, int depth, float frequency, float heightScale,
                            GLuint* outHeightTex, GLuint* outNormalTex) {
    TerrainData data = generateTerrainData(width, depth, frequency);
    if (outHeightTex) *outHeightTex = createTextureFromPixels(data.heightPixels, width, depth);
    if (outNormalTex) *outNormalTex = createTextureFromPixels(data.normalPixels, width, depth);
    freeTerrainData(&data);
}

GLuint createHeightMapTexture(int width, int depth, float frequency, float heightScale) {
    GLuint tex = 0;
    createTerrainTextures(width, depth, frequency, heightScale, &tex, nullptr);
    return tex;
}

GLuint createNormalMapTexture(int width, int depth, float frequency, float heightScale) {
    GLuint tex = 0;
    createTerrainTextures(width, depth, frequency, heightScale, nullptr, &tex);
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
    LOG_I("Generating terrain mesh %dx%d...", g_terrainMeshWidth, g_terrainMeshDepth);

    int vertexCount = g_terrainMeshWidth * g_terrainMeshDepth;
    int patchCount = (g_terrainMeshWidth - 1) * (g_terrainMeshDepth - 1);
    int indexCount = patchCount * 4;

    float* positions = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* normals = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* texCoords = (float*)malloc(vertexCount * 2 * sizeof(float));
    unsigned int* indices = (unsigned int*)malloc(indexCount * sizeof(unsigned int));

    if (!positions || !normals || !texCoords || !indices) {
        LOG_E("Failed to allocate terrain mesh data");
        free(positions);
        free(normals);
        free(texCoords);
        free(indices);
        return NULL;
    }

    float spacing = g_terrainScale;
    float halfWidth = (g_terrainMeshWidth * spacing) / 2.0f;
    float halfDepth = (g_terrainMeshDepth * spacing) / 2.0f;

    int idx = 0;
    for (int z = 0; z < g_terrainMeshDepth; z++) {
        for (int x = 0; x < g_terrainMeshWidth; x++) {
            positions[idx * 3 + 0] = (float)x * spacing - halfWidth;
            positions[idx * 3 + 1] = 0.0f;
            positions[idx * 3 + 2] = (float)z * spacing - halfDepth;

            normals[idx * 3 + 0] = 0.0f;
            normals[idx * 3 + 1] = 1.0f;
            normals[idx * 3 + 2] = 0.0f;

            texCoords[idx * 2 + 0] = (float)x / (float)(g_terrainMeshWidth - 1);
            texCoords[idx * 2 + 1] = (float)z / (float)(g_terrainMeshDepth - 1);

            idx++;
        }
    }

    idx = 0;
    for (int z = 0; z < g_terrainMeshDepth - 1; z++) {
        for (int x = 0; x < g_terrainMeshWidth - 1; x++) {
            int bottomLeft = z * g_terrainMeshWidth + x;
            int bottomRight = bottomLeft + 1;
            int topLeft = (z + 1) * g_terrainMeshWidth + x;
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

    // Enable texture usage flags for PBR
    material.useDiffuseTexture = true;
    material.useNormalTexture = true;
    material.useMetallicRoughnessTexture = true;
    material.useAOTexture = true;
    material.useEmissiveTexture = true;

    material.roughness = g_terrainRoughness;
    material.metalness = g_terrainMetalness;

    const char* dPath = "";
    const char* nPath = "";
    const char* aPath = "";
    const char* sPath = "";

    if (g_terrainMaterialIndex == -1) {
        dPath = g_terrainDiffusePath;
        nPath = g_terrainNormalPath;
        aPath = g_terrainARMPath;
        sPath = g_terrainDispPath;
    } else {
        TerrainMaterialDef* matDef = &g_terrainMaterials[g_terrainMaterialIndex];
        dPath = matDef->diffusePath;
        nPath = matDef->normalPath;
        aPath = matDef->armPath;
        sPath = matDef->dispPath;
    }

    if (dPath[0]) loadPNGTexture(&material.diffuseTexture, const_cast<char*>(dPath), 1);
    if (nPath[0]) loadPNGTexture(&material.normalTexture, const_cast<char*>(nPath), 1);
    if (aPath[0]) loadPNGTexture(&material.metallicRoughnessTexture, const_cast<char*>(aPath), 1);
    if (sPath[0]) loadPNGTexture(&g_terrainDisplacementMap, const_cast<char*>(sPath), 1);
    
    LOG_I("Terrain textures loaded: diffuse=%u normal=%u ARM=%u disp=%u",
          material.diffuseTexture, material.normalTexture,
          material.metallicRoughnessTexture, g_terrainDisplacementMap);

    Mesh* mesh = createMesh(&vertexData, &material);

    free(positions);
    free(normals);
    free(texCoords);
    free(indices);

    if (mesh) {
        mesh->aabbLocal.min[1] -= g_displacementScale * 2.0f;
        mesh->aabbLocal.max[1] += g_displacementScale * 2.0f;
        LOG_I("Terrain mesh created: %d vertices, %d patches (%d indices)", vertexCount, patchCount, indexCount);
    } else {
        LOG_E("Failed to create terrain mesh");
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
        LOG_I("Terrain mesh regenerated successfully");
    } else {
        LOG_E("Failed to regenerate terrain mesh");
    }
}

void switchTerrainMaterial(int materialIndex) {
    if (materialIndex < -1 || materialIndex >= g_terrainMaterialCount) return;
    if (terrainMesh == NULL) return;

    g_terrainMaterialIndex = materialIndex;

    const char* dPath = "";
    const char* nPath = "";
    const char* aPath = "";
    const char* sPath = "";

    if (materialIndex == -1) {
        dPath = g_terrainDiffusePath;
        nPath = g_terrainNormalPath;
        aPath = g_terrainARMPath;
        sPath = g_terrainDispPath;
    } else {
        TerrainMaterialDef* matDef = &g_terrainMaterials[materialIndex];
        dPath = matDef->diffusePath;
        nPath = matDef->normalPath;
        aPath = matDef->armPath;
        sPath = matDef->dispPath;
    }

    if (terrainMesh->material.diffuseTexture) glDeleteTextures(1, &terrainMesh->material.diffuseTexture);
    if (terrainMesh->material.normalTexture) glDeleteTextures(1, &terrainMesh->material.normalTexture);
    if (terrainMesh->material.metallicRoughnessTexture) glDeleteTextures(1, &terrainMesh->material.metallicRoughnessTexture);
    if (g_terrainDisplacementMap) glDeleteTextures(1, &g_terrainDisplacementMap);

    terrainMesh->material.diffuseTexture = 0;
    terrainMesh->material.normalTexture = 0;
    terrainMesh->material.metallicRoughnessTexture = 0;
    g_terrainDisplacementMap = 0;

    if (dPath[0]) loadPNGTexture(&terrainMesh->material.diffuseTexture, const_cast<char*>(dPath), 1);
    if (nPath[0]) loadPNGTexture(&terrainMesh->material.normalTexture, const_cast<char*>(nPath), 1);
    if (aPath[0]) loadPNGTexture(&terrainMesh->material.metallicRoughnessTexture, const_cast<char*>(aPath), 1);
    if (sPath[0]) loadPNGTexture(&g_terrainDisplacementMap, const_cast<char*>(sPath), 1);

    LOG_I("Switched terrain material to: %s", (materialIndex == -1) ? "Custom" : g_terrainMaterials[materialIndex].name);
}

// ============================================================================
// TERRAIN RENDERING
// ============================================================================

extern vec3 lightPos;
extern vec3 lightColor;
extern vec3 lightDir;
extern int lightType;
extern float lightRadius;
extern float lightInnerCutoff;
extern float lightOuterCutoff;
extern float lightIntensity;
extern bool useIBL;
extern float iblIntensity;

void renderTerrain(GLint HeightMap, mat4 modelMatrix, mat4 view, mat4 proj) {
    if (terrainMesh == NULL || tessellationShaderProgram == NULL) {
        return;
    }

    if (g_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    glUseProgram(tessellationShaderProgram->id);
    ShaderLocations& loc = tessellationShaderProgram->loc;

    glUniformMatrix4fv(loc.uProjection,     1, GL_FALSE, proj);
    glUniformMatrix4fv(loc.uView,           1, GL_FALSE, view);
    glUniformMatrix4fv(loc.uModel,          1, GL_FALSE, modelMatrix);

    glUniform1f(loc.uTessLevelInner,    (float)g_tessellationInner);
    glUniform1f(loc.uTessLevelOuter,    (float)g_tessellationOuter);
    glUniform1f(loc.uDisplacementScale, g_displacementScale);

    extern Camera* GetActiveCamera();
    Camera* cam = GetActiveCamera();
    vec3 viewPosFromCamera = cam ? cam->position : vec3(0.0f, 50.0f, 100.0f);

    glUniform3fv(loc.uLightPos,       1, lightPos);
    glUniform3fv(loc.uLightColor,     1, lightColor);
    glUniform1f(loc.uLightIntensity,  lightIntensity);
    glUniform1i(loc.uLightType,       lightType);
    glUniform3fv(loc.uLightDir,       1, lightDir);
    glUniform1f(loc.uLightRadius,     lightRadius);
    glUniform1f(loc.uInnerCutoff,     lightInnerCutoff);
    glUniform1f(loc.uOuterCutoff,     lightOuterCutoff);
    
    glUniform3fv(loc.uViewPos,     1, viewPosFromCamera);
    glUniform1i(loc.uHasIBL,       useIBL);
    glUniform1f(loc.uIBLIntensity, iblIntensity);

    if (useIBL) {
        bindIBL(tessellationShaderProgram);
    }

    extern void setDebugUniforms(ShaderProgram* program);
    setDebugUniforms(tessellationShaderProgram);

    extern void setFogUniforms(ShaderProgram* program);
    setFogUniforms(tessellationShaderProgram);

    terrainMesh->material.roughness = g_terrainRoughness;
    terrainMesh->material.metalness = g_terrainMetalness;
    setMaterialUniforms(tessellationShaderProgram, &terrainMesh->material);

    // Override texture enable flags based on GUI toggles
    if (!g_enableTerrainDiffuse)  glUniform1i(loc.uHasDiffuseTexture, 0);
    if (!g_enableTerrainNormalMap) glUniform1i(loc.uHasNormalTexture,  0);
    if (!g_enableTerrainARM) {
        glUniform1i(loc.uHasMetallicMap,   0);
        glUniform1i(loc.uHasRoughnessMap,  0);
        glUniform1i(loc.uHasAOMap,         0);
    }

    glUniform1f(loc.uUVScale, g_terrainUVScale);

    // Heightmap goes on slot 11 — slot 9 is reserved for the shadow map
    // (layout(binding = 9) in pbrFrag.glsl), and slot 10 is the displacement map.
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, HeightMap);
    glUniform1i(loc.uHeightMap, 11);

    int hmW = 512;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &hmW);
    glUniform1f(loc.uTexelSize, 1.0f / (float)hmW);

    if (g_terrainDisplacementMap != 0) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, g_terrainDisplacementMap);
        glUniform1i(loc.uDisplacementMap, 10);
    }

    glUniform1i(loc.uHasDisplacementMap, g_enableTerrainDisplacement && g_terrainDisplacementMap != 0);
    glUniform1i(loc.uEnableStochastic, g_enableTerrainStochastic ? 1 : 0);
    glUniform1f(loc.uStochasticContrast, g_stochasticContrast > 0.0f ? g_stochasticContrast : 8.0f);
    glUniform1f(loc.uStochasticScale, g_stochasticScale > 0.0f ? g_stochasticScale : 1.0f);
    glUniform1f(loc.uUVScale, g_terrainUVScale > 0.0f ? g_terrainUVScale : 100.0f);

    // Shadow uniforms — Bind the depth texture on slot 9 (matches the
    // layout(binding = 9) sampler2DShadow uShadowMap in pbrFrag.glsl).
    extern bool   g_ShadowActive;
    extern mat4   g_ShadowSBPV;
    extern GLuint g_ShadowDepthTexID;
    extern float  g_ShadowBias;

    if (loc.uShadowEnabled >= 0) {
        glUniform1i(loc.uShadowEnabled, g_ShadowActive ? 1 : 0);
    }
    
    if (loc.uShadowMatrix >= 0) {
        glUniformMatrix4fv(loc.uShadowMatrix, 1, GL_FALSE, (const float*)g_ShadowSBPV);
    }

    if (g_ShadowActive && loc.uShadowMap >= 0) {
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, g_ShadowDepthTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glUniform1i(loc.uShadowMap, 9);
        glUniform1f(loc.uShadowBias, g_ShadowBias);
    }

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
#endif // TERRAIN_CPP
