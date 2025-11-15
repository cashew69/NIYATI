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



  

void renderer(float rotationAngle, GLint HeightMap) {
    // Light settings
    //vec3 lightPos = vec3(100.0f, 100.0f, 100.0f);
    //vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
    //vec3 viewPos = vec3(0.0f, 50.0f, 150.0f);

    /*if (mainShaderProgram && mainShaderProgram->id) {
        glUseProgram(mainShaderProgram->id);
        
        //if (projLocUniform != -1) glUniformMatrix4fv(projLocUniform, 1, GL_FALSE, perspectiveProjectionMatrix);
        //if (viewLocUniform != -1) glUniformMatrix4fv(viewLocUniform, 1, GL_FALSE, viewMatrix);
        
        //if (lightPosLocUniform != -1) glUniform3fv(lightPosLocUniform, 1, lightPos);
        //if (lightColorLocUniform != -1) glUniform3fv(lightColorLocUniform, 1, lightColor);
        //if (viewPosLocUniform != -1) glUniform3fv(viewPosLocUniform, 1, viewPos);

        for (int i = 0; i < meshCount; i++) {
            if (!sceneMeshes[i].transform) continue;
            
            setPosition(sceneMeshes[i].transform, vec3(0.0f, 10.0f, 0.0f));
            setRotation(sceneMeshes[i].transform, vec3(0.0f, 0.0f, -90.0f));
            
            mat4 modelMatrix = getWorldMatrix(sceneMeshes[i].transform);
            
            if (modelLocUniform != -1) {
                glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
            }
            
            setCommonUniformsForCurrentProgram(mainShaderProgram, perspectiveProjectionMatrix, 
                                              viewMatrix, lightPos, lightColor, viewPos);
            setMaterialUniforms(mainShaderProgram, &sceneMeshes[i].material);
            fprintf(gpFile, "In program shaderprog:%d \n", mainShaderProgram);
            
            glBindVertexArray(sceneMeshes[i].vao);
            
            if (sceneMeshes[i].ibo && sceneMeshes[i].indexCount > 0) {
                glDrawElementsInstanced(GL_TRIANGLES, sceneMeshes[i].indexCount, GL_UNSIGNED_INT, NULL, 30);
            }
            
            glBindVertexArray(0);
        }
        
        fprintf(gpFile, "In program shaderprog:%d \n", mainShaderProgram);
        glUseProgram(0);
    }*/

    // Terrain
    
        renderUserMeshes(HeightMap);
}
