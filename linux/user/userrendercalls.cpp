#include "userrenderimports.h"

// Global PBR Settings
vec3 lightPos = vec3(0.0f, 10.0f, 10.0f);
vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
float lightIntensity = 1.0f;
vec3 viewPos = vec3(0.0f, 0.0f, 10.0f);

// Model Transform
vec3 modelPosition(0.0f, 0.0f, 0.0f);
vec3 modelRotation(90.0f, 0.0f, 0.0f);
vec3 modelScale(3.0f, 3.0f, 3.0f);

// Debug Texture Toggles
bool debugDisableDiffuse = false;
bool debugDisableNormal = false;
bool debugDisableMetallic = false;
bool debugDisableRoughness = false;
bool debugDisableAO = false;
bool debugDisableEmissive = false;

// Debug Parameter Overrides
bool debugOverrideRoughness = false;
bool debugOverrideMetallic = false;
float debugRoughness = 0.5f;
float debugMetallic = 0.0f;
float debugAOStrength = 1.0f;
float debugEmissiveIntensity = 5.0f;

// IBL Settings
bool useIBL = true;
float iblIntensity = 1.0f;

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

void setDebugUniforms(ShaderProgram* program)
{
    GLint loc;
    
    // Texture disable toggles
    loc = glGetUniformLocation(program->id, "uDebugDisableDiffuseTex");
    if (loc != -1) glUniform1i(loc, debugDisableDiffuse);
    
    loc = glGetUniformLocation(program->id, "uDebugDisableNormalTex");
    if (loc != -1) glUniform1i(loc, debugDisableNormal);
    
    loc = glGetUniformLocation(program->id, "uDebugDisableMetallicTex");
    if (loc != -1) glUniform1i(loc, debugDisableMetallic);
    
    loc = glGetUniformLocation(program->id, "uDebugDisableRoughnessTex");
    if (loc != -1) glUniform1i(loc, debugDisableRoughness);
    
    loc = glGetUniformLocation(program->id, "uDebugDisableAOTex");
    if (loc != -1) glUniform1i(loc, debugDisableAO);
    
    loc = glGetUniformLocation(program->id, "uDebugDisableEmissiveTex");
    if (loc != -1) glUniform1i(loc, debugDisableEmissive);
    
    // Parameter overrides
    loc = glGetUniformLocation(program->id, "uDebugOverrideRoughness");
    if (loc != -1) glUniform1i(loc, debugOverrideRoughness);
    
    loc = glGetUniformLocation(program->id, "uDebugOverrideMetallic");
    if (loc != -1) glUniform1i(loc, debugOverrideMetallic);
    
    loc = glGetUniformLocation(program->id, "uDebugRoughness");
    if (loc != -1) glUniform1f(loc, debugRoughness);
    
    loc = glGetUniformLocation(program->id, "uDebugMetallic");
    if (loc != -1) glUniform1f(loc, debugMetallic);
    
    loc = glGetUniformLocation(program->id, "uDebugAOStrength");
    if (loc != -1) glUniform1f(loc, debugAOStrength);
    
    loc = glGetUniformLocation(program->id, "uDebugEmissiveIntensity");
    if (loc != -1) glUniform1f(loc, debugEmissiveIntensity);
}

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

