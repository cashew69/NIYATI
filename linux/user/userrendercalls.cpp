#include "userrenderimports.h"

void setCommonUniformsForCurrentProgram(ShaderProgram* program, const mat4 &proj, const mat4 &view, 
        vec3 lightPos, vec3 lightColor, vec3 viewPos)
{
    GLint loc;
    loc = glGetUniformLocation(program->id, "uProjection");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, proj);

    loc = glGetUniformLocation(program->id, "uView");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, view);

    loc = glGetUniformLocation(program->id, "uLightPos");
    if (loc != -1) glUniform3fv(loc, 1, lightPos);

    loc = glGetUniformLocation(program->id, "uLightColor");
    if (loc != -1) glUniform3fv(loc, 1, lightColor);

    loc = glGetUniformLocation(program->id, "uViewPos");
    if (loc != -1) glUniform3fv(loc, 1, viewPos);
}

// Light settings
vec3 lightPos = vec3(100.0f, 100.0f, 100.0f);
vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
vec3 viewPos = vec3(0.0f, 50.0f, 150.0f);


void renderUserMeshes(GLint HeightMap)
{

    getFrustum();
    if (mainShaderProgram && mainShaderProgram->id) 
    {
        glUseProgram(mainShaderProgram->id);
        setCommonUniformsForCurrentProgram(mainShaderProgram, perspectiveProjectionMatrix, viewMatrix, lightPos, lightColor, viewPos);

        renderGrassBlade();
    }
    
    if (lineShaderProgram && lineShaderProgram->id)
{
    glUseProgram(lineShaderProgram->id);
    
    // Set uniforms for line shader
    GLint modelLoc = glGetUniformLocation(lineShaderProgram->id, "model");
    GLint viewLoc = glGetUniformLocation(lineShaderProgram->id, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram->id, "projection");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram->id, "lineColor");
    
    mat4 modelMatrix = mat4::identity();
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMatrix);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, perspectiveProjectionMatrix);
    glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f); // Yellow color
    
    renderBoundingBoxes();
}


    // Terrain
    if (tessellationShaderProgram && tessellationShaderProgram->id) {
        glUseProgram(tessellationShaderProgram->id);
        setCommonUniformsForCurrentProgram(tessellationShaderProgram, perspectiveProjectionMatrix, viewMatrix, lightPos, lightColor, viewPos);
        renderTerrain(HeightMap);

    }

    glUseProgram(0);
}
