
#define INSTANCES 100000

GLuint instanceVBO;
Transform** instanceTransforms;
vec3* grass_positions;

void setupGrassInstancing(GLuint grassVAO);

void initGrassInstancing()
{
    grass_positions = (vec3*)malloc(INSTANCES * sizeof(vec3));
    instanceTransforms = (Transform**)malloc(INSTANCES * sizeof(Transform*));
    
    srand(time(NULL));
    
    float spread = 500.0f;
    for (int i = 0; i < INSTANCES; i++)
    {
        float x = ((float)rand() / RAND_MAX) * spread - (spread * 0.5f);
        float z = ((float)rand() / RAND_MAX) * spread - (spread * 0.5f);
        grass_positions[i] = vec3(x, 12.0f, z);
    }

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

void renderGrassBlade()
{
    mat4 modelMatrix = mat4::identity();
    glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
    setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

    glBindVertexArray(sceneMeshes[0].vao);
    glDrawElementsInstanced(GL_TRIANGLES, sceneMeshes[0].indexCount, 
            GL_UNSIGNED_INT, NULL, INSTANCES);
    glBindVertexArray(0);
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
    
    free(grass_positions);
    free(instanceTransforms);
}
