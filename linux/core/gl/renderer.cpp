/*void setUniforms()
{
        // Set uniforms
        projLocUniform = getUniformLocation(mainShaderProgram, "uProjection");
        viewLocUniform = getUniformLocation(mainShaderProgram, "uView");
        modelLocUniform = getUniformLocation(mainShaderProgram, "uModel");
        lightPosLocUniform = getUniformLocation(mainShaderProgram, "uLightPos");
        lightColorLocUniform = getUniformLocation(mainShaderProgram, "uLightColor");
        viewPosLocUniform = getUniformLocation(mainShaderProgram, "uViewPos");
}*/


void setCommonUniformsForCurrentProgram(ShaderProgram* program, const mat4 &proj, const mat4 &view, 
                                        vec3 lightPos, vec3 lightColor, vec3 viewPos)
{
    GLint loc;
    loc = glGetUniformLocation(program->id, "uProjection");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, proj);

    loc = glGetUniformLocation(program->id, "uView");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, view);

    /*loc = glGetUniformLocation(program->id, "uModel");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, model);*/

    loc = glGetUniformLocation(program->id, "uLightPos");
    if (loc != -1) glUniform3fv(loc, 1, lightPos);

    loc = glGetUniformLocation(program->id, "uLightColor");
    if (loc != -1) glUniform3fv(loc, 1, lightColor);

    loc = glGetUniformLocation(program->id, "uViewPos");
    if (loc != -1) glUniform3fv(loc, 1, viewPos);

}
  

void renderer(float rotationAngle, GLint HeightMap) {
    // Light settings
        vec3 lightPos = vec3(100.0f, 100.0f, 100.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        vec3 viewPos = vec3(0.0f, 50.0f, 150.0f);

    if (mainShaderProgram && mainShaderProgram->id) {
        glUseProgram(mainShaderProgram->id);
        
       
        if (projLocUniform != -1) glUniformMatrix4fv(projLocUniform, 1, GL_FALSE, perspectiveProjectionMatrix);
        if (viewLocUniform != -1) glUniformMatrix4fv(viewLocUniform, 1, GL_FALSE, viewMatrix);
        
                
        if (lightPosLocUniform != -1) glUniform3fv(lightPosLocUniform, 1, lightPos);
        if (lightColorLocUniform != -1) glUniform3fv(lightColorLocUniform, 1, lightColor);
        if (viewPosLocUniform != -1) glUniform3fv(viewPosLocUniform, 1, viewPos);

        
        // Render existing model meshes
        for (int i = 0; i < meshCount; i++) {
            mat4 modelMatrix = vmath::rotate(rotationAngle, vec3(0.0f, 1.0f, 0.0f)) * 
                              vmath::rotate(-90.0f, vec3(1.0f, 0.0f, 0.0f));
            
            if (modelLocUniform != -1) glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
            
        setCommonUniformsForCurrentProgram(mainShaderProgram, perspectiveProjectionMatrix, viewMatrix, lightPos, lightColor, viewPos);
            setMaterialUniforms(mainShaderProgram, &sceneMeshes[i].material);
            fprintf(gpFile, "In program shaderprog:%d \n", mainShaderProgram);
            
            glBindVertexArray(sceneMeshes[i].vao);
            
            if (sceneMeshes[i].ibo && sceneMeshes[i].indexCount > 0) {
                glDrawElements(GL_TRIANGLES, sceneMeshes[i].indexCount, GL_UNSIGNED_INT, NULL);
            }
            
            glBindVertexArray(0);
        }

        
        fprintf(gpFile, "In program shaderprog:%d \n", mainShaderProgram);
       
        glUseProgram(0);
    }
    if (tessellationShaderProgram && tessellationShaderProgram->id)
    {
        glUseProgram(tessellationShaderProgram->id);

        setCommonUniformsForCurrentProgram(tessellationShaderProgram, perspectiveProjectionMatrix, viewMatrix, lightPos, lightColor, viewPos);
        renderUserMeshes(HeightMap);
        glUseProgram(0);
    }

}
