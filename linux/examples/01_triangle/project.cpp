
// ============================================================================
// Example 01: Simple Triangle
// Custom inline GLSL shaders. Uses engine's createVAO_VBO for buffer setup.
// ============================================================================

static ShaderProgram* shaderProgram = NULL;
static GLuint triangleVAO      = 0;
static GLuint triangleVBO_pos  = 0;
static GLuint triangleVBO_col  = 0;

float   rotationAngle = 0.0f;

void projectInit()
{
    camera_pos = vec3(0.0f, 0.0f, 5.0f);

        // ---- Shaders ----
        static const char* vertSrc = R"(
    #version 460 core
    layout(location = 0) in vec3 aPosition;
    layout(location = 2) in vec3 aColor;

    out vec3 vColor;
    uniform mat4 uMVPMatrix;

    void main() {
        vColor = aColor;
        gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
    }
    )";

        static const char* fragSrc = R"(
    #version 460 core
    in  vec3 vColor;
    out vec4 FragColor;

    void main() {
        FragColor = vec4(vColor, 1.0);
    }
    )";

    const GLchar* sources[2] = { vertSrc, fragSrc };
    GLenum        types[2]   = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
    if (!buildShaderProgram(sources, types, 2, &shaderProgram, attribNames, attribIndices, 4)) {
        fprintf(gpFile, "Failed to build triangle shader\n");
        return;
    }
    shaderProgram->name = "Triangle";

    // ---- Geometry — separate arrays for createVAO_VBO ----
    static float positions[] = {
         0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
    };
    static float colors[] = {
        1.0f, 0.2f, 0.3f,   // top    — red
        0.2f, 1.0f, 0.3f,   // left   — green
        0.3f, 0.2f, 1.0f,   // right  — blue
    };

    ModelVertexData data = {};
    data.positions   = positions;
    data.colors      = colors;
    data.vertexCount = 3;

    createVAO_VBO(&triangleVAO, &triangleVBO_pos, NULL, &triangleVBO_col, NULL, NULL, &data);

    fprintf(gpFile, "Project 01 (Triangle) initialized\n");
}

void projectRender()
{
    if (!shaderProgram) return;

    viewMatrix = vmath::lookat(camera_pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(shaderProgram->id);

    mat4 mvp = perspectiveProjectionMatrix * viewMatrix
             * vmath::rotate(rotationAngle, 0.0f, 1.0f, 0.0f);

    // Default glUniformStuff.
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram->id, "uMVPMatrix"), 1, GL_FALSE, mvp);


    // Currently not planned to abstract glDrawArrays though can be done. 
    // I need more control so probably not a good idea.
    glBindVertexArray(triangleVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void projectUpdate()  { 
    rotationAngle += 0.5f;
    if (rotationAngle >= 360.0f)
        rotationAngle = 0.0f;
}
void projectCleanup()
{

    // similar to createVAO_VBO, I should probably have a destroyVAO_VBO soon.
    if (triangleVAO)     glDeleteVertexArrays(1, &triangleVAO);
    if (triangleVBO_pos) glDeleteBuffers(1, &triangleVBO_pos);
    if (triangleVBO_col) glDeleteBuffers(1, &triangleVBO_col);
    if (shaderProgram)   freeThyShaderProgram(shaderProgram);
}
