void renderer(float rotationAngle) {
    if (mainShaderProgram && mainShaderProgram->id) {
        glUseProgram(mainShaderProgram->id);
        
        // Set uniforms
        GLint projLoc = getUniformLocation(mainShaderProgram, "uProjection");
        GLint viewLoc = getUniformLocation(mainShaderProgram, "uView");
        GLint modelLoc = getUniformLocation(mainShaderProgram, "uModel");
        GLint lightPosLoc = getUniformLocation(mainShaderProgram, "uLightPos");
        GLint lightColorLoc = getUniformLocation(mainShaderProgram, "uLightColor");
        GLint viewPosLoc = getUniformLocation(mainShaderProgram, "uViewPos");
        
        if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, perspectiveProjectionMatrix);
        if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix);
        
        // Light settings
        vec3 lightPos = vec3(100.0f, 100.0f, 100.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
        vec3 viewPos = vec3(0.0f, 50.0f, 150.0f);
        
        if (lightPosLoc != -1) glUniform3fv(lightPosLoc, 1, lightPos);
        if (lightColorLoc != -1) glUniform3fv(lightColorLoc, 1, lightColor);
        if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, viewPos);
        
        // Render existing model meshes
        for (int i = 0; i < meshCount; i++) {
            mat4 modelMatrix = vmath::rotate(rotationAngle, vec3(0.0f, 1.0f, 0.0f)) * 
                              vmath::rotate(-90.0f, vec3(1.0f, 0.0f, 0.0f));
            
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMatrix);
            
            setMaterialUniforms(mainShaderProgram, &sceneMeshes[i].material);
            
            glBindVertexArray(sceneMeshes[i].vao);
            
            if (sceneMeshes[i].ibo && sceneMeshes[i].indexCount > 0) {
                glDrawElements(GL_TRIANGLES, sceneMeshes[i].indexCount, GL_UNSIGNED_INT, NULL);
            }
            
            glBindVertexArray(0);
        }
        
        // Render terrain
        if (terrainMesh && terrainMesh->vao) {
            mat4 terrainModelMatrix = mat4::identity();  // Terrain at origin, no rotation
            
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, terrainModelMatrix);
            
            setMaterialUniforms(mainShaderProgram, &terrainMesh->material);
            
            glBindVertexArray(terrainMesh->vao);
            glDrawElements(GL_TRIANGLES, terrainMesh->indexCount, GL_UNSIGNED_INT, NULL);
            glBindVertexArray(0);
        }
        
        glUseProgram(0);
    }
}
