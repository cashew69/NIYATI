enum {
    ATTRIB_POSITION = 0,
    ATTRIB_NORMAL = 1,
    ATTRIB_COLOR = 2,
    ATTRIB_TEXCOORD = 3
};

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

// Helper to check and bind texture
void bindTex(ShaderProgram* program, const char* hasName, const char* texName, GLuint textureID, int slot) {
    GLint hasLoc = getUniformLocation(program, hasName);
    GLint texLoc = getUniformLocation(program, texName);

    if (hasLoc != -1 && texLoc != -1) {
        Bool hasTex = (textureID != 0);
        glUniform1i(hasLoc, hasTex);
        if (hasTex) {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glUniform1i(texLoc, slot);
        }
    }
}

void setMaterialUniforms(ShaderProgram* program, Material* material) {

    // Basic PBR / Phong Uniforms
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

    // Bind Textures
    bindTex(program, "uHasDiffuseTexture", "uDiffuseTexture", material->diffuseTexture, 0);
    bindTex(program, "uHasNormalTexture", "uNormalTexture", material->normalTexture, 1);

    // PBR: Metallic/Roughness often packed in one texture (ORM: R=Occlusion, G=Roughness, B=Metallic)
    // We bind it to both Metallic and Roughness slots in shader if they are separate,
    // or if shader handles packed ORM, we bind it once.
    // Our pbrFrag.glsl expects "uMetallicMap", "uRoughnessMap", "uAOMap".
    // If we have a packed texture, we bind it to ALL of them and shader needs to channel select.
    // Since pbrFrag currently just samples .r from each, we might need to update shader or bind different views.
    // For now, let's bind the ORM texture to all 3 slots (2, 3, 4).
    // The shader samples .r for metallic/roughness/ao.
    // WAIT: glTF ORM is R=AO, G=Roughness, B=Metallic.
    // My shader: uMetallicMap.r, uRoughnessMap.r, uAOMap.r.
    // This mismatch needs fixing in SHADER.

    bindTex(program, "uHasMetallicMap", "uMetallicMap", material->metallicRoughnessTexture, 2);
    bindTex(program, "uHasRoughnessMap", "uRoughnessMap", material->metallicRoughnessTexture, 3);
    bindTex(program, "uHasAOMap", "uAOMap", material->metallicRoughnessTexture, 4);

    bindTex(program, "uHasEmissiveMap", "uEmissiveMap", material->emissiveTexture, 5);

    if (material->opacity < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

}


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

// Helper to determine if texture path is embedded
// Assimp uses "*0", "*1" etc.
const struct aiTexture* getEmbeddedTexture(const struct aiScene* scene, const char* path) {
    if (path[0] == '*') {
        int index = atoi(path + 1);
        if (index >= 0 && index < scene->mNumTextures) {
            return scene->mTextures[index];
        }
    }
    return NULL;
}

GLuint loadEmbeddedTexture(const struct aiTexture* embeddedTex) {
    if (!embeddedTex) return 0;

    unsigned char* image = nullptr;
    int width, height, channels;

    if (embeddedTex->mHeight == 0) {
        // Compressed texture (e.g. PNG, JPEG) in mWidth bytes
        image = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                                      embeddedTex->mWidth, &width, &height, &channels, STBI_rgb_alpha);
    } else {
        // Raw ARGB8888 data
        image = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                                      embeddedTex->mWidth * embeddedTex->mHeight * 4, &width, &height, &channels, STBI_rgb_alpha);
    }

    if (!image) {
        fprintf(gpFile, "Failed to load embedded texture from memory\n");
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // For PBR textures, repeat is usually good, but clamp might be safer for single objects
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

GLuint loadTextureGeneral(const char* path, const char* modelDirectory, const struct aiScene* scene) {
    const struct aiTexture* embedded = getEmbeddedTexture(scene, path);
    if (embedded) {
        fprintf(gpFile, "Loading embedded texture: %s\n", path);
        return loadEmbeddedTexture(embedded);
    } else {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", modelDirectory, path);
        fprintf(gpFile, "Loading file texture: %s\n", fullPath);
        return loadGLTexture(fullPath);
    }
}

// Needs to go through logger and error handling.
void loadMaterialFromAssimp(Material* material, const struct aiMaterial* aiMat, const char* modelDirectory, const struct aiScene* scene) {
    aiColor4D diffuse;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
        material->diffuseColor[0] = diffuse.r;
        material->diffuseColor[1] = diffuse.g;
        material->diffuseColor[2] = diffuse.b;
    } else {
        material->diffuseColor[0] = 0.8f; material->diffuseColor[1] = 0.8f; material->diffuseColor[2] = 0.8f;
    }

    aiColor4D specular;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_SPECULAR, &specular) == AI_SUCCESS) {
        material->specularColor[0] = specular.r; material->specularColor[1] = specular.g; material->specularColor[2] = specular.b;
    } else {
        material->specularColor[0] = 1.0f; material->specularColor[1] = 1.0f; material->specularColor[2] = 1.0f;
    }

    float shininess;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_SHININESS, &shininess) == AI_SUCCESS) {
        material->shininess = shininess;
    } else {
        material->shininess = 32.0f;
    }

    float opacity;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS) {
        material->opacity = opacity;
    } else {
        material->opacity = 1.0f;
    }

    aiColor4D emissive;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_EMISSIVE, &emissive) == AI_SUCCESS) {
        material->isEmissive = (emissive.r > 0.01f || emissive.g > 0.01f || emissive.b > 0.01f);
    } else {
        material->isEmissive = False;
    }

    aiString texPath;

    // Log all available textures in this material for debugging
    fprintf(gpFile, "--- Material Texture Debug ---\n");
    for (int i = 0; i <= 21; i++) {  // Check all texture types
        if (aiGetMaterialTexture(aiMat, (aiTextureType)i, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
            fprintf(gpFile, "  TextureType %d: %s\n", i, texPath.data);
        }
    }
    fprintf(gpFile, "--- End Material Debug ---\n");

    // Diffuse / BaseColor - Try multiple fallbacks
    material->diffuseTexture = 0;
    if (aiGetMaterialTexture(aiMat, aiTextureType_DIFFUSE, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Diffuse (DIFFUSE): %s\n", texPath.data);
        material->diffuseTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_BASE_COLOR, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Diffuse (BASE_COLOR): %s\n", texPath.data);
        material->diffuseTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    }
    fprintf(gpFile, "Diffuse texture ID: %u\n", material->diffuseTexture);

    // Normal Map - Try multiple fallbacks
    material->normalTexture = 0;
    if (aiGetMaterialTexture(aiMat, aiTextureType_NORMALS, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Normal (NORMALS): %s\n", texPath.data);
        material->normalTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_HEIGHT, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Normal (HEIGHT): %s\n", texPath.data);
        material->normalTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_NORMAL_CAMERA, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Normal (NORMAL_CAMERA): %s\n", texPath.data);
        material->normalTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    }
    fprintf(gpFile, "Normal texture ID: %u\n", material->normalTexture);

    // PBR Metallic/Roughness Texture (ORM)
    // glTF stores this as aiTextureType_UNKNOWN or aiTextureType_METALNESS/DIFFUSE_ROUGHNESS
    material->metallicRoughnessTexture = 0;
    if (aiGetMaterialTexture(aiMat, aiTextureType_UNKNOWN, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading ORM (UNKNOWN): %s\n", texPath.data);
        material->metallicRoughnessTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_METALNESS, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading ORM (METALNESS): %s\n", texPath.data);
        material->metallicRoughnessTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading ORM (DIFFUSE_ROUGHNESS): %s\n", texPath.data);
        material->metallicRoughnessTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    }
    fprintf(gpFile, "ORM texture ID: %u\n", material->metallicRoughnessTexture);

    // Emissive Texture
    material->emissiveTexture = 0;
    if (aiGetMaterialTexture(aiMat, aiTextureType_EMISSIVE, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Emissive: %s\n", texPath.data);
        material->emissiveTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_EMISSION_COLOR, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading Emissive (EMISSION_COLOR): %s\n", texPath.data);
        material->emissiveTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    }
    fprintf(gpFile, "Emissive texture ID: %u\n", material->emissiveTexture);

    // AO Texture - Try lightmap or ambient occlusion
    material->aoTexture = 0;
    if (aiGetMaterialTexture(aiMat, aiTextureType_LIGHTMAP, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading AO (LIGHTMAP): %s\n", texPath.data);
        material->aoTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    } else if (aiGetMaterialTexture(aiMat, aiTextureType_AMBIENT_OCCLUSION, 0, &texPath, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
        fprintf(gpFile, "Loading AO (AMBIENT_OCCLUSION): %s\n", texPath.data);
        material->aoTexture = loadTextureGeneral(texPath.data, modelDirectory, scene);
    }
    fprintf(gpFile, "AO texture ID: %u\n", material->aoTexture);

    // --- BRUTE FORCE FALLBACK ---
    // If we have embedded textures but material lookup failed, force assign them
    // heavily dependent on specific GLB packing, but good for debug

    if (scene->mNumTextures > 0) {
        fprintf(gpFile, "DEBUG: Scene has %d embedded textures. Checking for missing assignments...\n", scene->mNumTextures);

        // Helper to load by index if not already loaded
        auto loadByIndex = [&](int index) -> GLuint {
             if (index >= scene->mNumTextures) return 0;
             char path[16];
             sprintf(path, "*%d", index);
             fprintf(gpFile, "DEBUG: Force loading embedded texture %d (%s)\n", index, path);
             return loadTextureGeneral(path, modelDirectory, scene);
        };

        // DamagedHelmet Fallback Strategy:
        // Usually: *0=BaseColor/Diffuse, *1=ORM (Occ/Rough/Metal), *2=Normal, *3=Emissive
        // Currently log showed *1=Emissive, *0=Lightmap(AO)

        // 1. Force Diffuse from *0 (likely BaseColor) if missing
        if (material->diffuseTexture == 0 && scene->mNumTextures > 0) {
            fprintf(gpFile, "FALLBACK: Force assigning *0 to Diffuse\n");
            material->diffuseTexture = loadByIndex(0);
        }

        // 2. Force Normal from *2 (often Normal) if missing
        if (material->normalTexture == 0 && scene->mNumTextures > 2) {
             fprintf(gpFile, "FALLBACK: Force assigning *2 to Normal\n");
             material->normalTexture = loadByIndex(2);
        }
        else if (material->normalTexture == 0 && scene->mNumTextures > 1) {
             // Maybe *1 is normal if no ORM/Emissive?
             // But log said *1 is Emissive.
        }

        // 3. Force ORM from *1 or *something else if missing
        // Log said *1 was Emissive. *0 was AO.
        // If *0 is AO, it might actually be ORM (R=AO).
        if (material->metallicRoughnessTexture == 0 && scene->mNumTextures > 0) {
             if (material->aoTexture != 0) {
                 // Reuse AO texture as ORM if we found one (likely *0)
                 fprintf(gpFile, "FALLBACK: reusing AO texture as ORM\n");
                 material->metallicRoughnessTexture = material->aoTexture;
             } else {
                 // Try *0
                 fprintf(gpFile, "FALLBACK: Force assigning *0 to ORM\n");
                 material->metallicRoughnessTexture = loadByIndex(0);
             }
        }
    }
}

Mesh* createMesh(const ModelVertexData* data, const Material* material) {
    if (!data || data->vertexCount == 0) {
        fprintf(gpFile, "Error: Invalid vertex data for mesh creation\n");
        return NULL;
    }

    Mesh* mesh = (Mesh*)calloc(1, sizeof(Mesh));
    if (!mesh) {
        fprintf(gpFile, "Error: Failed to allocate memory for mesh\n");
        return NULL;
    }

    if (!createVAO_VBO(&mesh->vao, &mesh->vbo_position, &mesh->vbo_normal,
                       &mesh->vbo_color, &mesh->vbo_texcoord, &mesh->ibo, data)) {
        fprintf(gpFile, "Error: Failed to create VAO/VBO for mesh\n");
        free(mesh);
        return NULL;
    }

    mesh->indexCount = data->indexCount;

    if (material) {
        memcpy(mesh->material.diffuseColor, material->diffuseColor, sizeof(float) * 3);
        memcpy(mesh->material.specularColor, material->specularColor, sizeof(float) * 3);
        mesh->material.shininess = material->shininess;
        mesh->material.opacity = material->opacity;
        mesh->material.isEmissive = material->isEmissive;

        mesh->material.diffuseTexture = material->diffuseTexture;
        mesh->material.normalTexture = material->normalTexture;
        mesh->material.metallicRoughnessTexture = material->metallicRoughnessTexture;
        mesh->material.aoTexture = material->aoTexture;
        mesh->material.emissiveTexture = material->emissiveTexture;

    } else {
        // Defaults
        mesh->material.diffuseColor[0] = 0.8f; mesh->material.diffuseColor[1] = 0.8f; mesh->material.diffuseColor[2] = 0.8f;
        mesh->material.specularColor[0] = 1.0f; mesh->material.specularColor[1] = 1.0f; mesh->material.specularColor[2] = 1.0f;
        mesh->material.shininess = 32.0f;
        mesh->material.opacity = 1.0f;
        mesh->material.isEmissive = False;
        mesh->material.diffuseTexture = 0;
        mesh->material.normalTexture = 0;
        mesh->material.metallicRoughnessTexture = 0;
        mesh->material.aoTexture = 0;
        mesh->material.emissiveTexture = 0;
    }


    mesh->transform = createTransform(
        vec3(0.0f, 0.0f, 0.0f),  // position
        vec3(0.0f, 0.0f, 0.0f),  // rotation
        vec3(1.0f, 1.0f, 1.0f)   // scale
    );

    mesh->userFragmentCode = NULL;

    fprintf(gpFile, "Mesh created: %zu indices\n", mesh->indexCount);
    return mesh;
}

void freeMesh(Mesh* mesh) {
    if (!mesh) return;

    if (mesh->material.diffuseTexture) {
        glDeleteTextures(1, &mesh->material.diffuseTexture);
    }
    if (mesh->material.normalTexture) {
        glDeleteTextures(1, &mesh->material.normalTexture);
    }

    if (mesh->userFragmentCode) {
        free(mesh->userFragmentCode);
    }

    if (mesh->transform) {
        freeTransform(mesh->transform);
        mesh->transform = NULL;
    }

    glDeleteVertexArrays(1, &mesh->vao);
    glDeleteBuffers(1, &mesh->vbo_position);
    if (mesh->vbo_normal) glDeleteBuffers(1, &mesh->vbo_normal);
    if (mesh->vbo_color) glDeleteBuffers(1, &mesh->vbo_color);
    if (mesh->vbo_texcoord) glDeleteBuffers(1, &mesh->vbo_texcoord);
    if (mesh->ibo) glDeleteBuffers(1, &mesh->ibo);

    free(mesh);
}

Bool loadModel(const char* filename, Mesh** meshes, int* meshCount, float scale) {
    const struct aiScene* scene = aiImportFile(filename,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(gpFile, "Error: Failed to load model %s: %s\n", filename, aiGetErrorString());
        return False;
    }

    fprintf(gpFile, "Loading model: %s\n", filename);
    fprintf(gpFile, "Number of meshes: %d\n", scene->mNumMeshes);

    *meshCount = scene->mNumMeshes;
    *meshes = (Mesh*)calloc(*meshCount, sizeof(Mesh));
    if (!*meshes) {
        fprintf(gpFile, "Error: Failed to allocate memory for meshes\n");
        aiReleaseImport(scene);
        return False;
    }

    const char* modelDir = getDirectoryFromPath(filename);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        struct aiMesh* aiMesh = scene->mMeshes[i];
        ModelVertexData data = {0};

        data.vertexCount = aiMesh->mNumVertices;
        data.positions = (float*)malloc(data.vertexCount * 3 * sizeof(float));

        for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
            data.positions[v * 3 + 0] = aiMesh->mVertices[v].x * scale;
            data.positions[v * 3 + 1] = aiMesh->mVertices[v].y * scale;
            data.positions[v * 3 + 2] = aiMesh->mVertices[v].z * scale;
        }

        if (aiMesh->mNormals) {
            data.normals = (float*)malloc(data.vertexCount * 3 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.normals[v * 3 + 0] = aiMesh->mNormals[v].x;
                data.normals[v * 3 + 1] = aiMesh->mNormals[v].y;
                data.normals[v * 3 + 2] = aiMesh->mNormals[v].z;
            }
        }

        if (aiMesh->mTextureCoords[0]) {
            data.texCoords = (float*)malloc(data.vertexCount * 2 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.texCoords[v * 2 + 0] = aiMesh->mTextureCoords[0][v].x;
                data.texCoords[v * 2 + 1] = aiMesh->mTextureCoords[0][v].y;
            }
        }

        if (aiMesh->mColors[0]) {
            data.colors = (float*)malloc(data.vertexCount * 3 * sizeof(float));
            for (unsigned int v = 0; v < aiMesh->mNumVertices; v++) {
                data.colors[v * 3 + 0] = aiMesh->mColors[0][v].r;
                data.colors[v * 3 + 1] = aiMesh->mColors[0][v].g;
                data.colors[v * 3 + 2] = aiMesh->mColors[0][v].b;
            }
        }

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

        Material material = {0};
        if (aiMesh->mMaterialIndex >= 0 && aiMesh->mMaterialIndex < scene->mNumMaterials) {
            struct aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
            loadMaterialFromAssimp(&material, aiMat, modelDir, scene);
        }

        createVAO_VBO(&(*meshes)[i].vao, &(*meshes)[i].vbo_position, &(*meshes)[i].vbo_normal,
                      &(*meshes)[i].vbo_color, &(*meshes)[i].vbo_texcoord, &(*meshes)[i].ibo, &data);

        (*meshes)[i].indexCount = data.indexCount;

        memcpy((*meshes)[i].material.diffuseColor, material.diffuseColor, sizeof(float) * 3);
        memcpy((*meshes)[i].material.specularColor, material.specularColor, sizeof(float) * 3);
        (*meshes)[i].material.shininess = material.shininess;
        (*meshes)[i].material.opacity = material.opacity;
        (*meshes)[i].material.isEmissive = material.isEmissive;
        (*meshes)[i].material.diffuseTexture = material.diffuseTexture;
        (*meshes)[i].material.normalTexture = material.normalTexture;
        (*meshes)[i].material.metallicRoughnessTexture = material.metallicRoughnessTexture;
        (*meshes)[i].material.emissiveTexture = material.emissiveTexture;
        (*meshes)[i].material.aoTexture = material.aoTexture;

        (*meshes)[i].transform = createTransform(
            vec3(0.0f, 0.0f, 0.0f),
            vec3(0.0f, 0.0f, 0.0f),
            vec3(1.0f, 1.0f, 1.0f)
        );

        (*meshes)[i].userFragmentCode = NULL;

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

void freeModel(Mesh* meshes, int meshCount) {
    if (!meshes) return;

    for (int i = 0; i < meshCount; i++) {
        if (meshes[i].material.diffuseTexture) {
            glDeleteTextures(1, &meshes[i].material.diffuseTexture);
        }
        if (meshes[i].material.normalTexture) {
            glDeleteTextures(1, &meshes[i].material.normalTexture);
        }

        if (meshes[i].userFragmentCode) {
            free(meshes[i].userFragmentCode);
        }

        if (meshes[i].transform) {
            freeTransform(meshes[i].transform);
            meshes[i].transform = NULL;
        }

        glDeleteVertexArrays(1, &meshes[i].vao);
        glDeleteBuffers(1, &meshes[i].vbo_position);
        if (meshes[i].vbo_normal) glDeleteBuffers(1, &meshes[i].vbo_normal);
        if (meshes[i].vbo_color) glDeleteBuffers(1, &meshes[i].vbo_color);
        if (meshes[i].vbo_texcoord) glDeleteBuffers(1, &meshes[i].vbo_texcoord);
        if (meshes[i].ibo) glDeleteBuffers(1, &meshes[i].ibo);
    }
    free(meshes);
}
