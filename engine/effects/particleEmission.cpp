#include "engine/dependancies/vmath.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Particle Data Structure
typedef struct {
    vec3 position;
    float size;
    vec4 color;
    vec3 velocity;
    float life;
    float maxLife;
} Particle;

#include <string.h>

// Particle Emitter System
typedef struct {
    Particle* particles;
    int maxParticles;
    int activeParticles;
    
    vec3 origin;
    
    float emissionRate;
    float accumulator;
    
    vec3 velocityBase;
    float velocitySpread;
    
    float lifeMin;
    float lifeMax;
    
    float sizeStart;
    float sizeEnd;
    vec4 colorStart;
    vec4 colorEnd;
    unsigned int vao;
    unsigned int vbo;
    unsigned int normalMapTexture;
    unsigned int depthMaskTexture;
    float noiseScale;
    float distortionAmount;
    float normalMergeFactor;
    
    bool use3D;
    bool enableDistortion;
    Mesh* mesh3D;
    int meshCount3D;
    
    float emissionMix;
    float emissionStrength;
    vec3 emissionColor;
    float particleTransparency;
    
    char currentMeshPath[256];
    char loadedMeshPath[256];
    char currentNormalPath[256];
    char loadedNormalPath[256];
    
    ShaderProgram* shader;
    ShaderProgram* shader3D;
} ParticleEmitter;

// Random float between 0.0 and 1.0
static float p_rand01() {
    return (float)rand() / (float)RAND_MAX;
}

// Random float between -1.0 and 1.0
static float p_rand11() {
    return p_rand01() * 2.0f - 1.0f;
}

// Create a configured Particle Emitter
static ParticleEmitter* createParticleEmitter(int maxParticles) {
    ParticleEmitter* emitter = (ParticleEmitter*)malloc(sizeof(ParticleEmitter));
    emitter->maxParticles = maxParticles;
    emitter->activeParticles = 0;
    emitter->particles = (Particle*)malloc(sizeof(Particle) * maxParticles);
    
    // Default smoke-like parameters
    emitter->origin = vec3(0.0f, 0.0f, 0.0f);
    emitter->emissionRate = 0.0f;
    emitter->accumulator = 0.0f;
    emitter->velocityBase = vec3(0.0f, 1.0f, 0.0f);
    emitter->velocitySpread = 0.5f;
    emitter->lifeMin = 1.0f;
    emitter->lifeMax = 2.0f;
    emitter->sizeStart = 1.0f;
    emitter->sizeEnd = 0.0f;
    emitter->colorStart = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    emitter->colorEnd = vec4(1.0f, 1.0f, 1.0f, 0.0f);
    emitter->normalMapTexture = 0;
    emitter->depthMaskTexture = 0;
    emitter->noiseScale = 0.15f;
    emitter->distortionAmount = 0.25f;
    emitter->normalMergeFactor = 1.0f;
    
    emitter->use3D = false;
    emitter->enableDistortion = true;
    emitter->mesh3D = NULL;
    emitter->meshCount3D = 0;
    
    emitter->emissionMix = 0.0f;
    emitter->emissionStrength = 1.0f;
    emitter->emissionColor = vec3(1.0f, 0.4f, 0.1f);
    emitter->particleTransparency = 1.0f;
    
    strcpy(emitter->currentMeshPath, "engine/effects/icosphere_particle.obj");
    strcpy(emitter->loadedMeshPath, "");
    strcpy(emitter->currentNormalPath, "engine/effects/sphere_normal_map.png");
    strcpy(emitter->loadedNormalPath, "");
    
    glGenVertexArrays(1, &emitter->vao);
    glGenBuffers(1, &emitter->vbo);
    
    glBindVertexArray(emitter->vao);
    glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(maxParticles * 8 * sizeof(float)), NULL, GL_DYNAMIC_DRAW);
    
    // Position Attribute (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // Size Attribute (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // Color Attribute (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    emitter->shader = NULL;
    const char* shaderFiles[5] = { "engine/shaders/particle.vert", NULL, NULL, NULL, "engine/shaders/particle.frag" };
    buildShaderProgramFromFiles(shaderFiles, 5, &emitter->shader, NULL, NULL, 0);
    
    emitter->shader3D = NULL;
    const char* shaderFiles3D[5] = { "engine/shaders/particle3D.vert", NULL, NULL, NULL, "engine/shaders/particle3D.frag" };
    buildShaderProgramFromFiles(shaderFiles3D, 5, &emitter->shader3D, NULL, NULL, 0);
    
    return emitter;
}

// Emits a single particle randomly from the origin parameter definitions
static void emitParticle(ParticleEmitter* emitter) {
    if (emitter->activeParticles >= emitter->maxParticles) return;
    
    Particle* p = &emitter->particles[emitter->activeParticles];
    
    p->position = emitter->origin;
    
    // Generate spherical spread direction
    float theta = p_rand01() * 2.0f * M_PI;
    float phi = acos(p_rand11()); 
    vec3 dir(sin(phi)*cos(theta), cos(phi), sin(phi)*sin(theta));
    
    p->velocity = emitter->velocityBase + (dir * emitter->velocitySpread * p_rand01());
    p->maxLife = emitter->lifeMin + p_rand01() * (emitter->lifeMax - emitter->lifeMin);
    p->life = p->maxLife;
    p->size = emitter->sizeStart;
    p->color = emitter->colorStart;
    
    emitter->activeParticles++;
}

// Updates particle lifecycle, movement, color, and size
static void updateParticleEmitter(ParticleEmitter* emitter, float dt) {
    if (emitter->emissionRate > 0.0f) {
        emitter->accumulator += dt;
        float timePerParticle = 1.0f / emitter->emissionRate;
        while (emitter->accumulator > timePerParticle && emitter->activeParticles < emitter->maxParticles) {
            emitParticle(emitter);
            emitter->accumulator -= timePerParticle;
        }
        
        if (emitter->accumulator > 0.0f && emitter->activeParticles >= emitter->maxParticles) {
            emitter->accumulator = 0.0f; // Prevent looping unnecessarily if particles cap
        }
    }
    
    for (int i = 0; i < emitter->activeParticles; ) {
        Particle* p = &emitter->particles[i];
        p->life -= dt;
        if (p->life <= 0.0f) {
            // Swap with last active element to delete
            *p = emitter->particles[emitter->activeParticles - 1];
            emitter->activeParticles--;
        } else {
            // Update movement
            p->position += p->velocity * dt;
            
            float t = 1.0f - (p->life / p->maxLife); // Normalized interpolation 0 -> 1
            
            // Apply interpolations
            p->size = emitter->sizeStart + t * (emitter->sizeEnd - emitter->sizeStart);
            p->color = emitter->colorStart + t * (emitter->colorEnd - emitter->colorStart);
            
            i++;
        }
    }
}

// Push to OpenGL pipeline using mapped dynamic buffers
static void renderParticleEmitter(ParticleEmitter* emitter, const mat4& viewMatrix, const mat4& projectionMatrix, vec3 lightPos = vec3(10.0f, 10.0f, 10.0f), vec3 lightColor = vec3(1.0f, 1.0f, 1.0f), float ambientStrength = 0.5f) {
    if (emitter->activeParticles == 0 || !emitter->shader) return;

    
    // Pack our data structure to float buffer
    float* packedData = (float*)malloc(emitter->activeParticles * 8 * sizeof(float));
    for (int i = 0; i < emitter->activeParticles; i++) {
        Particle* p = &emitter->particles[i];
        int idx = i * 8;
        packedData[idx+0] = p->position[0];
        packedData[idx+1] = p->position[1];
        packedData[idx+2] = p->position[2];
        packedData[idx+3] = p->size;
        packedData[idx+4] = p->color[0];
        packedData[idx+5] = p->color[1];
        packedData[idx+6] = p->color[2];
        packedData[idx+7] = p->color[3];
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(emitter->activeParticles * 8 * sizeof(float)), packedData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    free(packedData);
    
    // Dynamic asset loading
    if (emitter->use3D && strcmp(emitter->currentMeshPath, emitter->loadedMeshPath) != 0) {
        if (emitter->mesh3D) {
            freeModel(emitter->mesh3D, emitter->meshCount3D);
            emitter->mesh3D = NULL;
        }
        if (strlen(emitter->currentMeshPath) > 0) {
            loadModel(emitter->currentMeshPath, &emitter->mesh3D, &emitter->meshCount3D, 1.0f);
            if (emitter->mesh3D && emitter->meshCount3D > 0) {
                glBindVertexArray(emitter->mesh3D[0].vao);
                glBindBuffer(GL_ARRAY_BUFFER, emitter->vbo);
                
                glEnableVertexAttribArray(4);
                glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
                glVertexAttribDivisor(4, 1);
                
                glEnableVertexAttribArray(5);
                glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                glVertexAttribDivisor(5, 1);
                
                glEnableVertexAttribArray(6);
                glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
                glVertexAttribDivisor(6, 1);
                
                glBindVertexArray(0);
            }
        }
        strcpy(emitter->loadedMeshPath, emitter->currentMeshPath);
    }
    
    if (strcmp(emitter->currentNormalPath, emitter->loadedNormalPath) != 0) {
        if (emitter->normalMapTexture) {
            glDeleteTextures(1, &emitter->normalMapTexture);
            emitter->normalMapTexture = 0;
        }
        if (strlen(emitter->currentNormalPath) > 0) {
            emitter->normalMapTexture = loadGLTexture(emitter->currentNormalPath);
        }
        strcpy(emitter->loadedNormalPath, emitter->currentNormalPath);
    }
    
    // Disable Depth Write, Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    ShaderProgram* activeShader = emitter->use3D ? emitter->shader3D : emitter->shader;
    if (!activeShader) return;
    
    glUseProgram(activeShader->id);
    
    GLint loc;
    loc = glGetUniformLocation(activeShader->id, "uProjection");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, projectionMatrix);
    
    loc = glGetUniformLocation(activeShader->id, "uView");
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, viewMatrix);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, emitter->normalMapTexture);
    loc = glGetUniformLocation(activeShader->id, "uParticleNormalMap");
    if (loc != -1) glUniform1i(loc, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, emitter->depthMaskTexture);
    loc = glGetUniformLocation(activeShader->id, "uDepthMask");
    if (loc != -1) glUniform1i(loc, 1);

    loc = glGetUniformLocation(activeShader->id, "uNoiseScale");
    if (loc != -1) glUniform1f(loc, emitter->noiseScale);
    
    loc = glGetUniformLocation(activeShader->id, "uDistortion");
    if (loc != -1) glUniform1f(loc, emitter->distortionAmount);
    
    loc = glGetUniformLocation(activeShader->id, "uMergeNormals");
    if (loc != -1) glUniform1f(loc, emitter->normalMergeFactor);

    loc = glGetUniformLocation(activeShader->id, "uLightPos");
    if (loc != -1) glUniform3fv(loc, 1, lightPos);
    loc = glGetUniformLocation(activeShader->id, "uLightColor");
    if (loc != -1) glUniform3fv(loc, 1, lightColor);
    loc = glGetUniformLocation(activeShader->id, "uAmbientStrength");
    if (loc != -1) glUniform1f(loc, ambientStrength);
    
    loc = glGetUniformLocation(activeShader->id, "uEnableDistortion");
    if (loc != -1) glUniform1i(loc, emitter->enableDistortion);
    
    if (emitter->use3D) {
        loc = glGetUniformLocation(activeShader->id, "uEmissionMix");
        if (loc != -1) glUniform1f(loc, emitter->emissionMix);
        loc = glGetUniformLocation(activeShader->id, "uEmissionStrength");
        if (loc != -1) glUniform1f(loc, emitter->emissionStrength);
        loc = glGetUniformLocation(activeShader->id, "uEmissionColor");
        if (loc != -1) glUniform3fv(loc, 1, emitter->emissionColor);
        loc = glGetUniformLocation(activeShader->id, "uParticleTransparency");
        if (loc != -1) glUniform1f(loc, emitter->particleTransparency);
    }
    
    if (emitter->use3D && emitter->mesh3D && emitter->meshCount3D > 0) {
        glBindVertexArray(emitter->mesh3D[0].vao);
        glDrawElementsInstanced(GL_TRIANGLES, emitter->mesh3D[0].indexCount, GL_UNSIGNED_INT, 0, emitter->activeParticles);
        glBindVertexArray(0);
    } else {
        glBindVertexArray(emitter->vao);
        glDrawArrays(GL_POINTS, 0, emitter->activeParticles);
        glBindVertexArray(0);
    }
    
    // Return states to Engine defaults
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(0);
}

// Cleanup and memory handling
static void destroyParticleEmitter(ParticleEmitter* emitter) {
    if (!emitter) return;
    if (emitter->particles) free(emitter->particles);
    glDeleteBuffers(1, &emitter->vbo);
    glDeleteVertexArrays(1, &emitter->vao);
    free(emitter);
}
