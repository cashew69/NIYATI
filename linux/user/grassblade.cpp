void renderGrassBlade()
{

        setPosition(sceneMeshes[0].transform, vec3(0.0f, 10.0f, 0.0f));
        setRotation(sceneMeshes[0].transform, vec3(0.0f, 0.0f, -90.0f));

        mat4 modelMatrix = getWorldMatrix(sceneMeshes[0].transform);

        glUniformMatrix4fv(modelLocUniform, 1, GL_FALSE, modelMatrix);

        setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

        glBindVertexArray(sceneMeshes[0].vao);

        //if (sceneMeshes[0].ibo && sceneMeshes[0].indexCount > 0) 
        //{
            glDrawElementsInstanced(GL_TRIANGLES, sceneMeshes[0].indexCount, GL_UNSIGNED_INT, NULL, 30);
        //}

        glBindVertexArray(0);
}
