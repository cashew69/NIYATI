#include "engine/engine.h"
#include "engine/effects/terrain/terrain.h"
#include "engine/utils/scenegraph.h"
#include <stdio.h>

static void CacheCPUHeightmap(TerrainNodeData* data) {
    if (data->cpuHeightMap) {
        free(data->cpuHeightMap);
        data->cpuHeightMap = nullptr;
    }
    if (data->heightmapTex != 0) {
        glBindTexture(GL_TEXTURE_2D, data->heightmapTex);
        int w = 0, h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        if (w > 0 && h > 0) {
            unsigned char* pixels = (unsigned char*)malloc(w * h * 4);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            data->cpuHeightMap = (float*)malloc(w * h * sizeof(float));
            for(int i=0; i<w*h; i++) {
                data->cpuHeightMap[i] = (pixels[i*4] / 255.0f - 0.5f);
            }
            free(pixels);
            data->cpuHeightMapWidth = w;
            data->cpuHeightMapHeight = h;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

// Linkage to globals in terrain.cpp (we need to sync these with TerrainNodeData)
extern int g_terrainOctaves;
extern float g_terrainPersistence;
extern float g_terrainLacunarity;
extern float g_mountainThreshold;
extern float g_desertThreshold;
extern float g_mountainHeightScale;
extern float g_desertHeightScale;
extern float g_plainsHeightScale;
extern bool g_useIslandMask;
extern int g_biomeMode;
extern float g_ridgeStrength;
extern float g_heightOffset;
extern float g_islandFalloff;
extern int g_terrainMeshWidth;
extern int g_terrainMeshDepth;
extern float g_terrainScale;
extern int g_tessellationInner;
extern int g_tessellationOuter;
extern float g_displacementScale;
extern bool g_wireframeMode;
extern int g_lodBias;
extern int g_heightmapSource;
extern char g_heightmapFilePath[256];
extern GLuint g_loadedHeightmapTexture;
extern GLuint g_terrainDisplacementMap;
extern bool g_enableTerrainDiffuse;
extern bool g_enableTerrainNormalMap;
extern bool g_enableTerrainARM;
extern bool g_enableTerrainDisplacement;
extern float g_terrainUVScale;
extern int g_terrainMaterialIndex;
extern int g_perlinSeed;
extern float g_turbulence;
extern int g_terraceLevels;
extern float g_powerCurve;

static void SyncNodeToGlobals(TerrainNodeData* data) {
    g_terrainOctaves = data->octaves;
    g_terrainPersistence = data->persistence;
    g_terrainLacunarity = data->lacunarity;
    g_mountainThreshold = data->mountainThreshold;
    g_desertThreshold = data->desertThreshold;
    g_mountainHeightScale = data->mountainHeightScale;
    g_desertHeightScale = data->desertHeightScale;
    g_plainsHeightScale = data->plainsHeightScale;
    g_useIslandMask = data->useIslandMask;
    g_biomeMode = data->biomeMode;
    g_ridgeStrength = data->ridgeStrength;
    g_heightOffset = data->heightOffset;
    g_islandFalloff = data->islandFalloff;
    g_terrainMeshWidth = data->meshWidth;
    g_terrainMeshDepth = data->meshDepth;
    g_terrainScale = data->worldScale;
    g_tessellationInner = data->tessInner;
    g_tessellationOuter = data->tessOuter;
    g_displacementScale = data->displacementScale;
    g_wireframeMode = data->wireframe;
    g_lodBias = data->lodBias;
    g_heightmapSource = data->heightmapSource;
    strncpy(g_heightmapFilePath, data->heightmapPath, 256);
    g_loadedHeightmapTexture = data->heightmapTex;
    g_enableTerrainDiffuse = data->enableDiffuse;
    g_enableTerrainNormalMap = data->enableNormal;
    g_enableTerrainARM = data->enableARM;
    g_enableTerrainDisplacement = data->enableDisplacement;
    g_terrainUVScale = data->uvScale;
    g_terrainMaterialIndex = data->materialIndex;
    g_perlinSeed = data->seed;
    g_turbulence = data->turbulence;
    g_terraceLevels = data->terraceLevels;
    g_powerCurve = data->powerCurve;

    // Link the node-specific mesh and textures to the globals terrain.cpp expects
    extern Mesh* terrainMesh;
    terrainMesh = data->mesh;
    g_terrainDisplacementMap = data->displacementMapTex;
}

static void SyncGlobalsToNode(TerrainNodeData* data) {
    data->octaves = g_terrainOctaves;
    data->persistence = g_terrainPersistence;
    data->lacunarity = g_terrainLacunarity;
    data->mountainThreshold = g_mountainThreshold;
    data->desertThreshold = g_desertThreshold;
    data->mountainHeightScale = g_mountainHeightScale;
    data->desertHeightScale = g_desertHeightScale;
    data->plainsHeightScale = g_plainsHeightScale;
    data->useIslandMask = g_useIslandMask;
    data->biomeMode = g_biomeMode;
    data->ridgeStrength = g_ridgeStrength;
    data->heightOffset = g_heightOffset;
    data->islandFalloff = g_islandFalloff;
    data->meshWidth = g_terrainMeshWidth;
    data->meshDepth = g_terrainMeshDepth;
    data->worldScale = g_terrainScale;
    data->tessInner = g_tessellationInner;
    data->tessOuter = g_tessellationOuter;
    data->displacementScale = g_displacementScale;
    data->wireframe = g_wireframeMode;
    data->lodBias = g_lodBias;
    data->heightmapSource = g_heightmapSource;
    strncpy(data->heightmapPath, g_heightmapFilePath, 256);
    data->heightmapTex = g_loadedHeightmapTexture;
    data->enableDiffuse = g_enableTerrainDiffuse;
    data->enableNormal = g_enableTerrainNormalMap;
    data->enableARM = g_enableTerrainARM;
    data->enableDisplacement = g_enableTerrainDisplacement;
    data->uvScale = g_terrainUVScale;
    data->materialIndex = g_terrainMaterialIndex;
    data->seed = g_perlinSeed;
    data->turbulence = g_turbulence;
    data->terraceLevels = g_terraceLevels;
    data->powerCurve = g_powerCurve;

    extern Mesh* terrainMesh;
    data->mesh = terrainMesh;
    data->displacementMapTex = g_terrainDisplacementMap;
}

void sg_InitTerrainNode(SceneNode* node) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    TerrainNodeData* data = &node->data.terrain;

    // Set some defaults if not initialized
    if (data->meshWidth == 0) {
        data->octaves = 12;
        data->persistence = 0.5f;
        data->lacunarity = 2.0f;
        data->mountainThreshold = 0.3f;
        data->desertThreshold = -0.2f;
        data->mountainHeightScale = 50.0f;
        data->desertHeightScale = 600.0f;
        data->plainsHeightScale = 20.0f;
        data->biomeMode = 2;
        data->ridgeStrength = 1.0f;
        data->heightOffset = 0.0f;
        data->islandFalloff = 2.0f;
        data->meshWidth = 256;
        data->meshDepth = 256;
        data->worldScale = 10.0f;
        data->tessInner = 4;
        data->tessOuter = 4;
        data->displacementScale = 20.0f;
        data->uvScale = 100.0f;
        data->enableDiffuse = true;
        data->enableNormal = true;
        data->enableARM = true;
        data->enableDisplacement = true;
        data->powerCurve = 1.0f;
    }

    SyncNodeToGlobals(data);
    
    if (data->heightmapTex == 0) {
        data->heightmapTex = createHeightMapTexture(512, 512, 0.01f, 1.0f);
    }
    
    if (data->mesh == nullptr) {
        data->mesh = createTerrainMesh();
    }
    
    CacheCPUHeightmap(data);
}

void sg_RenderTerrainNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    TerrainNodeData* data = &node->data.terrain;

    if (data->mesh == nullptr) return;

    SyncNodeToGlobals(data);
    
    renderTerrain(data->heightmapTex, node->world_matrix);
}

void sg_RegenerateTerrain(SceneNode* node) {
    if (!node || node->type != ENTITY_TERRAIN) return;
    TerrainNodeData* data = &node->data.terrain;

    SyncNodeToGlobals(data);
    
    if (data->heightmapSource == 0) {
        if (data->heightmapTex) glDeleteTextures(1, &data->heightmapTex);
        data->heightmapTex = createHeightMapTexture(512, 512, 0.01f, 1.0f);
    }
    
    if (data->mesh) {
        extern Mesh* terrainMesh;
        Mesh* oldMesh = terrainMesh;
        terrainMesh = data->mesh;
        regenerateTerrainMesh();
        data->mesh = terrainMesh;
        terrainMesh = oldMesh;
    }
    
    CacheCPUHeightmap(data);
}
