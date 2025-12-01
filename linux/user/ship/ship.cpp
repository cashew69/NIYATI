void renderShip()
{
    if (!sceneMeshes || meshCount < 1) {
        fprintf(gpFile, "Warning: Ship mesh not loaded\n");
        return;
    }
    
    modelLocUniform = getUniformLocation(mainShaderProgram, "uModel");
    
    mat4 modelMatrix = mat4::identity();
    
    modelMatrix = vmath::translate(shipPosition) * modelMatrix;
    
    // Rotate based on ship rotation (Yaw, Pitch, Roll)
    // Order matters: usually Yaw -> Pitch -> Roll or similar depending on desired behavior
    modelMatrix = vmath::rotate(shipRotation[1], 0.0f, 1.0f, 0.0f) * modelMatrix; // Yaw (Y-axis)
    modelMatrix = vmath::rotate(shipRotation[0], 1.0f, 0.0f, 0.0f) * modelMatrix; // Pitch (X-axis)
    modelMatrix = vmath::rotate(shipRotation[2], 0.0f, 0.0f, 1.0f) * modelMatrix; // Roll (Z-axis)
    
    if (modelLocUniform != -1) {
        glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);
    }
    setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

    glBindVertexArray(sceneMeshes[0].vao);

    glDrawElements(GL_TRIANGLES, sceneMeshes[0].indexCount, GL_UNSIGNED_INT, NULL);
    fprintf(gpFile, "Ship rendered with %d indices\n", sceneMeshes[0].indexCount);
    glBindVertexArray(0);

}
