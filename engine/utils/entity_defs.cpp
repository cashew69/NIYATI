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
    { "type",             ATTR_INT,    (int)offsetof(LightData, type),             1.0f,  0, 2,    0 },
    { "color",            ATTR_COLOR3, (int)offsetof(LightData, color),            0.01f, 0, 0,    0 },
    { "intensity",        ATTR_FLOAT,  (int)offsetof(LightData, intensity),        0.1f,  0, 1000, 0 },
    { "radius",           ATTR_FLOAT,  (int)offsetof(LightData, radius),           0.1f,  0, 1000, 0 },
    { "direction",        ATTR_VEC3,   (int)offsetof(LightData, direction),        0.05f, 0, 0,    0 },
    { "innerCutoff",      ATTR_FLOAT,  (int)offsetof(LightData, innerCutoff),      0.01f, 0, 1,    0 },
    { "outerCutoff",      ATTR_FLOAT,  (int)offsetof(LightData, outerCutoff),      0.01f, 0, 1,    0 },
    { "castShadow",       ATTR_BOOL,   (int)offsetof(LightData, castShadow),       0,     0, 0,    0 },
    { "shadowResolution", ATTR_INT,    (int)offsetof(LightData, shadowResolution), 1.0f,  128, 8192, 0 },
    { "shadowBias",       ATTR_FLOAT,  (int)offsetof(LightData, shadowBias),       0.0001f, 0, 0.5f, 0 },
    { "shadowOrthoSize",  ATTR_FLOAT,  (int)offsetof(LightData, shadowOrthoSize),  0.5f,  1, 5000, 0 },
    { "shadowNear",       ATTR_FLOAT,  (int)offsetof(LightData, shadowNear),       0.1f,  0.1f, 100, 0 },
    { "shadowFar",        ATTR_FLOAT,  (int)offsetof(LightData, shadowFar),        1.0f,  1, 10000, 0 },
    { "shadowPolyFactor", ATTR_FLOAT,  (int)offsetof(LightData, shadowPolyFactor), 0.05f, 0, 16,    0 },
    { "shadowPolyUnits",  ATTR_FLOAT,  (int)offsetof(LightData, shadowPolyUnits),  0.1f,  0, 64,    0 },
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
    { "roughness",       ATTR_FLOAT,  (int)offsetof(TerrainNodeData, roughness),           0.01f, 0,     1,     0   },
    { "metalness",       ATTR_FLOAT,  (int)offsetof(TerrainNodeData, metalness),           0.01f, 0,     1,     0   },
};

// ---- Skybox ---------------------------------------------------------------
static const AttrDesc kSkybox[] = {
    { "hdrPath",       ATTR_STRING, (int)offsetof(SkyboxNodeData, hdrPath),       0,    0, 0, 256 },
    { "currentPreset", ATTR_INT,    (int)offsetof(SkyboxNodeData, currentPreset), 1.0f, 0, 1, 0   },
};

// ---- Catmull-Rom Spline ----------------------------------------------------
static const AttrDesc kCatmullRom[] = {
    { "tension",          ATTR_FLOAT,  (int)offsetof(CatmullRomNodeData, tension),          0.01f, 0, 10,  0 },
    { "segmentsPerCurve", ATTR_INT,    (int)offsetof(CatmullRomNodeData, segmentsPerCurve), 1.0f,  1, 100, 0 },
    { "isLooping",        ATTR_BOOL,   (int)offsetof(CatmullRomNodeData, isLooping),        0,     0, 0,   0 },
    { "showControlPoints",ATTR_BOOL,   (int)offsetof(CatmullRomNodeData, showControlPoints),0,     0, 0,   0 },
    { "color",            ATTR_COLOR3, (int)offsetof(CatmullRomNodeData, color),            0.01f, 0, 0,   0 },
};

// ---- Volumetric Cloud -------------------------------------------------------
static const AttrDesc kVolumetricCloud[] = {
    // Appearance
    { "cloudColorTop",     ATTR_COLOR3, (int)offsetof(VolumetricCloudNodeData, cloudColorTop),     0.01f, 0,    0,     0 },
    { "cloudColorBottom",  ATTR_COLOR3, (int)offsetof(VolumetricCloudNodeData, cloudColorBottom),  0.01f, 0,    0,     0 },
    { "absorption",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, absorption),        0.01f, 0.01f,1,     0 },
    { "coverage",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, coverage),          0.05f, -3,   3,     0 },
    { "erosion",           ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, erosion),           0.01f, 0.1f, 1,     0 },
    { "silverLining",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, silverLining),      0.01f, 0,    3,     0 },
    { "flatBottom",        ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, flatBottom),        0,     0,    0,     0 },
    { "cloudType",         ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, cloudType),         0.05f, 0,    1,     0 },
    { "noiseScale",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, noiseScale),        0.001f,0.001f,0.05f, 0 },
    { "detailScale",       ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, detailScale),       0.002f,0.002f,0.1f,  0 },
    { "noiseRes",          ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, noiseRes),          1.0f,  16,   512,   0 },
    { "noiseBasePath",     ATTR_STRING, (int)offsetof(VolumetricCloudNodeData, noiseBaseTexPath),   0,     0,    0,     256 },
    { "noiseDetailPath",   ATTR_STRING, (int)offsetof(VolumetricCloudNodeData, noiseDetailTexPath), 0,     0,    0,     256 },
    // Lighting
    { "sunDirection",      ATTR_VEC3,   (int)offsetof(VolumetricCloudNodeData, sunDirection),      0.01f, 0,    0,     0 },
    { "sunColor",          ATTR_COLOR3, (int)offsetof(VolumetricCloudNodeData, sunColor),          0.01f, 0,    0,     0 },
    { "sunIntensity",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, sunIntensity),      0.1f,  0,    100,   0 },
    { "ambientStrength",   ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, ambientStrength),   0.01f, 0,    10,    0 },
    { "scatterG",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, scatterG),          0.01f, -1,   1,     0 },
    // Raymarching
    { "densityScale",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, densityScale),      0.1f,  0.1f, 50,    0 },
    { "maxSteps",          ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, maxSteps),          1.0f,  8,    256,   0 },
    { "stepSize",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, stepSize),          0.01f, 0.05f,5,     0 },
    { "turbulence",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, turbulence),        0.01f, 0,    4,     0 },
    { "windSpeed",         ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, windSpeed),         0.01f, 0,    10,    0 },
    { "localNoiseSpeed",   ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, localNoiseSpeed),   0.01f, 0,    10,    0 },
    // Scene-depth occlusion
    { "useSceneDepth",     ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useSceneDepth),     0,     0,    0,     0 },
    // Volume
    { "boxSize",           ATTR_VEC3,   (int)offsetof(VolumetricCloudNodeData, boxSize),           1.0f,  1,    10000, 0 },
    { "gridX",             ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, gridX),             1.0f,  1,    20,    0 },
    { "gridZ",             ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, gridZ),             1.0f,  1,    20,    0 },
    { "gridSpacing",       ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, gridSpacing),       0.5f,  5,    200,   0 },
    { "gridScale",         ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, gridScale),         0.05f, 0.1f, 10,    0 },
    { "spheresPerMin",     ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, spheresPerCloudMin),1.0f,  1,    16,    0 },
    { "spheresPerMax",     ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, spheresPerCloudMax),1.0f,  1,    32,    0 },
    // Rendering
    { "renderScale",       ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, renderScale),       0.05f, 0.1f, 1,     0 },
    { "enableTAA",         ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, enableTAA),         0,     0,    0,     0 },
    { "taaBlend",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, taaBlend),          0.01f, 0.01f,1,     0 },
    // Circle field
    { "useCircleField",    ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useCircleField),    0,     0,    0,     0 },
    { "circleRadius",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, circleRadius),      1.0f,  10,   10000, 0 },
    // Sphere field
    { "useSphereField",    ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useSphereField),    0,     0,    0,     0 },
    { "planetRadius",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, planetRadius),      10.0f, 500,  50000, 0 },
    { "cloudBaseHeight",   ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, cloudBaseHeight),   1.0f,  0,    5000,  0 },
    { "cloudThickness",    ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, cloudThickness),    1.0f,  10,   5000,  0 },
    { "domeExtent",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, domeExtent),        0.01f, 0,    1,     0 },
    // Weather map
    { "useWeatherMap",       ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useWeatherMap),       0,      0,      0,     0 },
    { "weatherMapPath",      ATTR_STRING, (int)offsetof(VolumetricCloudNodeData, weatherMapPath),      0,      0,      0,     256 },
    { "weatherMapScale",     ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, weatherMapScale),     0.0001f,0.0001f,0.05f, 0 },
    { "autoWeatherMap",      ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, autoWeatherMap),      0,      0,      0,     0 },
    { "weatherMapGridExtent",ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, weatherMapGridExtent),1.0f,   0,      50000, 0 },
    // Atmospheric fog
    { "fogColor",          ATTR_COLOR3, (int)offsetof(VolumetricCloudNodeData, fogColor),          0.01f, 0,    0,     0 },
    { "fogDensity",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, fogDensity),        0.0001f,0,   0.01f, 0 },
    { "fogStart",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, fogStart),          1.0f,  0,    2000,  0 },
    // NVDF
    { "useNVDF",           ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useNVDF),           0,     0,    0,     0 },
    { "nvdfPath",          ATTR_STRING, (int)offsetof(VolumetricCloudNodeData, nvdfPath),          0,     0,    0,     256 },
    { "hwModStrength",     ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, hwModStrength),     0.01f, 0,    1,     0 },
    { "hwModScale",        ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, hwModScale),        0.0001f,0.0001f,0.05f, 0 },
    { "curlStrength",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, curlStrength),      0.01f, 0,    1,     0 },
    { "nvdfTileScale",     ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nvdfTileScale),     0.001f,0.001f,0.5f, 0 },
    { "nvdfRotAngle",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nvdfRotAngle),      0.01f, -3.14159f, 3.14159f, 0 },
    { "nvdfRotX",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nvdfRotX),          0.01f, -3.14159f, 3.14159f, 0 },
    { "nvdfRotZ",          ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nvdfRotZ),          0.01f, -3.14159f, 3.14159f, 0 },
    { "nvdfWorldOffset",   ATTR_VEC3,   (int)offsetof(VolumetricCloudNodeData, nvdfWorldOffset),   1.0f,  0,    0,     0 },
    { "nvdfYOffset",       ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nvdfYOffset),       0.005f,-1,   1,     0 },
    // Adaptive raymarching
    { "adaptiveFactor",    ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, adaptiveFactor),    0.001f,0,    0.1f,  0 },
    { "jitterSwitchDist",  ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, jitterSwitchDist),  1.0f,  0,    2000,  0 },
    // Dual pass
    { "useDualPass",       ATTR_BOOL,   (int)offsetof(VolumetricCloudNodeData, useDualPass),       0,     0,    0,     0 },
    { "nearFarSplit",      ATTR_FLOAT,  (int)offsetof(VolumetricCloudNodeData, nearFarSplit),      1.0f,  0,    2000,  0 },
    { "nearOutputW",       ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, nearOutputW),       1.0f,  64,   2048,  0 },
    { "nearOutputH",       ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, nearOutputH),       1.0f,  32,   1024,  0 },
    { "farOutputW",        ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, farOutputW),        1.0f,  64,   2048,  0 },
    { "farOutputH",        ATTR_INT,    (int)offsetof(VolumetricCloudNodeData, farOutputH),        1.0f,  32,   1024,  0 },
};

// ---- Sky Atmosphere --------------------------------------------------------
static const AttrDesc kSkyAtmosphere[] = {
    { "bottomRadius",            ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, bottomRadius),           0.1f,    100,      10000,  0 },
    { "topRadius",               ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, topRadius),              0.1f,    100,      10000,  0 },
    { "groundAlbedo",            ATTR_COLOR3, (int)offsetof(SkyAtmosphereNodeData, groundAlbedo),           0.01f,   0,        0,      0 },
    { "rayleighScattering",      ATTR_VEC3,   (int)offsetof(SkyAtmosphereNodeData, rayleighScattering),     0.00001f,0,        0,      0 },
    { "rayleighDensityExpScale", ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, rayleighDensityExpScale),0.001f,  -1,       0,      0 },
    { "mieScattering",           ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, mieScattering),          0.0001f, 0,        0.1f,   0 },
    { "mieAbsorption",           ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, mieAbsorption),          0.0001f, 0,        0.1f,   0 },
    { "mieAnisotropy",           ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, mieAnisotropy),          0.01f,   0,        0.99f,  0 },
    { "mieDensityExpScale",      ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, mieDensityExpScale),     0.001f,  -2,       0,      0 },
    { "absorptionExtinction",    ATTR_VEC3,   (int)offsetof(SkyAtmosphereNodeData, absorptionExtinction),   0.00001f,0,        0,      0 },
    { "sunDirection",            ATTR_VEC3,   (int)offsetof(SkyAtmosphereNodeData, sunDirection),           0.01f,   0,        0,      0 },
    { "sunColor",                ATTR_COLOR3, (int)offsetof(SkyAtmosphereNodeData, sunColor),               0.01f,   0,        0,      0 },
    { "sunIntensity",            ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, sunIntensity),           0.1f,    0,        100,    0 },
    { "sunAngularRadius",        ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, sunAngularRadius),       0.0001f, 0.001f,   0.1f,   0 },
    { "worldScale",              ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, worldScale),             0.0001f, 0.00001f, 1.0f,   0 },
    { "exposure",                ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, exposure),               0.1f,    0.01f,    100,    0 },
    { "castShadow",              ATTR_BOOL,   (int)offsetof(SkyAtmosphereNodeData, castShadow),             0,       0,        0,      0 },
    { "shadowResolution",        ATTR_INT,    (int)offsetof(SkyAtmosphereNodeData, shadowResolution),       1.0f,    128,      8192,   0 },
    { "shadowBias",              ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowBias),             0.0001f, 0,        0.5f,   0 },
    { "shadowOrthoSize",         ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowOrthoSize),        0.5f,    1,        5000,   0 },
    { "shadowNear",              ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowNear),             0.1f,    0.1f,     100,    0 },
    { "shadowFar",               ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowFar),              1.0f,    1,        10000,  0 },
    { "shadowPolyFactor",        ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowPolyFactor),       0.05f,   0,        16,     0 },
    { "shadowPolyUnits",         ATTR_FLOAT,  (int)offsetof(SkyAtmosphereNodeData, shadowPolyUnits),        0.1f,    0,        64,     0 },
};

// ---- Fog -------------------------------------------------------------------
static const AttrDesc kFog[] = {
    { "color",   ATTR_COLOR3, (int)offsetof(FogNodeData, color),   0.01f, 0,    0,    0 },
    { "density", ATTR_FLOAT,  (int)offsetof(FogNodeData, density), 0.001f,0,    10,   0 },
    { "start",   ATTR_FLOAT,  (int)offsetof(FogNodeData, start),   1.0f,  0,    10000,0 },
    { "end",     ATTR_FLOAT,  (int)offsetof(FogNodeData, end),     1.0f,  0,    10000,0 },
    { "type",    ATTR_INT,    (int)offsetof(FogNodeData, type),    1.0f,  0,    2,    0 },
    { "enabled", ATTR_BOOL,   (int)offsetof(FogNodeData, enabled), 0,     0,    0,    0 },
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
    { ENTITY_CATMULLROMSPLINE,   "CATMULLROMSPLINE",   "CatmullRomSpline",  kCatmullRom,       NELEM(kCatmullRom),       offsetof(SceneNode, data.catmullrom)       },
    { ENTITY_VOLUMETRIC_CLOUD,   "VOLUMETRIC_CLOUD",   "VolumetricCloud",   kVolumetricCloud,  NELEM(kVolumetricCloud),  offsetof(SceneNode, data.volumetricCloud)  },
    { ENTITY_SKY_ATMOSPHERE,    "SKY_ATMOSPHERE",     "SkyAtmosphere",     kSkyAtmosphere,    NELEM(kSkyAtmosphere),    offsetof(SceneNode, data.skyAtmosphere)    },
    { ENTITY_FOG,               "FOG",                "Fog",               kFog,              NELEM(kFog),             offsetof(SceneNode, data.fog)              },
};
const int g_EntityTableCount = NELEM(g_EntityTable);

const EntityDesc* findEntityDesc(NodeType t) {
    for (int i = 0; i < g_EntityTableCount; i++)
        if (g_EntityTable[i].type == t) return &g_EntityTable[i];
    return nullptr;
}
