
// ============================================================================
// Example 02: 3D Pyramid
// using shaders from glsl files and lighting, little more complex than ex 01
// ============================================================================

static ShaderProgram* pyramidShader = NULL;
static GLuint         pyramidVAO    = 0;
static GLuint         pyramidPosBuf = 0;
static GLuint         pyramidNormBuf = 0;

float rotationAngle = 0.0f;

static GLfloat pyramidVertices[] =
{
    // front
     0.0f,  1.0f,  0.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    // right
     0.0f,  1.0f,  0.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
    // back
     0.0f,  1.0f,  0.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    // left
     0.0f,  1.0f,  0.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
};

static GLfloat pyramidNormals[] =
{
    // front
     0.000000f, 0.447214f,  0.894427f,
     0.000000f, 0.447214f,  0.894427f,
     0.000000f, 0.447214f,  0.894427f,
    // right
     0.894427f, 0.447214f,  0.000000f,
     0.894427f, 0.447214f,  0.000000f,
     0.894427f, 0.447214f,  0.000000f,
    // back
     0.000000f, 0.447214f, -0.894427f,
     0.000000f, 0.447214f, -0.894427f,
     0.000000f, 0.447214f, -0.894427f,
    // left
    -0.894427f, 0.447214f,  0.000000f,
    -0.894427f, 0.447214f,  0.000000f,
    -0.894427f, 0.447214f,  0.000000f,
};


void projectInit()
{
    camera_pos = vec3(0.0f, 2.0f, 6.0f);

    // ---- Shaders from file ----
    const char *shaderFiles[5] = {
        "engine/shaders/vertex_shader.glsl",  // vert
        NULL, NULL, NULL,                      // no tcs/tes/geo
        "engine/shaders/main_fs[lambart].glsl" // frag
    };
    if (!buildShaderProgramFromFiles(shaderFiles, 5, &pyramidShader, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build pyramid shader\n");
        return;
    }
    pyramidShader->name = "Pyramid";

    // ---- Geometry via createVAO_VBO ----
    ModelVertexData data = {};
    data.positions   = pyramidVertices;
    data.normals     = pyramidNormals;
    data.vertexCount = 12;   // 4 faces × 3 verts

    createVAO_VBO(&pyramidVAO, &pyramidPosBuf, &pyramidNormBuf, NULL, NULL, NULL, &data);

    fprintf(gpFile, "Project 02 (Pyramid) initialized\n");
}

void setPyramidUniforms()
{
    glUniform3f(glGetUniformLocation(pyramidShader->id, "uDiffuseColor"),  0.85f, 0.65f, 0.13f); // gold
    glUniform3f(glGetUniformLocation(pyramidShader->id, "uSpecularColor"), 1.0f,  1.0f,  1.0f);
    glUniform1f(glGetUniformLocation(pyramidShader->id, "uShininess"),     64.0f);
    glUniform3f(glGetUniformLocation(pyramidShader->id, "uLightPos"),      5.0f,  10.0f, 5.0f);
    glUniform3f(glGetUniformLocation(pyramidShader->id, "uLightColor"),    1.0f,  1.0f,  1.0f);
    glUniform3fv(glGetUniformLocation(pyramidShader->id, "uViewPos"),   1, camera_pos);
    glUniform1i(glGetUniformLocation(pyramidShader->id, "uHasDiffuseTexture"), 0);
    glUniform1i(glGetUniformLocation(pyramidShader->id, "uIsEmissive"),        0);
    glUniform1f(glGetUniformLocation(pyramidShader->id, "uOpacity"),           1.0f);
}

void projectRender()
{
    if (!pyramidShader) return;

    // Compute view matrix — camera at camera_pos, looking at origin
    viewMatrix = vmath::lookat(camera_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(pyramidShader->id);

    mat4 model = vmath::rotate(rotationAngle, 0.0f, 1.0f, 0.0f);

    // Here i am using getUniformLocation instead of glGet, i made this, 
    // but i don't find it to be worth abstracting, shall we use
    // glGetUniformLocation() instead of getUniformLocation()?
    glUniformMatrix4fv(getUniformLocation(pyramidShader, "uProjection"), 1, GL_FALSE, perspectiveProjectionMatrix);
    glUniformMatrix4fv(getUniformLocation(pyramidShader, "uView"),       1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(getUniformLocation(pyramidShader, "uModel"),      1, GL_FALSE, model);

    setPyramidUniforms();
    

    glBindVertexArray(pyramidVAO);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    glBindVertexArray(0);
    glUseProgram(0);
}

void projectUpdate()
{
    rotationAngle += 0.5f;
    if (rotationAngle >= 360.0f) rotationAngle = 0.0f;
}

void projectCleanup()
{
    if (pyramidVAO)     glDeleteVertexArrays(1, &pyramidVAO);
    if (pyramidPosBuf)  glDeleteBuffers(1, &pyramidPosBuf);
    if (pyramidNormBuf) glDeleteBuffers(1, &pyramidNormBuf);
    if (pyramidShader)  freeThyShaderProgram(pyramidShader);
}
