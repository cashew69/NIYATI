#pragma once
#include <GL/glew.h>
#include "engine/dependancies/vmath.h"
#include "engine/core/gl/camera.h"
struct Transform;


#ifndef Bool
typedef int Bool;
#define True 1
#define False 0
#endif


// Model vertex data structure
typedef struct {
    float* positions;    // Array of vec3 (x, y, z)
    float* normals;      // Array of vec3 (nx, ny, nz), NULL if not provided
    float* colors;       // Array of vec3 (r, g, b), NULL if not provided
    float* texCoords;    // Array of vec2 (u, v), NULL if not provided
    unsigned int* indices;// Array of indices, NULL if not indexed
    int vertexCount;     // Number of vertices
    int indexCount;      // Number of indices (0 if not indexed)
} ModelVertexData;

typedef struct {
    Bool isEmissive;
    float diffuseColor[3];
    float specularColor[3];
    float shininess;
    float roughness;
    float metalness;
    GLuint diffuseTexture; // Renamed from texture
    GLuint normalTexture;  // For normal mapping
    GLuint metallicRoughnessTexture; // glTF standard ORM (Occlusion, Roughness, Metallic)
    GLuint aoTexture;      // Separate AO if not in ORM
    GLuint emissiveTexture;
    float opacity;

    // Texture toggles
    Bool useDiffuseTexture;
    Bool useNormalTexture;
    Bool useMetallicRoughnessTexture;
    Bool useAOTexture;
    Bool useEmissiveTexture;
} Material;

// Axis-aligned bounding box (object-space when stored on a Mesh)
typedef struct AABB {
    vec3 min;
    vec3 max;
} AABB;

// Mesh structure
typedef struct {
    GLuint vao;
    GLuint vbo_position;
    GLuint vbo_normal;
    GLuint vbo_color;
    GLuint vbo_texcoord;
    GLuint ibo;
    size_t indexCount;
    Material material;
    struct Transform* transform;
    char name[64];
    AABB aabbLocal;
} Mesh;

typedef struct {
    Mesh* meshes;
    int meshCount;
    char name[64];
    char filePath[256];
    struct Transform* transform;
} Model;

typedef struct {
    GLuint id;
    GLenum type;
    const char* name;
} Shader;

typedef struct {
    const GLchar* textData;
} shaderSource;

typedef struct {
    // Core matrices
    GLint uModel;
    GLint uView;
    GLint uProjection;
    // Lighting
    GLint uViewPos;
    GLint uLightPos;
    GLint uLightColor;
    GLint uLightIntensity; // New
    GLint uLightType;
    GLint uLightDir;
    GLint uLightRadius;
    GLint uInnerCutoff;
    GLint uOuterCutoff;
    // IBL
    GLint uHasIBL;
    GLint uIBLIntensity;
    GLint irradianceMap;
    GLint prefilterMap;
    GLint brdfLUT;
    // Material — base values
    GLint uDiffuseColor;
    GLint uHasDiffuseTexture;
    GLint uDiffuseTexture;
    GLint uHasNormalTexture;
    GLint uNormalTexture;
    GLint uHasMetallicMap;
    GLint uMetallicMap;
    GLint uHasRoughnessMap;
    GLint uRoughnessMap;
    GLint uHasAOMap;
    GLint uAOMap;
    GLint uHasEmissiveMap;
    GLint uEmissiveMap;
    GLint uRoughness;
    GLint uMetalness;
    GLint uShininess;
    GLint uOpacity;
    GLint uIsEmissive;
    // Terrain
    GLint uHeightMap;
    GLint uDisplacementMap;
    GLint uHasDisplacementMap;
    GLint uDisplacementScale;
    GLint uTexelSize;
    GLint uUVScale;
    GLint uTessLevelInner;
    GLint uTessLevelOuter;
    // Line shader (unprefixed names)
    GLint view;
    GLint projection;
    GLint model;

    // Fog
    GLint uFogColor;
    GLint uFogDensity;
    GLint uFogStart;
    GLint uFogEnd;
    GLint uFogType;
    GLint uFogEnabled;
    // Shadow
    GLint uShadowMap;
    GLint uShadowMatrix;
    GLint uShadowEnabled;
    GLint uShadowBias;
    GLint uShadowMinLight;
    GLint uUseOrenNayar;
    // Aerial perspective
    GLint uAerialPerspective;
    GLint uAerialTransmittanceLUT;
    GLint uAerialSkyViewLUT;
    GLint uAtmBotR;
    GLint uAtmTopR;
    GLint uAtmCamHeight;
    GLint uAtmWorldScale;
    GLint uAtmExposure;

    // NEW FIELDS (Added at end)
    GLint uEnableStochastic;
    GLint uStochasticContrast;
    GLint uStochasticScale;

    // Terrain overlay (footprint normal map)
    GLint uOverlayTexture;
    GLint uHasOverlayTexture;
} ShaderLocations;

typedef struct {
    GLuint id;
    Shader* shaders[6];
    int shaderCount;
    const char* name;
    ShaderLocations loc;
} ShaderProgram;

typedef struct {
    GLint location;
    ShaderProgram program;
    char *varInShader;
    int count;
} Uniform;

typedef struct {
    GLuint         fboID;
    GLuint         depthTexID;
    ShaderProgram* depthProgram;
    GLint          locDepthMVP;
    int            resolution;
    float          orthoSize;
    float          nearPlane;
    float          farPlane;
    float          bias;
    float          polyOffsetFactor; // slope-scale for glPolygonOffset
    float          polyOffsetUnits;  // constant units for glPolygonOffset
    mat4           lightView;
    mat4           lightProj;
    mat4           sbpv;       // scale_bias * lightProj * lightView
    vec3           debugEye;   // last computed light-eye position
    vec3           debugTarget;// last computed light-target position
} ShadowMap;

struct boundingRect
{
    vec3 ox;
    vec3 ex;
    vec3 oz;
    vec3 ez;

};
struct Plane {
    vec3 normal;
    float distance;
};

struct cullfrustum {
    Plane planes[6];  // LEFT, RIGHT, BOTTOM, TOP, NEAR, FAR
};


typedef enum {
    INSTANCE_PATTERN_GRID,
    INSTANCE_PATTERN_PERLIN_NOISE
} InstancePattern;

typedef struct {
    char modelPath[256];
    GLuint instanceVBO;
    int instanceCount;
    InstancePattern pattern;
    int gridCountX;
    int gridCountZ;
    float spacingX;
    float spacingZ;
    float noiseScale;
    float noiseThreshold;
    float areaWidth;
    float areaDepth;
    float minScale;
    float maxScale;
    float randomYRotation;
    mat4* instanceMatrices;
    int matricesCapacity;
    Mesh* instanceMeshes;
    int instanceMeshCount;
    AABB clusterAABB;
    AABB* instanceLocalBounds; // Cached local-space AABBs for each instance
    int* visibleIndices;
    int visibleCount;
    int visibleCapacity;
} InstanceData;

typedef struct {
    // Noise/Generation
    int   octaves;
    float persistence;
    float lacunarity;
    int   seed;
    float mountainThreshold;
    float desertThreshold;
    float mountainHeightScale;
    float desertHeightScale;
    float plainsHeightScale;
    bool  useIslandMask;
    float islandFalloff;
    int   biomeMode;
    float ridgeStrength;
    float turbulence;
    int   terraceLevels;
    float powerCurve;
    float heightOffset;

    // Mesh/Rendering
    int   meshWidth;
    int   meshDepth;
    float worldScale;
    int   tessInner;
    int   tessOuter;
    float displacementScale;
    int   lodBias;
    bool  wireframe;

    // Textures/Material
    int    heightmapSource; // 0: Generated, 1: File
    char   heightmapPath[256];
    GLuint heightmapTex;
    GLuint normalMapTex;
    GLuint displacementMapTex;
    int    materialIndex; // 0..N for presets, -1 for custom
    float  uvScale;
    float  roughness;
    float  metalness;

    // Toggles
    bool enableDiffuse;
    bool enableNormal;
    bool enableARM;
    bool enableDisplacement;

    // Runtime mesh
    Mesh* mesh;
    float* cpuHeightMap;
    int cpuHeightMapWidth;
    int cpuHeightMapHeight;

    // NEW FIELDS (Added at end to preserve offsets of critical pointers above)
    bool  enableStochastic;
    float stochasticContrast;
    float stochasticScale;
    char  diffusePath[256];
    char  normalPath[256];
    char  armPath[256];
    char  dispPath[256];
} TerrainNodeData;

typedef struct {
    char hdrPath[256];
    int  currentPreset;
} SkyboxNodeData;

typedef struct {
    // Lighting
    vec3  sunDirection;
    vec3  sunColor;
    float sunIntensity;
    float ambientStrength;
    float scatterG;

    // Appearance — the big levers for realistic vs cartoony look
    vec3  cloudColorTop;    // lit/sun-facing color  (default: near-white)
    vec3  cloudColorBottom; // shadowed underside color (default: blue-gray)
    float absorption;       // cloud opacity/thickness (0.05 = thin, 0.3 = thick storm)
    float coverage;         // coverage bias: neg=less cloud, pos=more cloud
    float erosion;          // detail erosion strength (0.3 solid, 0.9 wispy edges)
    float silverLining;     // back-lit edge glow intensity (0 = none, 1 = strong)
    bool  flatBottom;       // unused (height profile now handles flat base)
    float cloudType;        // 0=stratus, 0.5=stratocumulus, 1.0=cumulus
    float noiseScale;       // base shape noise frequency (default 0.006)
    float detailScale;      // edge erosion noise frequency (default 0.035)
    int   noiseRes;         // resolution of the 3D noise texture (e.g. 128)
    char  noiseBaseTexPath[256];
    char  noiseDetailTexPath[256];

    // Raymarching
    float densityScale;
    int   maxSteps;
    float stepSize;

    // Noise / wind
    float turbulence;
    float windSpeed;        // global drift: slides base noise + NVDF
    float localNoiseSpeed;  // local churn: animates detail/curl noise independently

    // Bounding volume (centred on node position)
    vec3  boxSize;

    // Sphere grid generation
    int   gridX;
    int   gridZ;
    float gridSpacing;
    float gridScale;
    int   spheresPerCloudMin;
    int   spheresPerCloudMax;

    // Rendering
    float renderScale;

    // Temporal anti-aliasing
    bool  enableTAA;
    float taaBlend;

    // NVDF (pre-baked 3D density texture) — HZD-style sampling path
    bool  useNVDF;          // when true, raymarcher samples nvdfTex instead of full Nubis eval
    char  nvdfPath[256];    // path to .nvdf file (relative to project root)
    float hwModStrength;    // height-width modulation amount (0..1) — anti-repeat
    float hwModScale;       // frequency of HW modulation field
    float curlStrength;     // high-freq "curly alligator" erosion strength
    float nvdfTileScale;    // XZ tile rate: 1 / worldMetersPerTile
    float nvdfRotAngle;     // texture rotation around Y axis (radians) — "Y spin"
    float nvdfRotX;         // texture tilt around X axis (radians)
    float nvdfRotZ;         // texture tilt around Z axis (radians)
    vec3  nvdfWorldOffset;  // XZ slide in world space (nvdfWorldOffset.xz used; y unused)
    float nvdfYOffset;      // Y texture slide: shifts which height-slice maps to the cloud layer

    // Circle field — masks the cloud volume to a cylinder in XZ
    bool  useCircleField;   // replaces rectangular XZ boundary with a circular one
    float circleRadius;     // world-space XZ radius from the box centre

    // Sphere field — wraps clouds around the viewer as a spherical shell
    bool  useSphereField;
    float planetRadius;     // shell curvature radius (bigger = flatter horizon)
    float cloudBaseHeight;  // world Y where cloud layer starts
    float cloudThickness;   // outer shell - inner shell
    float domeExtent;       // 1.0 = full sphere, 0.5 = hemisphere

    // Weather map — RGBA texture: R=coverage, G=precipitation, B=cloud type, A=height scale
    bool   useWeatherMap;
    char   weatherMapPath[256];
    float  weatherMapScale;
    GLuint weatherMapTex;    // runtime only — not serialised
    // Auto-generated procedural weather map (noise-based, covers world once)
    bool   autoWeatherMap;       // when true: generate noise map, ignore weatherMapPath
    float  weatherMapGridExtent; // extent to cover (0 = auto from box/sphere size)

    // Weather map generator parameters (used when autoWeatherMap is true)
    struct {
        int   patternType;    // 0=FBM Noise 1=Spiral 2=Cyclone 3=Bands 4=Cellular
        float centerX;        // pattern center in UV space
        float centerY;
        int   arms;           // spiral/cyclone arm count
        float tightness;      // spiral winding rate (radians per UV radius)
        float falloffRadius;  // outer envelope radius (UV units)
        float bandAngle;      // band direction in radians
        float bandWidth;      // cloud fraction of band period (0-1)
        float bandSpacing;    // band period in UV units
        float bandTurbulence; // turbulence displacement strength
        float noiseFreq;      // frequency multiplier for noise/cellular
        float coverageScale;  // global coverage density (0-1)
        float coverageMin;    // remap dark end of coverage output (0 = no remap)
        float coverageMax;    // remap bright end of coverage output (1 = no remap)
        int   texResolution;  // 0=256  1=512  2=1024
        float worldAnchorX;   // world XZ of map centre (used when !followCamera)
        float worldAnchorZ;
        bool  followCamera;   // true = anchor moves with camera (sphere default)
    } weatherGen;

    // Noise layer toggles
    bool  useBaseNoise;      // false = coverage IS density (no cellular grid)
    bool  useWorleyErosion;  // false = skip Worley GBA surface erosion
    bool  useDetailNoise;    // false = skip high-freq detail texture erosion

    // Aerial perspective / atmospheric fog
    vec3  fogColor;
    float fogDensity;        // 0 = disabled
    float fogStart;          // metres before fog kicks in

    // Scene-depth occlusion culling
    bool  useSceneDepth;     // early-exit when terrain/geometry occludes cloud entry

    // Adaptive raymarching
    float adaptiveFactor;      // empty-step grows by (tNear+t) * this; 0 = fixed step
    float jitterSwitchDist;    // below = animated jitter, above = static (no shimmer)

    // Dual-pass split (near at low-res, far at high-res)
    bool  useDualPass;         // enables two dispatches per frame
    float nearFarSplit;        // metres — near pass covers [0, split), far covers [split, ∞)
    int   nearOutputW, nearOutputH;  // resolution of near pass (default 480x270)
    int   farOutputW,  farOutputH;   // resolution of far  pass (default 960x540)

    // Cached uniform locations for the compute program (filled at init)
    struct {
        GLint cameraPos, view, proj, nonJitteredProj, invView, worldMatrix, time;
        GLint sphereCount, densityScale, maxSteps, stepSize, turbulence, windSpeed, localNoiseSpeed;
        GLint boxMin, boxMax;
        GLint sunDir, sunColor, sunIntensity, ambientStrength, scatterG;
        GLint cloudColorTop, cloudColorBottom, absorption, coverage, erosion;
        GLint silverLining, flatBottom;
        GLint cloudType, noiseScale, detailScale, noiseBase, noiseDetail;
        GLint enableTAA, frameIndex, taaBlend;
        GLint prevCameraPos, prevView, prevProj, historyTex;
        GLint useNVDF, nvdfTex, hwModStrength, hwModScale, curlStrength, nvdfTileScale;
        GLint nvdfWorldOffset, nvdfRotMat, nvdfYOffset;
        GLint adaptiveFactor, jitterSwitchDist;
        GLint passMode, nearFarSplit;
        GLint useCircleField, circleRadius;
        GLint useSphereField, planetRadius, cloudBaseHeight, cloudThickness, domeExtent;
        GLint blueNoise;
        GLint useWeatherMap, weatherMap, weatherMapScale, autoWeatherMap;
        GLint weatherMapAnchor, weatherMapGridExtent;
        GLint useBaseNoise, useWorleyErosion, useDetailNoise;
        GLint fogColor, fogDensity, fogStart;
        GLint useSceneDepth, sceneDepth;
    } computeLoc;

    // Runtime GPU resources (not serialised)
    GLuint outputTex;     // combined (single pass) or near pass (dual pass)
    GLuint historyTex;    // ping-pong for TAA — near pass
    GLuint farOutputTex;  // far pass output  (only used when useDualPass)
    GLuint farHistoryTex; // far pass TAA history
    GLuint sphereSSBO;
    GLuint nvdfTex;       // 3D R8 texture loaded from .nvdf file
    GLuint noiseBaseTex;  // 3D RGBA8 base noise
    GLuint noiseDetailTex;// 3D RGB8 detail noise
    int    outputW;
    int    outputH;
    int    farFrameIndex; // separate TAA counter for far pass
    int    frameIndex;    // incremented each rendered frame

    // CPU sphere data (heap-allocated, local-space)
    float* sphereData;
    int    sphereCount;
    bool   spheresDirty;
} VolumetricCloudNodeData;

typedef struct {
    // Atmosphere parameters (km units)
    float bottomRadius;
    float topRadius;
    vec3  groundAlbedo;

    // Rayleigh
    vec3  rayleighScattering;       // per km
    float rayleighDensityExpScale;  // negative, default -0.125

    // Mie
    float mieScattering;            // per km
    float mieAbsorption;            // per km
    float mieAnisotropy;
    float mieDensityExpScale;       // default -0.8333

    // Ozone
    vec3  absorptionExtinction;     // per km

    // Sun
    vec3  sunDirection;             // world-space toward sun (normalized)
    vec3  sunColor;
    float sunIntensity;
    float sunAngularRadius;         // radians half-angle, default 0.00872

    // Scene
    float worldScale;               // 1 world unit = worldScale km, default 0.001
    float exposure;                 // linear multiplier before gamma, default 10.0

    // GPU (not serialized)
    GLuint transmittanceLUT;        // 256x64 RGBA16F
    GLuint multiScatterLUT;         // 32x32 RGBA16F
    GLuint skyViewLUT;              // 192x108 RGBA16F
    GLuint atmosphereCubemap;       // 64x64 cubemap baked from sky-view LUT for IBL
    GLuint emptyVAO;
    GLuint transmittanceProg;       // compute
    GLuint multiScatterProg;        // compute
    GLuint skyViewProg;             // compute
    GLuint bakeToCubemapProg;       // compute
    GLuint quadProg;                // fullscreen vert+frag
    GLint  quadSkyViewLoc;
    GLint  quadTransmittanceLoc;
    GLint  quadInvViewProjLoc;
    GLint  quadCamPosLoc;
    GLint  quadSunDirLoc;
    GLint  quadSunColorLoc;
    GLint  quadSunIntensityLoc;
    GLint  quadSunRadiusLoc;
    GLint  quadExposureLoc;
    GLint  quadBotRLoc;
    GLint  quadTopRLoc;
    GLint  quadCamHeightLoc;
    bool   lutsDirty;
    int    iblFrameCounter;
    vec3   prevIBLSunDir;   // sun dir at last IBL bake; zero = "never baked"

    // Shadow — sun acts as directional shadow light
    bool  castShadow;
    int   shadowResolution;
    float shadowBias;
    float shadowOrthoSize;
    float shadowNear;
    float shadowFar;
    float shadowPolyFactor;
    float shadowPolyUnits;

    // Runtime state (not serialized)
    ShadowMap* shadow;
} SkyAtmosphereNodeData;

typedef struct {
    vec3* controlPoints;
    int pointCount;
    int pointCapacity;
    float tension;
    int segmentsPerCurve;
    bool isLooping;
    vec3* curvePoints;
    int curvePointCount;
    int curvePointCapacity;
    GLuint vao;
    GLuint vbo;
    bool showControlPoints;
    vec3 color;
} CatmullRomNodeData;


typedef struct {
    vec3  color;
    float density;
    float start;
    float end;
    int   type; // 0: Linear, 1: Exp, 2: Exp2
    bool  enabled;
} FogNodeData;

typedef struct {
    // Wave (serialized)
    float waveHeight;
    float waveSpeed;
    float waveRadius;
    float wavePointiness;
    float stormIntensity;   // 0..1 — multiplies toward storm on top of base params

    // Colors (serialized)
    float deepColor[3];
    float shallowColor[3];
    float foamColor[3];

    // Material (serialized)
    float roughness;
    float fresnelF0;
    float foamStrength;

    // Texture path (serialized)
    char  normalMapPath[256];

    // Runtime only — not serialized
    GLuint normalMapTex;
    void*  oceanObj;       // Ocean* — heap-allocated on first draw
} OceanNodeData;

typedef enum {
    ENTITY_EMPTY,
    ENTITY_MODEL,
    ENTITY_LIGHT,
    ENTITY_CAMERA,
    ENTITY_INSTANCE,
    ENTITY_TERRAIN,
    ENTITY_SKYBOX,
    ENTITY_CATMULLROMSPLINE,
    ENTITY_VOLUMETRIC_CLOUD,
    ENTITY_SKY_ATMOSPHERE,
    ENTITY_FOG,
    ENTITY_OCEAN
} NodeType;

typedef enum {
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT       = 1,
    LIGHT_SPOT        = 2
} LightType;

typedef struct {
    int   type;         // 0: Dir, 1: Point, 2: Spot
    vec3  color;
    float intensity;
    float radius;       // Range for Point/Spot
    vec3  direction;    // For Directional/Spot
    float innerCutoff;  // Cosine of inner angle for Spot
    float outerCutoff;  // Cosine of outer angle for Spot
    
    // Shadow settings (serialized)
    bool  castShadow;
    int   shadowResolution;
    float shadowBias;
    float shadowOrthoSize;
    float shadowNear;
    float shadowFar;
    float shadowPolyFactor;
    float shadowPolyUnits;

    // Runtime state (not serialized)
    ShadowMap* shadow;
} LightData;

// Camera is defined in camera.h (included above).

typedef enum RenderOrderCondition {
    RENDER_COND_ALWAYS = 0,
    RENDER_COND_CAMERA_ABOVE_Y,
    RENDER_COND_CAMERA_BELOW_Y,
    RENDER_COND_CAMERA_NEAR,
    RENDER_COND_CAMERA_FAR,
} RenderOrderCondition;

typedef struct RenderOrderRule {
    bool                 enabled;
    RenderOrderCondition condition;
    float                threshold;
    int                  targetIndex;  // render position when fired; 0=first, -1=last
} RenderOrderRule;

typedef struct SceneNode {
    vec3 position;
    vec3 rotation_euler;
    vec3 scale;

    mat4 local_matrix;
    mat4 world_matrix;

    struct SceneNode  *parent;
    struct SceneNode **children;
    int                num_children;
    int                capacity_children;

    NodeType type;
    union {
        Mesh mesh;
        Camera camera;
        LightData light;
        InstanceData instance;
        TerrainNodeData terrain;
        SkyboxNodeData skybox;
        CatmullRomNodeData catmullrom;
        VolumetricCloudNodeData volumetricCloud;
        SkyAtmosphereNodeData   skyAtmosphere;
        FogNodeData             fog;
        OceanNodeData           ocean;
    } data;

    const char *name;
    char sourcePath[256];
    int meshIndex;
    int ID;
    bool terrainYOffset;
    int selectedTerrainID;
    float pathProgress;
    RenderOrderRule renderRule;
} SceneNode;
