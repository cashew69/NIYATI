// Extended Model Loading with Material Support
// This file includes complete material loading from Assimp

// Attribute indices (from your shaders.c)
enum {
    ATTRIB_POSITION = 0,
    ATTRIB_NORMAL = 1,
    ATTRIB_COLOR = 2,
    ATTRIB_TEXCOORD = 3
};

// Helper function to extract directory from filepath
const char* getDirectoryFromPath(const char* filepath) {
    static char dir[512];
    const char* lastSlash = strrchr(filepath, '/');
    if (!lastSlash) lastSlash = strrchr(filepath, '\\');
    
    if (lastSlash) {
        size_t len = lastSlash - filepath;
        strncpy(dir, filepath, len);
        dir[len] = '\0';
        return dir;
    }
    return ".";
}


void setMaterialUniforms(ShaderProgram* program, Material* material) {
    GLint isEmissiveLoc = getUniformLocation(program, "uIsEmissive");
    if (isEmissiveLoc != -1) glUniform1i(isEmissiveLoc, material->isEmissive);
    GLint diffuseLoc = getUniformLocation(program, "uDiffuseColor");
    if (diffuseLoc != -1) glUniform3fv(diffuseLoc, 1, material->diffuseColor);
    GLint specularLoc = getUniformLocation(program, "uSpecularColor");
    if (specularLoc != -1) glUniform3fv(specularLoc, 1, material->specularColor);
    GLint shininessLoc = getUniformLocation(program, "uShininess");
    if (shininessLoc != -1) glUniform1f(shininessLoc, material->shininess);
    GLint opacityLoc = getUniformLocation(program, "uOpacity");
    if (opacityLoc != -1) glUniform1f(opacityLoc, material->opacity);
    GLint hasDiffuseLoc = getUniformLocation(program, "uHasDiffuseTexture");
    GLint diffuseTexLoc = getUniformLocation(program, "uDiffuseTexture");
    if (hasDiffuseLoc != -1 && diffuseTexLoc != -1) {
        Bool hasDiffuse = material->diffuseTexture != 0;
        glUniform1i(hasDiffuseLoc, hasDiffuse);
        if (hasDiffuse) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, material->diffuseTexture);
            glUniform1i(diffuseTexLoc, 0);
        }
    }
    GLint hasNormalLoc = getUniformLocation(program, "uHasNormalTexture");
    GLint normalTexLoc = getUniformLocation(program, "uNormalTexture");
    if (hasNormalLoc != -1 && normalTexLoc != -1) {
        Bool hasNormal = material->normalTexture != 0;
        glUniform1i(hasNormalLoc, hasNormal);
        if (hasNormal) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, material->normalTexture);
            glUniform1i(normalTexLoc, 1);
        }
    }

    // Handle transparency
    if (material->opacity < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

// Function to create VAO and VBOs for a model
Bool createVAO_VBO(GLuint* vao, GLuint* vbo_position, GLuint* vbo_normal, GLuint* vbo_color, 
                   GLuint* vbo_texcoord, GLuint* ibo, const ModelVertexData* data) {
    if (!vao || !vbo_position || !data || data->vertexCount == 0) {
        fprintf(gpFile, "Error: Invalid parameters for createVAO_VBO\n");
        return False;
    }

    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    if (data->positions) {
        glGenBuffers(1, vbo_position);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo_position);
        glBufferData(GL_ARRAY_BUFFER, data->vertexCount * 3 * sizeof(float), data->positions, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
        glEnableVertexAttribArray(ATTRIB_POSITION);
    }

    if (data->normals) {
        glGenBuffers(1, vbo_normal);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo_normal);
        glBufferData(GL_ARRAY_BUFFER, data->vertexCount * 3 * sizeof(float), data->normals, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
        glEnableVertexAttribArray(ATTRIB_NORMAL);
    }

    if (data->colors) {
        glGenBuffers(1, vbo_color);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo_color);
        glBufferData(GL_ARRAY_BUFFER, data->vertexCount * 3 * sizeof(float), data->colors, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), NULL);
        glEnableVertexAttribArray(ATTRIB_COLOR);
    }

    if (data->texCoords) {
        glGenBuffers(1, vbo_texcoord);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo_texcoord);
        glBufferData(GL_ARRAY_BUFFER, data->vertexCount * 2 * sizeof(float), data->texCoords, GL_STATIC_DRAW);
        glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
        glEnableVertexAttribArray(ATTRIB_TEXCOORD);
    }

    if (data->indices && data->indexCount > 0 && ibo) {
        glGenBuffers(1, ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->indexCount * sizeof(unsigned int), data->indices, GL_STATIC_DRAW);
    }

    glBindVertexArray(0);
    return True;
}

// Helper function to load material from Assimp
void loadMaterialFromAssimp(Material* material, const struct aiMaterial* aiMat, const char* modelDirectory) {
    // Extract diffuse color
     aiColor4D diffuse;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
        material->diffuseColor[0] = diffuse.r;
        material->diffuseColor[1] = diffuse.g;
        material->diffuseColor[2] = diffuse.b;
    } else {
        // Default diffuse color
        material->diffuseColor[0] = 0.8f;
        material->diffuseColor[1] = 0.8f;
        material->diffuseColor[2] = 0.8f;
    }

    // Extract specular color
     aiColor4D specular;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_SPECULAR, &specular) == AI_SUCCESS) {
        material->specularColor[0] = specular.r;
        material->specularColor[1] = specular.g;
        material->specularColor[2] = specular.b;
    } else {
        // Default specular color
        material->specularColor[0] = 1.0f;
        material->specularColor[1] = 1.0f;
        material->specularColor[2] = 1.0f;
    }

    // Extract shininess
    float shininess;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_SHININESS, &shininess) == AI_SUCCESS) {
        material->shininess = shininess;
    } else {
        material->shininess = 32.0f;
    }

    // Extract opacity
    float opacity;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS) {
        material->opacity = opacity;
    } else {
        material->opacity = 1.0f;
    }

    // Check if material is emissive
     aiColor4D emissive;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_EMISSIVE, &emissive) == AI_SUCCESS) {
        // If any emissive component is non-zero
        material->isEmissive = (emissive.r > 0.01f || emissive.g > 0.01f || emissive.b > 0.01f);
    } else {
        material->isEmissive = False;
    }

    // Load diffuse texture
    aiString texPath;
    if (aiGetMaterialTexture(aiMat, aiTextureType_DIFFUSE, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", modelDirectory, texPath.data);
        material->diffuseTexture = loadGLTexture(fullPath);
        fprintf(gpFile, "Loaded diffuse texture: %s\n", fullPath);
    } else {
        material->diffuseTexture = 0;
    }

    // Load normal texture
    if (aiGetMaterialTexture(aiMat, aiTextureType_NORMALS, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", modelDirectory, texPath.data);
        material->normalTexture = loadGLTexture(fullPath);
        fprintf(gpFile, "Loaded normal texture: %s\n", fullPath);
    } else {
        // Try height map as alternative for normal map
        if (aiGetMaterialTexture(aiMat, aiTextureType_HEIGHT, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", modelDirectory, texPath.data);
            material->normalTexture = loadGLTexture(fullPath);
            fprintf(gpFile, "Loaded height/normal texture: %s\n", fullPath);
        } else {
            material->normalTexture = 0;
        }
    }
}

// Function to load a model using Assimp with full material support
Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale) {
    const struct aiScene* scene = aiImportFile(filename, 
        aiProcess_Triangulate | 
        aiProcess_GenNormals | 
        aiProcess_FlipUVs | 
        aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace);  // Added for normal mapping
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(gpFile, "Error: Failed to load model %s: %s\n", filename, aiGetErrorString());
        return False;
    }

    fprintf(gpFile, "Loading model: %s\n", filename);
    fprintf(gpFile, "Number of meshes: %d\n", scene->mNumMeshes);
    fprintf(gpFile, "Number of materials: %d\n", scene->mNumMaterials);

    *meshCount = scene->mNumMeshes;
    *meshes = (Mesh*)calloc(*meshCount, sizeof(Mesh));
    if (!*meshes) {
        fprintf(gpFile, "Error: Failed to allocate memory for meshes\n");
        aiReleaseImport(scene);
        return False;
    }

    // Get model directory for texture loading
    const char* modelDir = getDirectoryFromPath(filename);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        struct aiMesh* aiMesh = scene->mMeshes[i];
        ModelVertexData data = {0};

        fprintf(gpFile, "Processing mesh %d: %d vertices, %d faces\n", 
                i, aiMesh->mNumVertices, aiMesh->mNumFaces);

        // Extract vertex positions
        data.vertexCount = aiMesh->mNumVertices;
        data.positions = (float*)malloc(data.vertexCount * 3 * sizeof(float));
        
        for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
            data.positions[v * 3 + 0] = aiMesh->mVertices[v].x * scale;
            data.positions[v * 3 + 1] = aiMesh->mVertices[v].y * scale;
            data.positions[v * 3 + 2] = aiMesh->mVertices[v].z * scale;
        }

        // Extract normals
        if (aiMesh->mNormals) {
            data.normals = (float*)malloc(data.vertexCount * 3 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.normals[v * 3 + 0] = aiMesh->mNormals[v].x;
                data.normals[v * 3 + 1] = aiMesh->mNormals[v].y;
                data.normals[v * 3 + 2] = aiMesh->mNormals[v].z;
            }
        }

        // Extract texture coordinates
        if (aiMesh->mTextureCoords[0]) {
            data.texCoords = (float*)malloc(data.vertexCount * 2 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.texCoords[v * 2 + 0] = aiMesh->mTextureCoords[0][v].x;
                data.texCoords[v * 2 + 1] = aiMesh->mTextureCoords[0][v].y;
            }
        }

        // Extract vertex colors (if available)
        if (aiMesh->mColors[0]) {
            data.colors = (float*)malloc(data.vertexCount * 3 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.colors[v * 3 + 0] = aiMesh->mColors[0][v].r;
                data.colors[v * 3 + 1] = aiMesh->mColors[0][v].g;
                data.colors[v * 3 + 2] = aiMesh->mColors[0][v].b;
            }
        }

        // Extract indices
        if (aiMesh->mNumFaces > 0) {
            data.indexCount = aiMesh->mNumFaces * 3;
            data.indices = (unsigned int*)malloc(data.indexCount * sizeof(unsigned int));
            for (unsigned int f = 0; f < aiMesh->mNumFaces; f++) {
                struct aiFace* face = &aiMesh->mFaces[f];
                for (unsigned int j = 0; j < 3; j++) {
                    data.indices[f * 3 + j] = face->mIndices[j];
                }
            }
        }

        // Create OpenGL buffers
        createVAO_VBO(&(*meshes)[i].vao, &(*meshes)[i].vbo_position, &(*meshes)[i].vbo_normal,
                      &(*meshes)[i].vbo_color, &(*meshes)[i].vbo_texcoord, &(*meshes)[i].ibo, &data);
        
        (*meshes)[i].indexCount = data.indexCount;
        
        // Load material from Assimp
        if (aiMesh->mMaterialIndex >= 0 && aiMesh->mMaterialIndex < scene->mNumMaterials) {
            struct aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
            loadMaterialFromAssimp(&(*meshes)[i].material, aiMat, modelDir);
            fprintf(gpFile, "Loaded material for mesh %d\n", i);
        } else {
            // Set default material if none exists
            fprintf(gpFile, "Using default material for mesh %d\n", i);
            (*meshes)[i].material.diffuseColor[0] = 0.8f;
            (*meshes)[i].material.diffuseColor[1] = 0.8f;
            (*meshes)[i].material.diffuseColor[2] = 0.8f;
            (*meshes)[i].material.specularColor[0] = 1.0f;
            (*meshes)[i].material.specularColor[1] = 1.0f;
            (*meshes)[i].material.specularColor[2] = 1.0f;
            (*meshes)[i].material.shininess = 32.0f;
            (*meshes)[i].material.opacity = 1.0f;
            (*meshes)[i].material.isEmissive = False;
            (*meshes)[i].material.diffuseTexture = 0;
            (*meshes)[i].material.normalTexture = 0;
        }
        
        // Initialize identity matrix
        for (int j = 0; j < 16; j++) {
            (*meshes)[i].modelMatrix[j] = (j % 5 == 0) ? 1.0f : 0.0f;
        }

        // Free temporary vertex data
        free(data.positions);
        if (data.normals) free(data.normals);
        if (data.colors) free(data.colors);
        if (data.texCoords) free(data.texCoords);
        if (data.indices) free(data.indices);
    }

    aiReleaseImport(scene);
    fprintf(gpFile, "Model loaded successfully: %s\n", filename);
    return True;
}

// Function to free a loaded model (with texture cleanup)
void freeModel(Mesh* meshes, int meshCount) {
    for (int i = 0; i < meshCount; i++) {
        // Delete textures
        if (meshes[i].material.diffuseTexture) {
            glDeleteTextures(1, &meshes[i].material.diffuseTexture);
        }
        if (meshes[i].material.normalTexture) {
            glDeleteTextures(1, &meshes[i].material.normalTexture);
        }
        
        // Free user fragment code if allocated
        if (meshes[i].userFragmentCode) {
            free(meshes[i].userFragmentCode);
        }
        
        // Delete OpenGL buffers
        glDeleteVertexArrays(1, &meshes[i].vao);
        glDeleteBuffers(1, &meshes[i].vbo_position);
        if (meshes[i].vbo_normal) glDeleteBuffers(1, &meshes[i].vbo_normal);
        if (meshes[i].vbo_color) glDeleteBuffers(1, &meshes[i].vbo_color);
        if (meshes[i].vbo_texcoord) glDeleteBuffers(1, &meshes[i].vbo_texcoord);
        if (meshes[i].ibo) glDeleteBuffers(1, &meshes[i].ibo);
    }
    free(meshes);
}
