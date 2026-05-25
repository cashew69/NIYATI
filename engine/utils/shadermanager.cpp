#include "shadermanager.h"

static void cacheShaderLocations(ShaderProgram* program) {
    if (!program || !program->id) return;
    GLuint id = program->id;

    program->loc.uModel      = glGetUniformLocation(id, "uModel");
    program->loc.uView       = glGetUniformLocation(id, "uView");
    program->loc.uProjection = glGetUniformLocation(id, "uProjection");

    program->loc.uViewPos    = glGetUniformLocation(id, "uViewPos");
    program->loc.uLightPos   = glGetUniformLocation(id, "uLightPos");
    program->loc.uLightColor = glGetUniformLocation(id, "uLightColor");
    program->loc.uLightIntensity = glGetUniformLocation(id, "uLightIntensity");
    program->loc.uLightType  = glGetUniformLocation(id, "uLightType");
    program->loc.uLightDir   = glGetUniformLocation(id, "uLightDir");
    program->loc.uLightRadius = glGetUniformLocation(id, "uLightRadius");
    program->loc.uInnerCutoff = glGetUniformLocation(id, "uInnerCutoff");
    program->loc.uOuterCutoff = glGetUniformLocation(id, "uOuterCutoff");

    program->loc.uHasIBL       = glGetUniformLocation(id, "uHasIBL");
    program->loc.uIBLIntensity = glGetUniformLocation(id, "uIBLIntensity");
    program->loc.irradianceMap = glGetUniformLocation(id, "irradianceMap");
    program->loc.prefilterMap  = glGetUniformLocation(id, "prefilterMap");
    program->loc.brdfLUT       = glGetUniformLocation(id, "brdfLUT");

    program->loc.uDiffuseColor      = glGetUniformLocation(id, "uDiffuseColor");
    program->loc.uHasDiffuseTexture = glGetUniformLocation(id, "uHasDiffuseTexture");
    program->loc.uDiffuseTexture    = glGetUniformLocation(id, "uDiffuseTexture");
    program->loc.uHasNormalTexture  = glGetUniformLocation(id, "uHasNormalTexture");
    program->loc.uNormalTexture     = glGetUniformLocation(id, "uNormalTexture");
    program->loc.uHasMetallicMap    = glGetUniformLocation(id, "uHasMetallicMap");
    program->loc.uMetallicMap       = glGetUniformLocation(id, "uMetallicMap");
    program->loc.uHasRoughnessMap   = glGetUniformLocation(id, "uHasRoughnessMap");
    program->loc.uRoughnessMap      = glGetUniformLocation(id, "uRoughnessMap");
    program->loc.uHasAOMap          = glGetUniformLocation(id, "uHasAOMap");
    program->loc.uAOMap             = glGetUniformLocation(id, "uAOMap");
    program->loc.uHasEmissiveMap    = glGetUniformLocation(id, "uHasEmissiveMap");
    program->loc.uEmissiveMap       = glGetUniformLocation(id, "uEmissiveMap");
    program->loc.uRoughness         = glGetUniformLocation(id, "uRoughness");
    program->loc.uMetalness         = glGetUniformLocation(id, "uMetalness");
    program->loc.uShininess         = glGetUniformLocation(id, "uShininess");
    program->loc.uOpacity           = glGetUniformLocation(id, "uOpacity");
    program->loc.uIsEmissive        = glGetUniformLocation(id, "uIsEmissive");

    program->loc.uHeightMap          = glGetUniformLocation(id, "uHeightMap");
    program->loc.uDisplacementMap    = glGetUniformLocation(id, "uDisplacementMap");
    program->loc.uHasDisplacementMap = glGetUniformLocation(id, "uHasDisplacementMap");
    program->loc.uDisplacementScale  = glGetUniformLocation(id, "uDisplacementScale");
    program->loc.uTexelSize          = glGetUniformLocation(id, "uTexelSize");
    program->loc.uUVScale            = glGetUniformLocation(id, "uUVScale");
    program->loc.uTessLevelInner     = glGetUniformLocation(id, "uTessLevelInner");
    program->loc.uTessLevelOuter     = glGetUniformLocation(id, "uTessLevelOuter");

    program->loc.view       = glGetUniformLocation(id, "view");
    program->loc.projection = glGetUniformLocation(id, "projection");
    program->loc.model      = glGetUniformLocation(id, "model");

    program->loc.uFogColor   = glGetUniformLocation(id, "uFogColor");
    program->loc.uFogDensity = glGetUniformLocation(id, "uFogDensity");
    program->loc.uFogStart   = glGetUniformLocation(id, "uFogStart");
    program->loc.uFogEnd     = glGetUniformLocation(id, "uFogEnd");
    program->loc.uFogType    = glGetUniformLocation(id, "uFogType");
    program->loc.uFogEnabled = glGetUniformLocation(id, "uFogEnabled");

    program->loc.uShadowMap     = glGetUniformLocation(id, "uShadowMap");
    program->loc.uShadowMatrix  = glGetUniformLocation(id, "uShadowMatrix");
    program->loc.uShadowEnabled = glGetUniformLocation(id, "uShadowEnabled");
    program->loc.uShadowBias    = glGetUniformLocation(id, "uShadowBias");

    program->loc.uAerialPerspective      = glGetUniformLocation(id, "uAerialPerspective");
    program->loc.uAerialTransmittanceLUT  = glGetUniformLocation(id, "uAerialTransmittanceLUT");
    program->loc.uAerialSkyViewLUT       = glGetUniformLocation(id, "uAerialSkyViewLUT");
    program->loc.uAtmBotR                = glGetUniformLocation(id, "uAtmBotR");
    program->loc.uAtmTopR                = glGetUniformLocation(id, "uAtmTopR");
    program->loc.uAtmCamHeight           = glGetUniformLocation(id, "uAtmCamHeight");
    program->loc.uAtmWorldScale          = glGetUniformLocation(id, "uAtmWorldScale");
    program->loc.uAtmExposure            = glGetUniformLocation(id, "uAtmExposure");

    // NEW FIELDS (Added at end to match ShaderLocations struct)
    program->loc.uEnableStochastic   = glGetUniformLocation(id, "uEnableStochastic");
    program->loc.uStochasticContrast = glGetUniformLocation(id, "uStochasticContrast");
    program->loc.uStochasticScale    = glGetUniformLocation(id, "uStochasticScale");
}

void InitializeShaders() {
#if defined(_WIN32) || defined(_glfw3_h_)
    // 1. Build Editor Line Shader
    if (!lineShaderProgram) {
        const char *lineShaderFiles[5] = {
            "engine/shaders/lineVert.glsl",
            NULL, NULL, NULL,
            "engine/shaders/lineFrag.glsl"
        };
        buildShaderProgramFromFiles(lineShaderFiles, 5, &lineShaderProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(lineShaderProgram);
    }

    if (!iconShaderProgram) {
        const char* iconFiles[5] = {
            "engine/shaders/icon.vert",
            NULL, NULL, NULL,
            "engine/shaders/icon.frag"
        };
        buildShaderProgramFromFiles(iconFiles, 5, &iconShaderProgram, attribNames, attribIndices, 4);
    }
#endif

    // 2. Build Default Scene Shaders
    if (!lambertShaderProgram) {
        const char* sceneFiles[5] = {
            "engine/shaders/vertex_shader.glsl",
            NULL, NULL, NULL,
            "engine/shaders/main_fs[lambart].glsl"
        };
        buildShaderProgramFromFiles(sceneFiles, 5, &lambertShaderProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(lambertShaderProgram);
    }

    // 3. Instancing shader
    if (!instancedProgram) {
        const char* sceneFiles[5] = {
            "engine/shaders/instance.vert",
            NULL, NULL, NULL,
            "engine/shaders/PBR/pbrFrag.glsl"
        };
        buildShaderProgramFromFiles(sceneFiles, 5, &instancedProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(instancedProgram);
    }

    if (!pbrShaderProgram) {
        const char* pbrFiles[5] = {
            "engine/shaders/vertex_shader.glsl",
            NULL, NULL, NULL,
            "engine/shaders/PBR/pbrFrag.glsl"
        };
        buildShaderProgramFromFiles(pbrFiles, 5, &pbrShaderProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(pbrShaderProgram);
    }

    if (!tessellationShaderProgram) {
        const char *tessShaderFiles[5] = {
            "engine/shaders/vertex_shader.glsl",
            "engine/effects/terrain/main_tcs.glsl",
            "engine/effects/terrain/main_tes.glsl",
            NULL,
            "engine/shaders/PBR/pbrFrag.glsl"
        };
        buildShaderProgramFromFiles(tessShaderFiles, 5, &tessellationShaderProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(tessellationShaderProgram);
    }

    if (!instancedShadowProgram) {
        const char* shadowFiles[5] = {
            "engine/shaders/shadow_instance_depth.vert",
            NULL, NULL, NULL,
            "engine/shaders/shadow_depth.frag"
        };
        buildShaderProgramFromFiles(shadowFiles, 5, &instancedShadowProgram, attribNames, attribIndices, 4);
        cacheShaderLocations(instancedShadowProgram);
    }
}
