#pragma once
// Forward declaration
//
#include "engine.h"
#include "camera.h"
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
    int    materialIndex;
    float  uvScale;

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
} TerrainNodeData;

typedef struct {
    char hdrPath[256];
    int  currentPreset;
} SkyboxNodeData;

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


typedef enum {
    ENTITY_EMPTY,
    ENTITY_MODEL,
    ENTITY_LIGHT,
    ENTITY_CAMERA,
    ENTITY_INSTANCE,
    ENTITY_TERRAIN,
    ENTITY_SKYBOX,
    ENTITY_CATMULLROMSPLINE
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
    bool  cast_shadows;
} LightData;

// Camera is defined in camera.h (included above).

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
    } data;

    const char *name;
    char sourcePath[256];
    int meshIndex;
    int ID;
    bool terrainYOffset;
    int selectedTerrainID;
    float pathProgress;
} SceneNode;
