// ocean.h
#pragma once

#include <GL/glew.h>
#include "vmath.h"

using namespace vmath;

struct OceanEngineBindings {
    // IBL
    GLuint irradianceMap, prefilterMap;
    bool   hasIBL;
    float  iblIntensity;
    // Light (engine globals)
    vec3  lightPos, lightColor, lightDir;
    float lightIntensity, lightRadius;
    int   lightType;
    // Shadow
    bool   shadowEnabled;
    float  shadowBias;
    mat4   shadowMatrix;
    GLuint shadowDepthTex;
    // Aerial perspective
    bool   aerialActive;
    GLuint aerialTransLUT, aerialSkyViewLUT;
    float  atmBotR, atmTopR, atmCamHeight, atmWorldScale, atmExposure;
    // Fog
    bool  fogEnabled;
    vec3  fogColor;
    float fogDensity, fogStart, fogEnd;
    int   fogType;
};

class Ocean
{
public:
    Ocean();
    ~Ocean();

    bool initialize(GLuint textureId);

    void render(
        float waveTime,
        mat4 modelMatrix,
        mat4 viewMatrix,
        mat4 projectionMatrix,
        vec3 cameraPosition,
        float waveHeight,
        float waveSpeed,
        float waveRadius,
        float wavePointiness,
        float* deepColor,
        float* shallowColor,
        float roughness,
        float fresnelF0,
        float foamStrength,
        float* foamColor,
        const OceanEngineBindings& eng
    );

    void uninitialize();

private:
    void createMesh();
    bool compileShaders();

    GLuint vao;
    GLuint vbo_position;
    GLuint vbo_normal;
    GLuint vbo_texcoord;
    GLuint vbo_index;
    GLuint texture;

    GLuint shaderProgramObject;

    // Matrices
    GLuint modelMatrixUniform;
    GLuint viewMatrixUniform;
    GLuint projectionMatrixUniform;
    GLuint shadowMatrixUniform;

    // Wave
    GLuint waveTimeUniform;
    GLuint waveHeightUniform;
    GLuint waveSpeedUniform;
    GLuint waveRadiusUniform;
    GLuint wavePointinessUniform;

    // Water colors
    GLuint deepColorUniform;
    GLuint shallowColorUniform;

    // Material
    GLuint roughnessUniform;
    GLuint fresnelF0Uniform;
    GLuint foamStrengthUniform;
    GLuint foamColorUniform;

    // View
    GLuint viewPositionUniform;

    // Normal map sampler (unit 0)
    GLuint samplerUniform;

    // IBL (units 1, 2)
    GLuint irradianceMapUniform;
    GLuint prefilterMapUniform;
    GLuint hasIBLUniform;
    GLuint iblIntensityUniform;

    // Light
    GLuint lightPosUniform;
    GLuint lightColorUniform;
    GLuint lightIntensityUniform;
    GLuint lightTypeUniform;
    GLuint lightDirUniform;
    GLuint lightRadiusUniform;

    // Shadow (unit 9)
    GLuint shadowMapUniform;
    GLuint shadowEnabledUniform;
    GLuint shadowBiasUniform;

    // Aerial perspective (units 3, 4)
    GLuint aerialPerspectiveUniform;
    GLuint aerialTransLUTUniform;
    GLuint aerialSkyViewLUTUniform;
    GLuint atmBotRUniform;
    GLuint atmTopRUniform;
    GLuint atmCamHeightUniform;
    GLuint atmWorldScaleUniform;
    GLuint atmExposureUniform;

    // Fog
    GLuint fogEnabledUniform;
    GLuint fogColorUniform;
    GLuint fogDensityUniform;
    GLuint fogStartUniform;
    GLuint fogEndUniform;
    GLuint fogTypeUniform;

    enum {
        AMC_ATTRIBUTE_POSITION = 0,
        AMC_ATTRIBUTE_NORMAL,
        AMC_ATTRIBUTE_TEXCOORD
    };
};
