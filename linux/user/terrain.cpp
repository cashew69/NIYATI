//#include "../engine/perlin/perlin.c"

// Terrain configuration
#define TERRAIN_WIDTH 64
#define TERRAIN_DEPTH 64
#define VARY_FREQUENCY 0.05f
#define TERRAIN_HEIGHT 10.0f

// Create terrain mesh using generic mesh creation
Mesh* createTerrainMesh() {
    fprintf(gpFile, "Generating terrain mesh...\n");
    
    int vertexCount = TERRAIN_WIDTH * TERRAIN_DEPTH;
    int indexCount = (TERRAIN_WIDTH - 1) * (TERRAIN_DEPTH - 1) * 6;
    
    // Allocate temporary arrays
    float* positions = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* normals = (float*)malloc(vertexCount * 3 * sizeof(float));
    float* texCoords = (float*)malloc(vertexCount * 2 * sizeof(float));
    unsigned int* indices = (unsigned int*)malloc(indexCount * sizeof(unsigned int));
    
    if (!positions || !normals || !texCoords || !indices) {
        fprintf(gpFile, "Error: Failed to allocate memory for terrain data\n");
        free(positions);
        free(normals);
        free(texCoords);
        free(indices);
        return NULL;
    }
    
    // Generate vertices with Perlin noise
    int posIndex = 0;
    int normIndex = 0;
    int texIndex = 0;
    
    for (int z = 0; z < TERRAIN_DEPTH; z++) {
        for (int x = 0; x < TERRAIN_WIDTH; x++) {
            // Position - centered around origin
            positions[posIndex++] = (float)x*10 - TERRAIN_WIDTH / 2.0f;
            printf("VALUE: %f  ", positions[posIndex]);
            printf("INDEX: %d  ", posIndex);
            printf("X VAL: %f | \n", (float)x);
            
            // Height from Perlin noise
            float height = perlinNoise(x * VARY_FREQUENCY, z * VARY_FREQUENCY);
            height = height * TERRAIN_HEIGHT;
            positions[posIndex++] = height;
            
            positions[posIndex++] = (float)z*10 - TERRAIN_DEPTH / 2.0f;
            
            // Texture coordinates
            texCoords[texIndex++] = (float)x / (float)TERRAIN_WIDTH;
            texCoords[texIndex++] = (float)z / (float)TERRAIN_DEPTH;
        }
    }
    
    // Calculate normals using finite differences
    for (int z = 0; z < TERRAIN_DEPTH; z++) {
        for (int x = 0; x < TERRAIN_WIDTH; x++) {
            // Sample nearby heights for normal calculation
            float heightL = (x > 0) ? 
                perlinNoise((x - 1) * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT : 
                perlinNoise(x * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT;
                
            float heightR = (x < TERRAIN_WIDTH - 1) ? 
                perlinNoise((x + 1) * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT : 
                perlinNoise(x * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT;
                
            float heightD = (z > 0) ? 
                perlinNoise(x * VARY_FREQUENCY, (z - 1) * VARY_FREQUENCY) * TERRAIN_HEIGHT : 
                perlinNoise(x * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT;
                
            float heightU = (z < TERRAIN_DEPTH - 1) ? 
                perlinNoise(x * VARY_FREQUENCY, (z + 1) * VARY_FREQUENCY) * TERRAIN_HEIGHT : 
                perlinNoise(x * VARY_FREQUENCY, z * VARY_FREQUENCY) * TERRAIN_HEIGHT;
            
            // normal using cross product
            float nx = heightL - heightR;
            float ny = 2.0f;  // Scale factor for smoothness
            float nz = heightD - heightU;
            
            // Normalize
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) {
                normals[normIndex++] = nx / length;
                normals[normIndex++] = ny / length;
                normals[normIndex++] = nz / length;
            } else {
                normals[normIndex++] = 0.0f;
                normals[normIndex++] = 1.0f;
                normals[normIndex++] = 0.0f;
            }
        }
    }
    
    // Generate indices for triangle mesh
    int indIndex = 0;
    for (int z = 0; z < TERRAIN_DEPTH - 1; z++) {
        for (int x = 0; x < TERRAIN_WIDTH - 1; x++) {
            // vertex indices for quad corners
            int topLeft = z * TERRAIN_WIDTH + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * TERRAIN_WIDTH + x;
            int bottomRight = bottomLeft + 1;
            
            // First triangle (counter-clockwise winding)
            indices[indIndex++] = topLeft;
            indices[indIndex++] = bottomLeft;
            indices[indIndex++] = topRight;
            
            // Second triangle (counter-clockwise winding)
            indices[indIndex++] = topRight;
            indices[indIndex++] = bottomLeft;
            indices[indIndex++] = bottomRight;
        }
    }
    
    // Create vertex data structure
    ModelVertexData data = {0};
    data.positions = positions;
    data.normals = normals;
    data.colors = NULL;  // No vertex colors
    data.texCoords = texCoords;
    data.indices = indices;
    data.vertexCount = vertexCount;
    data.indexCount = indexCount;
    
    // Setup terrain material (grass-like green)
    Material material = {0};
    material.diffuseColor[0] = 0.3f;
    material.diffuseColor[1] = 0.7f;
    material.diffuseColor[2] = 0.3f;
    material.specularColor[0] = 0.2f;
    material.specularColor[1] = 0.2f;
    material.specularColor[2] = 0.2f;
    material.shininess = 16.0f;
    material.opacity = 1.0f;
    material.isEmissive = False;
    material.diffuseTexture = 0;
    material.normalTexture = 0;
    
    loadPNGTexture(&material.diffuseTexture, const_cast<char*>("brainUV.png"), 4,0);
    fprintf(gpFile, "Inside create Terrain Mesh\n");

    fprintf(gpFile, "Called setMaterialUniforms\n");
    // USE GENERIC MESH CREATION
    Mesh* terrain = createMesh(&data, &material);
    
    // Free temporary arrays
    free(positions);
    free(normals);
    free(texCoords);
    free(indices);
    
    if (terrain) {
        fprintf(gpFile, "Terrain mesh created successfully: %d vertices, %d indices\n", 
                vertexCount, indexCount);
    } else {
        fprintf(gpFile, "Error: Failed to create terrain mesh\n");
    }
    
    return terrain;
}

void renderTerrain()
{
// RENDER TERRAIN MESH
        if (terrainMesh != NULL) {
            mat4 modelMatrix = mat4::identity(); // Terrain at origin
            
            if (modelLocUniform != -1) {
                glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
            }
            
            setMaterialUniforms(mainShaderProgram, &terrainMesh->material);
            glBindVertexArray(terrainMesh->vao);
            
            if (terrainMesh->ibo && terrainMesh->indexCount > 0) {
                glDrawElements(GL_TRIANGLES, terrainMesh->indexCount, GL_UNSIGNED_INT, NULL);
            }
            
            glBindVertexArray(0);
        }

}
