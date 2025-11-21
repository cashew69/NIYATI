// Include shared types (boundingRect, Frustum, culling functions)

#define CHUNKSIZE 64
#define INSTANCES 409600
#define CHUNKSCALE 64

bool grassCullingEnabled = false;

GLuint instanceVBO;
GLuint boundsVBO;
GLuint boundsVAO;
GLuint boundsEBO;
Transform** instanceTransforms;
vec3* grass_positions;

struct boundingRect bounds[4096];

void setupGrassInstancing(GLuint grassVAO);
void setupBoundsRendering();


void printBounds()
{
    for (int i = 0; i < PLANE_DEPTH; i++)
    {
        for (int j = 0; j < PLANE_WIDTH; j++)
        {
            int index = i * PLANE_WIDTH + j;

            printf("Chunk (%d, %d) index %d:\n", j, i, index);

            printf("  ox = (%.2f, %.2f, %.2f)\n",
                   bounds[index].ox[0], bounds[index].ox[1], bounds[index].ox[2]);

            printf("  ex = (%.2f, %.2f, %.2f)\n",
                   bounds[index].ex[0], bounds[index].ex[1], bounds[index].ex[2]);

            printf("  oz = (%.2f, %.2f, %.2f)\n",
                   bounds[index].oz[0], bounds[index].oz[1], bounds[index].oz[2]);

            printf("  ez = (%.2f, %.2f, %.2f)\n",
                   bounds[index].ez[0], bounds[index].ez[1], bounds[index].ez[2]);

            printf("\n");
        }
    }
}


void initGrassInstancing()
{
    grass_positions = (vec3*)malloc(INSTANCES * sizeof(vec3));
    instanceTransforms = (Transform**)malloc(INSTANCES * sizeof(Transform*));
    
    srand(time(NULL));

    int grassPerChunk = INSTANCES / (PLANE_DEPTH * PLANE_WIDTH);
    int currentIndex = 0; 
    for (int offsetz = 0; offsetz < PLANE_DEPTH; ++offsetz)
    {
        for (int offsetx = 0; offsetx < PLANE_WIDTH; ++offsetx)
        {
            float spread = (float)CHUNKSIZE;

            for (int i = 0; i < grassPerChunk; i++)
            {
                // Random position within -0.5 to +0.5 of chunk size
                float x = ((float)rand() / RAND_MAX) * spread - (spread * 0.5f);
                float z = ((float)rand() / RAND_MAX) * spread - (spread * 0.5f);

                // Normalize to -0.5 to +0.5, then scale to chunk size (10 units)
                // and offset by chunk position, then CENTER the entire field
                float fieldHalfSize = (PLANE_WIDTH * CHUNKSCALE) * 0.5f; // 320
                x = (x / spread) * CHUNKSCALE + offsetx * CHUNKSCALE - fieldHalfSize;
                z = (z / spread) * CHUNKSCALE + offsetz * CHUNKSCALE - fieldHalfSize;

                grass_positions[currentIndex] = vec3(x, 1.0f, z);  
                currentIndex++; 

            }

            // Calculate bounding box for this 10×10 chunk, centered at origin
            float fieldHalfSize = (PLANE_WIDTH * CHUNKSCALE) * 0.5f; // 320
            float chunkMinX = offsetx * CHUNKSCALE - fieldHalfSize;
            float chunkMinZ = offsetz * CHUNKSCALE - fieldHalfSize;
            float chunkMaxX = chunkMinX + CHUNKSCALE;
            float chunkMaxZ = chunkMinZ + CHUNKSCALE;

            // fill boundingRect for this chunk (y = 0)
            int bIndex = offsetz * PLANE_WIDTH + offsetx;
            if (bIndex >= 0 && bIndex < 4096) {
                bounds[bIndex].ox = vec3(chunkMinX, 0.0f, chunkMinZ); // minX, minZ
                bounds[bIndex].ex = vec3(chunkMaxX, 0.0f, chunkMinZ); // maxX, minZ
                bounds[bIndex].oz = vec3(chunkMinX, 0.0f, chunkMaxZ); // minX, maxZ
                bounds[bIndex].ez = vec3(chunkMaxX, 0.0f, chunkMaxZ); // maxX, maxZ
            }
        }
    }
    printf("Total Grass Blades: %d\n", currentIndex);

    for (int i = 0; i < INSTANCES; i++)
    {
        float randomRot = ((float)rand() / RAND_MAX);
        instanceTransforms[i] = createTransform(
            grass_positions[i], 
            vec3( randomRot * 360.0f, randomRot * 60.0f, -90.0f), 
            vec3(1.0f, 1.0f, 1.0f)
        );
    }

    mat4* instanceMatrices = (mat4*)malloc(INSTANCES * sizeof(mat4));
    for (int i = 0; i < INSTANCES; i++)
    {
        instanceMatrices[i] = getLocalMatrix(instanceTransforms[i]);
    }

    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, INSTANCES * sizeof(mat4), instanceMatrices, GL_STATIC_DRAW);

    free(instanceMatrices);
    
    setupGrassInstancing(sceneMeshes[0].vao);
    setupBoundsRendering();
}

void setupGrassInstancing(GLuint grassVAO)
{
    glBindVertexArray(grassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    for (int i = 0; i < 4; i++) 
    {
        GLuint attribLocation = 4 + i;
        glVertexAttribPointer(attribLocation, 4, GL_FLOAT, GL_FALSE, 
                sizeof(mat4), (void*)(sizeof(vec4) * i));
        glEnableVertexAttribArray(attribLocation);
        glVertexAttribDivisor(attribLocation, 1);
    }


    glBindVertexArray(0);
}

void setupBoundsRendering()
{
    int numChunks = PLANE_DEPTH * PLANE_WIDTH;
    
    // Create vertex data: 4 vertices per bounding quad (just ground-level quads)
    vec3* vertices = (vec3*)malloc(numChunks * 4 * sizeof(vec3));
    
    float quadHeight = 0.1f; // Slightly above ground for visibility
    
    for (int i = 0; i < numChunks; i++)
    {
        // Quad vertices at ground level (4 corners)
        vertices[i * 4 + 0] = vec3(bounds[i].ox[0], quadHeight, bounds[i].ox[2]); // bottom-left
        vertices[i * 4 + 1] = vec3(bounds[i].ex[0], quadHeight, bounds[i].ex[2]); // bottom-right
        vertices[i * 4 + 2] = vec3(bounds[i].ez[0], quadHeight, bounds[i].ez[2]); // top-right
        vertices[i * 4 + 3] = vec3(bounds[i].oz[0], quadHeight, bounds[i].oz[2]); // top-left
    }
    
    // Create index data for quad outline (4 lines forming a rectangle)
    GLuint* indices = (GLuint*)malloc(numChunks * 8 * sizeof(GLuint));
    
    for (int i = 0; i < numChunks; i++)
    {
        GLuint baseVertex = i * 4;
        int idx = i * 8;
        
        // Four lines forming the quad perimeter
        indices[idx + 0] = baseVertex + 0; // bottom-left to bottom-right
        indices[idx + 1] = baseVertex + 1;
        
        indices[idx + 2] = baseVertex + 1; // bottom-right to top-right
        indices[idx + 3] = baseVertex + 2;
        
        indices[idx + 4] = baseVertex + 2; // top-right to top-left
        indices[idx + 5] = baseVertex + 3;
        
        indices[idx + 6] = baseVertex + 3; // top-left to bottom-left
        indices[idx + 7] = baseVertex + 0;
    }
    
    // Generate and setup VAO, VBO, EBO
    glGenVertexArrays(1, &boundsVAO);
    glGenBuffers(1, &boundsVBO);
    glGenBuffers(1, &boundsEBO);
    
    glBindVertexArray(boundsVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, boundsVBO);
    glBufferData(GL_ARRAY_BUFFER, numChunks * 4 * sizeof(vec3), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boundsEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numChunks * 8 * sizeof(GLuint), indices, GL_STATIC_DRAW);
    
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    free(vertices);
    free(indices);
}

void renderBoundingBoxes()
{
    int numChunks = PLANE_DEPTH * PLANE_WIDTH;
    
    mat4 modelMatrix = mat4::identity();
    glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
    
    // Set a bright emissive material so boxes are visible regardless of lighting
    // This assumes your shader has ambient/diffuse/specular/emissive uniforms
    GLint ambientLoc = glGetUniformLocation(mainShaderProgram->id, "material.ambient");
    GLint diffuseLoc = glGetUniformLocation(mainShaderProgram->id, "material.diffuse");
    GLint specularLoc = glGetUniformLocation(mainShaderProgram->id, "material.specular");
    GLint emissiveLoc = glGetUniformLocation(mainShaderProgram->id, "material.emissive");
    
    if (emissiveLoc != -1) {
        glUniform3f(emissiveLoc, 1.0f, 1.0f, 0.0f); // Bright yellow emissive
    }
    if (ambientLoc != -1) {
        glUniform3f(ambientLoc, 1.0f, 1.0f, 0.0f);
    }
    if (diffuseLoc != -1) {
        glUniform3f(diffuseLoc, 0.0f, 0.0f, 0.0f);
    }
    if (specularLoc != -1) {
        glUniform3f(specularLoc, 0.0f, 0.0f, 0.0f);
    }
    
    // Disable depth test to ensure quads are always visible
    glDisable(GL_DEPTH_TEST);
    
    glBindVertexArray(boundsVAO);
    glDrawElements(GL_LINES, numChunks * 8, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void renderVisibleBoundingBoxes()
{
    // Render only the bounding boxes of visible chunks (for debugging culling)
    getFrustum();
    
    int numChunks = PLANE_DEPTH * PLANE_WIDTH;
    
    mat4 modelMatrix = mat4::identity();
    glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
    
    // Set green color for visible chunks
    GLint ambientLoc = glGetUniformLocation(mainShaderProgram->id, "material.ambient");
    GLint diffuseLoc = glGetUniformLocation(mainShaderProgram->id, "material.diffuse");
    GLint specularLoc = glGetUniformLocation(mainShaderProgram->id, "material.specular");
    GLint emissiveLoc = glGetUniformLocation(mainShaderProgram->id, "material.emissive");
    
    if (emissiveLoc != -1) {
        glUniform3f(emissiveLoc, 0.0f, 1.0f, 0.0f); // Bright green for visible
    }
    if (ambientLoc != -1) {
        glUniform3f(ambientLoc, 0.0f, 1.0f, 0.0f);
    }
    if (diffuseLoc != -1) {
        glUniform3f(diffuseLoc, 0.0f, 0.0f, 0.0f);
    }
    if (specularLoc != -1) {
        glUniform3f(specularLoc, 0.0f, 0.0f, 0.0f);
    }
    
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(boundsVAO);
    
    // Draw only visible chunks
    for (int chunkIndex = 0; chunkIndex < numChunks; chunkIndex++)
    {
        if (isChunkVisible(bounds[chunkIndex], viewFrustum))
        {
            // Draw this chunk's bounding box (8 indices starting at chunkIndex * 8)
            glDrawElements(GL_LINES, 8, GL_UNSIGNED_INT, (void*)(chunkIndex * 8 * sizeof(GLuint)));
        }
    }
    
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void renderGrassBlade()
{
    // Get the current frustum
    getFrustum();
    
    mat4 modelMatrix = mat4::identity();
    glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
    setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

    glBindVertexArray(sceneMeshes[0].vao);
    
    if (grassCullingEnabled) {
        // Render with frustum culling
        int numChunks = PLANE_DEPTH * PLANE_WIDTH;
        int grassPerChunk = INSTANCES / numChunks;
        int visibleChunks = 0;
        
        for (int chunkIndex = 0; chunkIndex < numChunks; chunkIndex++)
        {
            if (isChunkVisible(bounds[chunkIndex], viewFrustum))
            {
                int startInstance = chunkIndex * grassPerChunk;
                
                glDrawElementsInstancedBaseInstance(
                    GL_TRIANGLES, 
                    sceneMeshes[0].indexCount, 
                    GL_UNSIGNED_INT, 
                    NULL, 
                    grassPerChunk,
                    startInstance
                );
                
                visibleChunks++;
            }
        }
    } else {
        // Render all grass without culling
        glDrawElementsInstanced(GL_TRIANGLES, sceneMeshes[0].indexCount, 
                               GL_UNSIGNED_INT, NULL, INSTANCES);
    }
    
    glBindVertexArray(0);
}

void toggleCulling()
{
    grassCullingEnabled = !grassCullingEnabled;
    const char* state = grassCullingEnabled ? "ENABLED" : "DISABLED";
    printf("====================================\n");
    printf("   GRASS FRUSTUM CULLING %s   \n", state);
    printf("====================================\n");
    if (!grassCullingEnabled) {
        printf("   → Rendering ALL %d grass instances (no culling)\n", INSTANCES);
    }
}

void updateInstanceTransforms()
{
    mat4* instanceMatrices = (mat4*)malloc(INSTANCES * sizeof(mat4));
    
    for (int i = 0; i < INSTANCES; i++)
    {
        instanceMatrices[i] = getLocalMatrix(instanceTransforms[i]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, INSTANCES * sizeof(mat4), instanceMatrices);
    
    free(instanceMatrices);
}

void cleanupGrassInstances()
{
    for (int i = 0; i < INSTANCES; i++)
    {
        freeTransform(instanceTransforms[i]);
    }
    glDeleteBuffers(1, &instanceVBO);
    glDeleteBuffers(1, &boundsVBO);
    glDeleteBuffers(1, &boundsEBO);
    glDeleteVertexArrays(1, &boundsVAO);
    
    free(grass_positions);
    free(instanceTransforms);
}
