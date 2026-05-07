#include "attrdesc.h"
#include "engine/core/gl/structs.h"

// ============================================================================
// ENTITY REGISTRY
//
// Adding a new entity type:
//   1. Define data struct + add to SceneNode union in structs.h
//   2. Add static const AttrDesc kXxx[] block below + one row in g_EntityTable
//   3. (optional) Add custom attribute UI in attributemanager_layout.cpp
//
// Save / load / generic UI all read this table. No other file needs editing.
// ============================================================================

#define NELEM(arr) (int)(sizeof(arr) / sizeof(arr[0]))

// ---- Light ----------------------------------------------------------------
static const AttrDesc kLight[] = {
    { "type",         ATTR_INT,    (int)offsetof(LightData, type),         1.0f,  0, 2,    0 },
    { "color",        ATTR_COLOR3, (int)offsetof(LightData, color),        0.01f, 0, 0,    0 },
    { "intensity",    ATTR_FLOAT,  (int)offsetof(LightData, intensity),    0.1f,  0, 1000, 0 },
    { "radius",       ATTR_FLOAT,  (int)offsetof(LightData, radius),       0.1f,  0, 1000, 0 },
    { "direction",    ATTR_VEC3,   (int)offsetof(LightData, direction),    0.05f, 0, 0,    0 },
    { "innerCutoff",  ATTR_FLOAT,  (int)offsetof(LightData, innerCutoff),  0.01f, 0, 1,    0 },
    { "outerCutoff",  ATTR_FLOAT,  (int)offsetof(LightData, outerCutoff),  0.01f, 0, 1,    0 },
    { "castShadows",  ATTR_BOOL,   (int)offsetof(LightData, cast_shadows), 0,     0, 0,    0 },
};

// ---- Camera ---------------------------------------------------------------
static const AttrDesc kCamera[] = {
    { "position", ATTR_VEC3,  (int)offsetof(Camera, position), 0.1f,  0,    0,     0 },
    { "target",   ATTR_VEC3,  (int)offsetof(Camera, target),   0.1f,  0,    0,     0 },
    { "up",       ATTR_VEC3,  (int)offsetof(Camera, up),       0.05f, 0,    0,     0 },
    { "roll",     ATTR_FLOAT, (int)offsetof(Camera, roll),     0.5f,  -180, 180,   0 },
    { "fov",      ATTR_FLOAT, (int)offsetof(Camera, fov),      0.5f,  5,    160,   0 },
    { "near",     ATTR_FLOAT, (int)offsetof(Camera, near),     0.01f, 0.001f, 100, 0 },
    { "far",      ATTR_FLOAT, (int)offsetof(Camera, far),      1.0f,  1,    10000, 0 },
};

// ---- Instance -------------------------------------------------------------
static const AttrDesc kInstance[] = {
    { "modelPath",       ATTR_STRING, (int)offsetof(InstanceData, modelPath),       0,     0,     0,    256 },
    { "pattern",         ATTR_INT,    (int)offsetof(InstanceData, pattern),         1.0f,  0,     1,    0   },
    { "gridCountX",      ATTR_INT,    (int)offsetof(InstanceData, gridCountX),      1.0f,  1,     100,  0   },
    { "gridCountZ",      ATTR_INT,    (int)offsetof(InstanceData, gridCountZ),      1.0f,  1,     100,  0   },
    { "spacingX",        ATTR_FLOAT,  (int)offsetof(InstanceData, spacingX),        0.1f,  0.1f,  100,  0   },
    { "spacingZ",        ATTR_FLOAT,  (int)offsetof(InstanceData, spacingZ),        0.1f,  0.1f,  100,  0   },
    { "noiseScale",      ATTR_FLOAT,  (int)offsetof(InstanceData, noiseScale),      0.01f, 0.01f, 1.0f, 0   },
    { "noiseThreshold",  ATTR_FLOAT,  (int)offsetof(InstanceData, noiseThreshold),  0.01f, -1.0f, 1.0f, 0   },
    { "areaWidth",       ATTR_FLOAT,  (int)offsetof(InstanceData, areaWidth),       0.5f,  1.0f,  1000, 0   },
    { "areaDepth",       ATTR_FLOAT,  (int)offsetof(InstanceData, areaDepth),       0.5f,  1.0f,  1000, 0   },
    { "minScale",        ATTR_FLOAT,  (int)offsetof(InstanceData, minScale),        0.1f,  0.01f, 100,  0   },
    { "maxScale",        ATTR_FLOAT,  (int)offsetof(InstanceData, maxScale),        0.1f,  0.01f, 100,  0   },
    { "randomYRotation", ATTR_FLOAT,  (int)offsetof(InstanceData, randomYRotation), 0.1f,  0,     1,    0   },
};

// ---- Terrain --------------------------------------------------------------
static const AttrDesc kTerrain[] = {
    { "heightmapPath",   ATTR_STRING, (int)offsetof(TerrainNodeData, heightmapPath),       0,     0,     0,     256 },
    { "octaves",         ATTR_INT,    (int)offsetof(TerrainNodeData, octaves),             1.0f,  1,     12,    0   },
    { "persistence",     ATTR_FLOAT,  (int)offsetof(TerrainNodeData, persistence),         0.01f, 0,     1,     0   },
    { "lacunarity",      ATTR_FLOAT,  (int)offsetof(TerrainNodeData, lacunarity),          0.01f, 1,     4,     0   },
    { "seed",            ATTR_INT,    (int)offsetof(TerrainNodeData, seed),                1.0f,  0,     10000, 0   },
    { "mountainThreshold", ATTR_FLOAT, (int)offsetof(TerrainNodeData, mountainThreshold),  0.01f, 0,     1,     0   },
    { "desertThreshold", ATTR_FLOAT,  (int)offsetof(TerrainNodeData, desertThreshold),     0.01f, 0,     1,     0   },
    { "mountainHeight",  ATTR_FLOAT,  (int)offsetof(TerrainNodeData, mountainHeightScale), 0.1f,  0,     1000,  0   },
    { "desertHeight",    ATTR_FLOAT,  (int)offsetof(TerrainNodeData, desertHeightScale),   0.1f,  0,     1000,  0   },
    { "plainsHeight",    ATTR_FLOAT,  (int)offsetof(TerrainNodeData, plainsHeightScale),   0.1f,  0,     1000,  0   },
    { "useIslandMask",   ATTR_BOOL,   (int)offsetof(TerrainNodeData, useIslandMask),       0,     0,     0,     0   },
    { "islandFalloff",   ATTR_FLOAT,  (int)offsetof(TerrainNodeData, islandFalloff),       0.01f, 0,     10,    0   },
    { "biomeMode",       ATTR_INT,    (int)offsetof(TerrainNodeData, biomeMode),           1.0f,  0,     2,     0   },
    { "ridgeStrength",   ATTR_FLOAT,  (int)offsetof(TerrainNodeData, ridgeStrength),       0.01f, 0,     2,     0   },
    { "turbulence",      ATTR_FLOAT,  (int)offsetof(TerrainNodeData, turbulence),          0.01f, 0,     10,    0   },
    { "terraceLevels",   ATTR_INT,    (int)offsetof(TerrainNodeData, terraceLevels),       1.0f,  0,     20,    0   },
    { "heightOffset",    ATTR_FLOAT,  (int)offsetof(TerrainNodeData, heightOffset),        0.1f,  -500,  500,   0   },
    { "meshWidth",       ATTR_INT,    (int)offsetof(TerrainNodeData, meshWidth),           1.0f,  4,     1024,  0   },
    { "meshDepth",       ATTR_INT,    (int)offsetof(TerrainNodeData, meshDepth),           1.0f,  4,     1024,  0   },
    { "worldScale",      ATTR_FLOAT,  (int)offsetof(TerrainNodeData, worldScale),          0.1f,  0.1f,  1000,  0   },
    { "tessInner",       ATTR_INT,    (int)offsetof(TerrainNodeData, tessInner),           1.0f,  1,     64,    0   },
    { "tessOuter",       ATTR_INT,    (int)offsetof(TerrainNodeData, tessOuter),           1.0f,  1,     64,    0   },
    { "displacement",    ATTR_FLOAT,  (int)offsetof(TerrainNodeData, displacementScale),   0.1f,  0,     500,   0   },
    { "lodBias",         ATTR_INT,    (int)offsetof(TerrainNodeData, lodBias),             1.0f,  0,     10,    0   },
    { "wireframe",       ATTR_BOOL,   (int)offsetof(TerrainNodeData, wireframe),           0,     0,     0,     0   },
    { "materialIndex",   ATTR_INT,    (int)offsetof(TerrainNodeData, materialIndex),       1.0f,  0,     10,    0   },
    { "uvScale",         ATTR_FLOAT,  (int)offsetof(TerrainNodeData, uvScale),             1.0f,  0.1f,  1000,  0   },
    { "enableDiffuse",   ATTR_BOOL,   (int)offsetof(TerrainNodeData, enableDiffuse),       0,     0,     0,     0   },
    { "enableNormal",    ATTR_BOOL,   (int)offsetof(TerrainNodeData, enableNormal),        0,     0,     0,     0   },
    { "enableARM",       ATTR_BOOL,   (int)offsetof(TerrainNodeData, enableARM),           0,     0,     0,     0   },
    { "enableDisp",      ATTR_BOOL,   (int)offsetof(TerrainNodeData, enableDisplacement),  0,     0,     0,     0   },
    { "powerCurve",      ATTR_FLOAT,  (int)offsetof(TerrainNodeData, powerCurve),          0.01f, 0.1f,  10.0f, 0   },
    { "heightmapSource", ATTR_INT,    (int)offsetof(TerrainNodeData, heightmapSource),     1.0f,  0,     1,     0   },
};

// ---- Skybox ---------------------------------------------------------------
static const AttrDesc kSkybox[] = {
    { "hdrPath",       ATTR_STRING, (int)offsetof(SkyboxNodeData, hdrPath),       0,    0, 0, 256 },
    { "currentPreset", ATTR_INT,    (int)offsetof(SkyboxNodeData, currentPreset), 1.0f, 0, 1, 0   },
};

// ============================================================================
// THE TABLE — one row per entity type
// ============================================================================
const EntityDesc g_EntityTable[] = {
    { ENTITY_EMPTY,    "EMPTY",    nullptr,    nullptr,   0,                 0                                    },
    { ENTITY_MODEL,    "MODEL",    nullptr,    nullptr,   0,                 0                                    }, // sourcePath/meshIndex/Material handled specially
    { ENTITY_LIGHT,    "LIGHT",    "Light",    kLight,    NELEM(kLight),     offsetof(SceneNode, data.light)      },
    { ENTITY_CAMERA,   "CAMERA",   "Camera",   kCamera,   NELEM(kCamera),    offsetof(SceneNode, data.camera)     },
    { ENTITY_INSTANCE, "INSTANCE", "Instance", kInstance, NELEM(kInstance),  offsetof(SceneNode, data.instance)   },
    { ENTITY_TERRAIN,  "TERRAIN",  "Terrain",  kTerrain,  NELEM(kTerrain),   offsetof(SceneNode, data.terrain)    },
    { ENTITY_SKYBOX,   "SKYBOX",   "Skybox",   kSkybox,   NELEM(kSkybox),    offsetof(SceneNode, data.skybox)     },
};
const int g_EntityTableCount = NELEM(g_EntityTable);

const EntityDesc* findEntityDesc(NodeType t) {
    for (int i = 0; i < g_EntityTableCount; i++)
        if (g_EntityTable[i].type == t) return &g_EntityTable[i];
    return nullptr;
}
