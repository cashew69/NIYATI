
// skybox.cpp — Environment cubemap loading and skybox rendering.
// Owns: envCubemap, skybox presets, cubemap/HDR loading, skybox rendering.
// Calls: generateIBLMaps() and renderCube() from pbr.cpp.

#include "engine/platform.h"

// ============================================================================
// Environment Cubemap
// ============================================================================

unsigned int envCubemap = 0;

// ============================================================================
// Skybox Presets
// ============================================================================

struct SkyboxPreset {
    const char* name;
    const char* faces[6];
};

const int SKYBOX_PRESET_COUNT = 2;
SkyboxPreset skyboxPresets[SKYBOX_PRESET_COUNT] = {
    {
        "Night Sky",
        {
            "user/models/skybox/posx.jpg",
            "user/models/skybox/negx.jpg",
            "user/models/skybox/posy.jpg",
            "user/models/skybox/negy.jpg",
            "user/models/skybox/posz.jpg",
            "user/models/skybox/negz.jpg"
        }
    },
    {
        "Standard Daylight",
        {
            "user/models/Standard-Cube-Map/px.png",
            "user/models/Standard-Cube-Map/nx.png",
            "user/models/Standard-Cube-Map/py.png",
            "user/models/Standard-Cube-Map/ny.png",
            "user/models/Standard-Cube-Map/pz.png",
            "user/models/Standard-Cube-Map/nz.png"
        }
    }
};

int currentSkyboxPreset = 0;
bool skyboxNeedsReload = false;

// ============================================================================
// Cubemap Loading
// ============================================================================

unsigned int loadCubemapFromFaces(const char* faces[6])
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    stbi_set_flip_vertically_on_load(false); // Cubemap faces should NOT be flipped

    for (unsigned int i = 0; i < 6; i++)
    {
        int width, height, nrChannels;
        unsigned char *data = stbi_load(faces[i], &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            fprintf(gpFile, "Loaded cubemap face %d: %s (%dx%d)\n", i, faces[i], width, height);
        }
        else
        {
            fprintf(gpFile, "Failed to load cubemap face: %s\n", faces[i]);
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    return textureID;
}

// ============================================================================
// HDR → Cubemap Conversion
// ============================================================================

static ShaderProgram* equirectangularToCubemapShader = NULL;

static unsigned int convertHDRToCubemap(const char* hdrPath)
{
    if (!hdrPath) return 0;

    // Load HDR image
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf(hdrPath, &width, &height, &nrComponents, 0);

    if (!data)
    {
        fprintf(gpFile, "Failed to load HDR image: %s\n", hdrPath);
        return 0;
    }

    // Upload HDR to 2D texture
    unsigned int hdrTexture;
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    fprintf(gpFile, "HDR image loaded: %s (%dx%d)\n", hdrPath, width, height);

    // Build equirectangular shader if needed
    if (!equirectangularToCubemapShader)
    {
        const char* files[5] = { "engine/shaders/PBR/cubemap.vert", NULL, NULL, NULL, "engine/shaders/PBR/equirectangular_to_cubemap.frag" };
        buildShaderProgramFromFiles(files, 5, &equirectangularToCubemapShader, NULL, NULL, 0);
    }

    // Create empty cubemap
    unsigned int cubemap;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Setup FBO for rendering into cubemap faces
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    mat4 captureProjection = vmath::perspective(90.0f, 1.0f, 0.1f, 10.0f);
    mat4 captureViews[] =
    {
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f)),
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f)),
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f)),
        vmath::lookat(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f))
    };

    // Render equirectangular HDR to 6 cubemap faces
    glUseProgram(equirectangularToCubemapShader->id);
    glUniform1i(glGetUniformLocation(equirectangularToCubemapShader->id, "equirectangularMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(equirectangularToCubemapShader->id, "projection"), 1, GL_FALSE, captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectangularToCubemapShader->id, "view"), 1, GL_FALSE, captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube(); // from pbr.cpp
    }

    // Generate mipmaps
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
    glDeleteTextures(1, &hdrTexture);

    // Restore viewport
    int scrWidth, scrHeight;
    platformGetFramebufferSize(&scrWidth, &scrHeight);
    glViewport(0, 0, scrWidth, scrHeight);

    return cubemap;
}

// ============================================================================
// Skybox Initialization & Rendering
// ============================================================================

ShaderProgram* skyboxShader = NULL;

void initSkybox(const char* hdrPath)
{
    // 1. Try loading HDR environment
    envCubemap = convertHDRToCubemap(hdrPath);

    // 2. Fallback to cubemap face images
    if (envCubemap == 0)
    {
        fprintf(gpFile, "HDR failed, loading cubemap faces from preset: %s\n", skyboxPresets[currentSkyboxPreset].name);
        envCubemap = loadCubemapFromFaces(skyboxPresets[currentSkyboxPreset].faces);
    }

    // 3. Generate IBL maps from environment
    if (envCubemap)
    {
        generateIBLMaps(envCubemap); // from pbr.cpp
        fprintf(gpFile, "Skybox initialized with envCubemap=%u\n", envCubemap);
    }
    else
    {
        fprintf(gpFile, "WARNING: No environment loaded. Skybox and IBL disabled.\n");
    }

    // 4. Build skybox rendering shader
    const char* skyboxFiles[5] = { "engine/shaders/PBR/skybox.vert", NULL, NULL, NULL, "engine/shaders/PBR/skybox.frag" };
    buildShaderProgramFromFiles(skyboxFiles, 5, &skyboxShader, NULL, NULL, 0);
}

void renderSkybox(mat4 viewMatrix, mat4 projectionMatrix)
{
    if (!skyboxShader || !envCubemap) return;

    glDepthFunc(GL_LEQUAL);
    glUseProgram(skyboxShader->id);

    // Remove translation from view matrix (sky follows camera)
    mat4 view = viewMatrix;
    view[3][0] = 0.0f;
    view[3][1] = 0.0f;
    view[3][2] = 0.0f;

    glUniformMatrix4fv(glGetUniformLocation(skyboxShader->id, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader->id, "projection"), 1, GL_FALSE, projectionMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glUniform1i(glGetUniformLocation(skyboxShader->id, "environmentMap"), 0);

    renderCube(); // from pbr.cpp

    glDepthFunc(GL_LESS);
}

// ============================================================================
// Skybox Preset Reloading
// ============================================================================

void reloadSkyboxPreset(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= SKYBOX_PRESET_COUNT) return;

    currentSkyboxPreset = presetIndex;

    // Delete old environment cubemap
    if (envCubemap) { glDeleteTextures(1, &envCubemap); envCubemap = 0; }

    // Load new cubemap from faces
    envCubemap = loadCubemapFromFaces(skyboxPresets[currentSkyboxPreset].faces);
    if (envCubemap == 0)
    {
        fprintf(gpFile, "Failed to reload skybox preset: %s\n", skyboxPresets[currentSkyboxPreset].name);
        return;
    }

    fprintf(gpFile, "Reloaded skybox preset: %s\n", skyboxPresets[currentSkyboxPreset].name);

    // Regenerate IBL maps for new environment
    generateIBLMaps(envCubemap); // from pbr.cpp
}
