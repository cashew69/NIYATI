#include "engine/engine.h"
#include "engine/utils/scenegraph.h"
#include "engine/platform.h"
#include "engine/utils/primitives.h"

extern ShaderProgram* sdfShaderProgram;
extern mat4 viewMatrix;
extern mat4 perspectiveProjectionMatrix;
extern vec3 GetActiveCameraPosition();
extern GLuint loadGLTexture(const char* filename);

// Engine lighting globals synced each frame by RenderSceneModels
extern vec3  lightPos;
extern vec3  lightColor;
extern vec3  lightDir;
extern int   lightType;
extern float lightIntensity;

static const int MAX_SDF_SHAPES = 8;

void sg_InitSDFNode(SceneNode* node) {
    (void)node;
}

void sg_RenderSDFScene(SceneNode* root, mat4 view, mat4 proj) {
    if (!root || !sdfShaderProgram) return;

    // Collect up to MAX_SDF_SHAPES SDF nodes
    SceneNode* sdfNodes[MAX_SDF_SHAPES] = {};
    int sdfCount = 0;
    auto findSDFs = [&](auto self, SceneNode* n) -> void {
        if (!n || sdfCount >= MAX_SDF_SHAPES) return;
        if (n->type == ENTITY_SDF) sdfNodes[sdfCount++] = n;
        for (int i = 0; i < n->num_children; i++) self(self, n->children[i]);
    };
    findSDFs(findSDFs, root);

    if (sdfCount == 0) return;

    SDFNodeData* d0 = &sdfNodes[0]->data.sdf;

    // Lazy-load texture for first node
    if (d0->texturePath[0] != '\0' && d0->textureID == 0)
        d0->textureID = loadGLTexture(d0->texturePath);

    // GL state — proper depth so SDF intersects scene geometry
    bool transparent = d0->opacity < 0.999f;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    if (transparent) {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glUseProgram(sdfShaderProgram->id);

    // Camera
    vec3 camPos = GetActiveCameraPosition();
    glUniform3fv(glGetUniformLocation(sdfShaderProgram->id, "u_cameraPos"), 1, (float*)&camPos);
    glUniformMatrix4fv(glGetUniformLocation(sdfShaderProgram->id, "u_view"),       1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(sdfShaderProgram->id, "u_projection"), 1, GL_FALSE, proj);

    // Build flat arrays for all collected SDF nodes
    float posArr[MAX_SDF_SHAPES * 3]    = {};
    float radiusArr[MAX_SDF_SHAPES]     = {};
    float colorArr[MAX_SDF_SHAPES * 3]  = {};
    for (int i = 0; i < sdfCount; i++) {
        posArr[i*3+0] = sdfNodes[i]->world_matrix[3][0];
        posArr[i*3+1] = sdfNodes[i]->world_matrix[3][1];
        posArr[i*3+2] = sdfNodes[i]->world_matrix[3][2];
        radiusArr[i]   = sdfNodes[i]->data.sdf.radius;
        colorArr[i*3+0] = sdfNodes[i]->data.sdf.color[0];
        colorArr[i*3+1] = sdfNodes[i]->data.sdf.color[1];
        colorArr[i*3+2] = sdfNodes[i]->data.sdf.color[2];
    }

    glUniform3fv(sdfShaderProgram->loc.uSdfPos,    sdfCount, posArr);
    glUniform1fv(sdfShaderProgram->loc.uSdfRadius, sdfCount, radiusArr);
    glUniform3fv(sdfShaderProgram->loc.uSdfColor,  sdfCount, colorArr);
    glUniform1i (sdfShaderProgram->loc.uSdfCount,  sdfCount);

    // Operation and raymarching settings from first node (global)
    glUniform1i(sdfShaderProgram->loc.uSdfOperation, d0->operation);
    glUniform1f(sdfShaderProgram->loc.uSdfSmoothK,   d0->smoothK);
    glUniform1i(sdfShaderProgram->loc.uSdfMaxSteps,  d0->maxSteps);
    glUniform1f(sdfShaderProgram->loc.uSdfSurfDist,  d0->surfDist);
    glUniform1f(sdfShaderProgram->loc.uSdfMaxDist,   d0->maxDist);

    // Material / opacity
    glUniform1f(sdfShaderProgram->loc.uSdfOpacity, d0->opacity);
    int hasTex = (d0->textureID != 0) ? 1 : 0;
    glUniform1i(sdfShaderProgram->loc.uSdf1HasTexture, hasTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hasTex ? d0->textureID : 0);
    glUniform1i(sdfShaderProgram->loc.uSdf1Texture, 0);

    // Lighting — pass engine globals into SDF shader
    glUniform3fv(sdfShaderProgram->loc.uLightPos,       1, (float*)&lightPos);
    glUniform3fv(sdfShaderProgram->loc.uLightColor,     1, (float*)&lightColor);
    glUniform1f (sdfShaderProgram->loc.uLightIntensity, lightIntensity);
    glUniform1i (sdfShaderProgram->loc.uLightType,      lightType);
    glUniform3fv(sdfShaderProgram->loc.uLightDir,       1, (float*)&lightDir);

    renderQuad();

    // Restore GL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
