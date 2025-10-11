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
    GLuint diffuseTexture; // Renamed from texture
    GLuint normalTexture;  // For normal mapping
    float opacity;
} Material;

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
    float modelMatrix[16];
    char* userFragmentCode;
} Mesh;

typedef struct {
    GLuint id;
    GLenum type;
    const char* name;
} Shader;

typedef struct {

} Shaders;

typedef struct {
    GLuint id;
    Shader* shaders[6];
    int shaderCount;
} ShaderProgram;

