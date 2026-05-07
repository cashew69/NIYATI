#include "userrenderimports.h"

// Global PBR Settings (now in engine.h)
extern vec3 lightPos;
extern vec3 lightColor;
extern float lightIntensity;
extern bool useIBL;
extern float iblIntensity;

vec3 viewPos = vec3(0.0f, 0.0f, 10.0f);

// Model Transform
vec3 modelPosition(0.0f, 0.0f, 0.0f);
vec3 modelRotation(90.0f, 0.0f, 0.0f);
vec3 modelScale(3.0f, 3.0f, 3.0f);

// PBR Debug Settings (now in modelloading.cpp)
extern bool debugDisableDiffuse;
extern bool debugDisableNormal;
extern bool debugDisableMetallic;
extern bool debugDisableRoughness;
extern bool debugDisableAO;
extern bool debugDisableEmissive;

extern bool debugOverrideRoughness;
extern bool debugOverrideMetallic;
extern float debugRoughness;
extern float debugMetallic;
extern float debugAOStrength;
extern float debugEmissiveIntensity;

void setCommonUniformsForCurrentProgram(ShaderProgram* program, const mat4 &proj, const mat4 &view, 
        vec3 lightPos, vec3 lightColor, vec3 viewPos)
{
    GLint loc;
    loc = glGetUniformLocation(program->id, "uProjection");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, proj);

    loc = glGetUniformLocation(program->id, "uView");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, view);

    loc = glGetUniformLocation(program->id, "uLightPos");
    if (loc != -1) glUniform3fv(loc, 1, lightPos);

    loc = glGetUniformLocation(program->id, "uLightColor");
    if (loc != -1) glUniform3fv(loc, 1, lightColor * lightIntensity); 

    loc = glGetUniformLocation(program->id, "uViewPos");
    if (loc != -1) glUniform3fv(loc, 1, viewPos);
}

extern void setDebugUniforms(ShaderProgram* program);

void renderUserMeshes(GLint HeightMap)
{
    // Update Camera View Pos for Shader
    if (mainCamera) {
        viewPos = mainCamera->position;
    }

    // PBR Helmet
    if (pbrShaderProgram && pbrShaderProgram->id && helmetMeshes && helmetMeshCount > 0)
    {
        glUseProgram(pbrShaderProgram->id);
        setCommonUniformsForCurrentProgram(pbrShaderProgram, perspectiveProjectionMatrix, viewMatrix, lightPos, lightColor, viewPos);
        setDebugUniforms(pbrShaderProgram);
        
        // IBL Uniforms - set BEFORE drawing
        glUniform1i(glGetUniformLocation(pbrShaderProgram->id, "uHasIBL"), useIBL);
        glUniform1f(glGetUniformLocation(pbrShaderProgram->id, "uIBLIntensity"), iblIntensity);
        
        // Bind IBL Maps
        if (useIBL) {
            bindIBL(pbrShaderProgram);
        }

        for (int i = 0; i < helmetMeshCount; i++) {
             if (helmetMeshes[i].transform == NULL) {
                helmetMeshes[i].transform = createTransform();
            }
            
            setPosition(helmetMeshes[i].transform, modelPosition);
            setRotation(helmetMeshes[i].transform, modelRotation); 
            setScale(helmetMeshes[i].transform, modelScale);

            mat4 modelMatrix = getWorldMatrix(helmetMeshes[i].transform);
            GLint modelLoc = glGetUniformLocation(pbrShaderProgram->id, "uModel");
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMatrix);

            setMaterialUniforms(pbrShaderProgram, &helmetMeshes[i].material);

            glBindVertexArray(helmetMeshes[i].vao);
            if (helmetMeshes[i].indexCount > 0) {
                 glDrawElements(GL_TRIANGLES, helmetMeshes[i].indexCount, GL_UNSIGNED_INT, NULL);
            }
            glBindVertexArray(0);
        }
    }
    
    // Render Skybox (Last)
    renderSkybox(viewMatrix, perspectiveProjectionMatrix);

    
    glUseProgram(0);
}

