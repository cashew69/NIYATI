#include "engine/engine.h"
#include "engine/effects/ocean/ocean.h"
#include "engine/utils/scenegraph.h"

// Engine globals needed for ocean rendering
extern vec3   lightPos;
extern vec3   lightColor;
extern vec3   lightDir;
extern float  lightIntensity;
extern float  lightRadius;
extern int    lightType;
extern bool   useIBL;
extern float  iblIntensity;

// IBL maps (defined in pbr.cpp)
extern unsigned int irradianceMap;
extern unsigned int prefilterMap;

// Shadow (defined in scenegraph.cpp)
extern bool   g_ShadowActive;
extern mat4   g_ShadowSBPV;
extern GLuint g_ShadowDepthTexID;
extern float  g_ShadowBias;

// Aerial perspective (defined in scenegraph.cpp)
extern bool   g_AerialActive;
extern GLuint g_AerialTransLUT;
extern GLuint g_AerialSkyViewLUT;
extern float  g_AtmBotR;
extern float  g_AtmTopR;
extern float  g_AtmCamHeight;
extern float  g_AtmWorldScale;
extern float  g_AtmExposure;

// Fog (defined in scenegraph.cpp)
extern bool  g_FogEnabled;
extern vec3  g_FogColor;
extern float g_FogDensity;
extern float g_FogStart;
extern float g_FogEnd;
extern int   g_FogType;

// Time (defined in platform.h / platform.cpp)
extern float g_Time;

extern vec3 GetActiveCameraPosition();
extern GLuint loadGLTexture(const char* filename);

void sg_InitOceanNode(SceneNode* node) {
    if (!node || node->type != ENTITY_OCEAN) return;
    OceanNodeData* od = &node->data.ocean;

    // Load normal map texture
    if (od->normalMapPath[0] != '\0') {
        if (od->normalMapTex != 0) glDeleteTextures(1, &od->normalMapTex);
        od->normalMapTex = loadGLTexture(od->normalMapPath);
    }
    if (od->normalMapTex == 0) {
        // fallback: 1x1 flat normal (128, 128, 255)
        unsigned char flat[4] = { 128, 128, 255, 255 };
        glGenTextures(1, &od->normalMapTex);
        glBindTexture(GL_TEXTURE_2D, od->normalMapTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, flat);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Create Ocean object
    if (od->oceanObj) {
        Ocean* old = (Ocean*)od->oceanObj;
        old->uninitialize();
        delete old;
        od->oceanObj = nullptr;
    }
    Ocean* ocean = new Ocean();
    if (ocean->initialize(od->normalMapTex)) {
        od->oceanObj = ocean;
    } else {
        delete ocean;
    }
}

void sg_DrawOceanNode(SceneNode* node, mat4 view, mat4 proj) {
    if (!node || node->type != ENTITY_OCEAN) return;
    OceanNodeData* od = &node->data.ocean;
    if (!od->oceanObj) return;
    Ocean* ocean = (Ocean*)od->oceanObj;

    // Apply storm intensity multipliers
    float s = od->stormIntensity;
    float h  = od->waveHeight    * (1.0f + s * 3.0f);
    float sp = od->waveSpeed     * (1.0f + s * 1.5f);
    float r  = od->waveRadius    / (1.0f + s * 2.0f);
    float pt = od->wavePointiness + s * 0.8f;

    // Build engine bindings
    OceanEngineBindings eng;
    eng.irradianceMap = irradianceMap;
    eng.prefilterMap  = prefilterMap;
    eng.hasIBL        = useIBL && irradianceMap != 0;
    eng.iblIntensity  = iblIntensity;

    eng.lightPos       = lightPos;
    eng.lightColor     = lightColor;
    eng.lightDir       = lightDir;
    eng.lightIntensity = lightIntensity;
    eng.lightRadius    = lightRadius;
    eng.lightType      = lightType;

    eng.shadowEnabled  = g_ShadowActive;
    eng.shadowBias     = g_ShadowBias;
    eng.shadowMatrix   = g_ShadowSBPV;
    eng.shadowDepthTex = g_ShadowDepthTexID;

    eng.aerialActive    = g_AerialActive;
    eng.aerialTransLUT  = g_AerialTransLUT;
    eng.aerialSkyViewLUT= g_AerialSkyViewLUT;
    eng.atmBotR         = g_AtmBotR;
    eng.atmTopR         = g_AtmTopR;
    eng.atmCamHeight    = g_AtmCamHeight;
    eng.atmWorldScale   = g_AtmWorldScale;
    eng.atmExposure     = g_AtmExposure;

    eng.fogEnabled  = g_FogEnabled;
    eng.fogColor    = g_FogColor;
    eng.fogDensity  = g_FogDensity;
    eng.fogStart    = g_FogStart;
    eng.fogEnd      = g_FogEnd;
    eng.fogType     = g_FogType;

    vec3 camPos = GetActiveCameraPosition();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ocean->render(
        g_Time,
        node->world_matrix, view, proj,
        camPos,
        h, sp, r, pt,
        od->deepColor, od->shallowColor,
        od->roughness, od->fresnelF0,
        od->foamStrength, od->foamColor,
        eng
    );

    glDisable(GL_BLEND);
}

SceneNode* AddSceneOcean(const char* name, SceneNode* parent) {
    SceneNode* node = sg_CreateNode(ENTITY_OCEAN, name);
    OceanNodeData* od = &node->data.ocean;

    od->waveHeight     = 1.0f;
    od->waveSpeed      = 1.0f;
    od->waveRadius     = 1.0f;
    od->wavePointiness = 1.0f;
    od->stormIntensity = 0.0f;

    od->deepColor[0]    = 0.01f; od->deepColor[1]    = 0.05f; od->deepColor[2]    = 0.12f;
    od->shallowColor[0] = 0.05f; od->shallowColor[1] = 0.25f; od->shallowColor[2] = 0.30f;
    od->foamColor[0]    = 0.90f; od->foamColor[1]    = 0.92f; od->foamColor[2]    = 0.95f;

    od->roughness    = 0.10f;
    od->fresnelF0    = 0.02f;
    od->foamStrength = 0.18f;

    od->normalMapPath[0] = '\0';
    od->normalMapTex     = 0;
    od->oceanObj         = nullptr;

    if (parent) sg_AddChild(parent, node);
    sg_InitNode(node);
    return node;
}
