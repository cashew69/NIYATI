void setUniforms()
{
        // Set uniforms
        projLocUniform = getUniformLocation(mainShaderProgram, "uProjection");
        viewLocUniform = getUniformLocation(mainShaderProgram, "uView");
        modelLocUniform = getUniformLocation(mainShaderProgram, "uModel");
        lightPosLocUniform = getUniformLocation(mainShaderProgram, "uLightPos");
        lightColorLocUniform = getUniformLocation(mainShaderProgram, "uLightColor");
        viewPosLocUniform = getUniformLocation(mainShaderProgram, "uViewPos");
        colorTextureLocUniform = getUniformLocation(mainShaderProgram, "uColorTexture");
}
  

void renderer(float rotationAngle) {
    if (mainShaderProgram && mainShaderProgram->id) {
        glUseProgram(mainShaderProgram->id);
        
       
        if (projLocUniform != -1) glUniformMatrix4fv(projLocUniform, 1, GL_FALSE, perspectiveProjectionMatrix);
        if (viewLocUniform != -1) glUniformMatrix4fv(viewLocUniform, 1, GL_FALSE, viewMatrix);
        
        // Light settings
        vec3 lightPos = vec3(100.0f, 100.0f, 100.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        vec3 viewPos = vec3(0.0f, 50.0f, 150.0f);
        
        if (lightPosLocUniform != -1) glUniform3fv(lightPosLocUniform, 1, lightPos);
        if (lightColorLocUniform != -1) glUniform3fv(lightColorLocUniform, 1, lightColor);
        if (viewPosLocUniform != -1) glUniform3fv(viewPosLocUniform, 1, viewPos);

        if (colorTextureLocUniform != -1) glUniform1i(colorTextureLocUniform, 2);
        
        // Render existing model meshes
        for (int i = 0; i < meshCount; i++) {
            mat4 modelMatrix = vmath::rotate(rotationAngle, vec3(0.0f, 1.0f, 0.0f)) * 
                              vmath::rotate(-90.0f, vec3(1.0f, 0.0f, 0.0f));
            
            if (modelLocUniform != -1) glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
            
            setMaterialUniforms(mainShaderProgram, &sceneMeshes[i].material);
            
            glBindVertexArray(sceneMeshes[i].vao);
            
            if (sceneMeshes[i].ibo && sceneMeshes[i].indexCount > 0) {
                glDrawElements(GL_TRIANGLES, sceneMeshes[i].indexCount, GL_UNSIGNED_INT, NULL);
            }
            
            glBindVertexArray(0);
        }

        renderUserMeshes();
        
       
        glUseProgram(0);
    }
}
