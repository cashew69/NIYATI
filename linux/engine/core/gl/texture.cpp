// Used in modelloading.cpp
GLuint loadGLTexture(const char* filename) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

    if (!image) {
        fprintf(gpFile, "Failed to load texture: %s\n", filename);
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image);

    return texture;
}

bool loadPNGTexture(GLuint *texture, char *file, int repeat) // removed unused numberOfChannels arg
{
    int width, height, fileChannels; // Use a local variable for file info
    unsigned char *image = NULL;
    bool bResult = false;

    // 1. Force STBI to always give us 4 channels (RGBA)
    //    We ignore 'fileChannels' for uploading because we are forcing conversion.
    image = stbi_load(file, &width, &height, &fileChannels, STBI_rgb_alpha);

    if (image)
    {
        bResult = true;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glGenTextures(1, texture);
        glBindTexture(GL_TEXTURE_2D, *texture);

        // 2. Set wrapping parameters
        if (repeat == 1) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // 3. Set filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        // 4. Upload Texture
        //    Since we used STBI_rgb_alpha, the data is GUARANTEED to be GL_RGBA.
        //    We do not need to check fileChannels.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

        // 5. Generate Mipmaps
        //    CRITICAL: This must happen for ALL textures using GL_LINEAR_MIPMAP_LINEAR
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(image);
    }
    else
    {
        if (gpFile) fprintf(gpFile, "stbi_load failed for %s: %s\n", file, stbi_failure_reason());
        return false;
    }

    return bResult;
}
