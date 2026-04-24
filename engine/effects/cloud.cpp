#include "engine/platform.h"
#include "engine/dependancies/vmath.h"
#include "engine/core/gl/structs.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "engine/core/logger.h"


// Cloud sphere data (Max 256 spheres: xyz = position, w = radius)
static float cloudSpheres[256 * 4]; 
static int   numCloudSpheres = 0;

// Globals for clouds
GLuint cloudTexture = 0;
GLuint cloudVAO = 0;
GLuint cloudVBO = 0;
static GLint resLoc = 0;

// Grid parameters
int gridX = 3;
int gridZ = 3;
float envSpacing = 20.0f;
float envScale = 1.0f;
int spheresPerEnvMin = 3;
int spheresPerEnvMax = 8;
int cloudTexRes = 128;

// Real-time tweakable params
float cloudDensityScale = 20.0f;
int   cloudMaxSteps = 128;
float cloudStepSize = 0.02f;
vec3  cloudBoxPos(-10.0f, 0.0f, -10.0f);
vec3  cloudBoxScale(20.0f, 10.0f, 20.0f);

// Externs from engine
extern ShaderProgram *VolumeRenderingProgram;
extern mat4 viewMatrix;
extern mat4 perspectiveProjectionMatrix;
extern Camera *mainCamera;
// extern FILE *gpFile; // Now using logger.h


// From noise.c
extern float threeDNoise(float x, float y, float z);
extern float g_powerCurve;
extern float g_turbulence;

// Generate a grid of random sphere clusters
void generateCloudSpheres()
{
    srand((unsigned int)time(NULL));
    numCloudSpheres = 0;

    float startX = - (gridX - 1) * envSpacing / 2.0f;
    float startZ = - (gridZ - 1) * envSpacing / 2.0f;

    for (int gx = 0; gx < gridX; gx++) {
        for (int gz = 0; gz < gridZ; gz++) {
            float cx = startX + gx * envSpacing;
            float cy = 4.0f;
            float cz = startZ + gz * envSpacing;

            if (spheresPerEnvMin > spheresPerEnvMax) spheresPerEnvMin = spheresPerEnvMax;
            int envSpheres = spheresPerEnvMin + (rand() % (spheresPerEnvMax - spheresPerEnvMin + 1));
            
            if (numCloudSpheres + envSpheres > 256) envSpheres = 256 - numCloudSpheres;
            if (envSpheres <= 0) break;

            int numLower = (int)(envSpheres * 0.6f);
            if (numLower < 2 && envSpheres >= 3) numLower = 2;
            int numUpper = envSpheres - numLower;

            for (int i = 0; i < numLower; i++) {
                float ox = ((float)rand() / (float)RAND_MAX) * 15.0f * envScale - 7.5f * envScale; 
                float oy = ((float)rand() / (float)RAND_MAX) * 0.5f * envScale - 0.25f * envScale;
                float oz = ((float)rand() / (float)RAND_MAX) * 3.0f * envScale - 1.5f * envScale;

                float r  = (2.0f + ((float)rand() / (float)RAND_MAX) * 1.0f) * envScale;

                cloudSpheres[numCloudSpheres * 4 + 0] = cx + ox;
                cloudSpheres[numCloudSpheres * 4 + 1] = cy + oy;
                cloudSpheres[numCloudSpheres * 4 + 2] = cz + oz;
                cloudSpheres[numCloudSpheres * 4 + 3] = r;
                numCloudSpheres++;
            }

            for (int i = 0; i < numUpper; i++) {
                float ox = ((float)rand() / (float)RAND_MAX) * 13.0f * envScale - 6.5f * envScale;
                float oy = ((float)rand() / (float)RAND_MAX) * 1.0f * envScale + 2.0f * envScale;
                float oz = ((float)rand() / (float)RAND_MAX) * 2.0f * envScale - 1.0f * envScale;

                float r  = (1.0f + ((float)rand() / (float)RAND_MAX) * 0.6f) * envScale;

                cloudSpheres[numCloudSpheres * 4 + 0] = cx + ox;
                cloudSpheres[numCloudSpheres * 4 + 1] = cy + oy;
                cloudSpheres[numCloudSpheres * 4 + 2] = cz + oz;
                cloudSpheres[numCloudSpheres * 4 + 3] = r;
                numCloudSpheres++;
            }
        }
    }

    LOG_I("Generated %d trail cloud spheres in a %dx%d Grid.", numCloudSpheres, gridX, gridZ);
}


void generateCloudTexture()
{
    int texWidth = cloudTexRes;
    int texHeight = cloudTexRes;
    int texDepth = cloudTexRes;

    // Allocate memory for a single channel (GL_RED)
    int totalPixels = texWidth * texHeight * texDepth;
    unsigned char* noisePixels = new unsigned char[totalPixels];

    // The scale determines the "zoom" level of the noise
    float scale = 0.05f;

    LOG_I("Generating 3D Noise Texture for Clouds (%dx%dx%d)...", texWidth, texHeight, texDepth);
    for (int z = 0; z < texDepth; ++z) {
        // Log progress every 32 slices
        if (z % 32 == 0 && z > 0) LOG_I("  ...generated %d/%d slices", z, texDepth);

        for (int y = 0; y < texHeight; ++y) {
            for (int x = 0; x < texWidth; ++x) {
                // Sample the noise function from noise.c
                float density = threeDNoise(x * scale, y * scale, z * scale);
                if (density < 0.0f) density = 0.0f;
                if (density > 1.0f) density = 1.0f;
                unsigned char byteVal = (unsigned char)(density * 255.0f);
                int index = (z * texWidth * texHeight) + (y * texWidth) + x;
                noisePixels[index] = byteVal;
            }
        }
    }
    LOG_I("3D Noise generation complete.");


    // Delete old texture if it exists to prevent memory leaks
    if (cloudTexture) glDeleteTextures(1, &cloudTexture);

    // Upload to GPU using engine's create3DTexture
    cloudTexture = create3DTexture(texWidth, texHeight, texDepth, noisePixels);

    // Free the CPU memory since OpenGL now has it
    delete[] noisePixels;
}

void initClouds()
{
    // Setup defaults for clouds
    g_powerCurve = 3.0f;
    g_turbulence = 0.5f;

    generateCloudSpheres();
    generateCloudTexture();

    // Cube vertices (Position, Normal, Color, TexCoord3D)
    // 12 floats per vertex
    float vertices[] = {
        // Back face
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,

        // Front face
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,

        // Left face
        0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,

        // Right face
        1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f,

        // Bottom face
        0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 0.0f,

        // Top face
        0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
    };

    glGenVertexArrays(1, &cloudVAO);
    glGenBuffers(1, &cloudVBO);

    glBindVertexArray(cloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position (Location 0)
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    // Normal (Location 1)
    glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_NORMAL);
    // Color (Location 2)
    glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_COLOR);
    // TexCoord3D (Location 3)
    glVertexAttribPointer(ATTRIB_TEXCOORD, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_TEXCOORD);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (VolumeRenderingProgram) {
        resLoc = glGetUniformLocation(VolumeRenderingProgram->id, "u_resolution");
    }
}

void renderClouds()
{
    if (!VolumeRenderingProgram) return;

    glUseProgram(VolumeRenderingProgram->id);

    // Set matrices
    // Translate and scale the cloud cube
    mat4 modelMatrix = vmath::translate(cloudBoxPos) * vmath::scale(cloudBoxScale);
    glUniformMatrix4fv(glGetUniformLocation(VolumeRenderingProgram->id, "uModel"), 1, GL_FALSE, modelMatrix);
    glUniformMatrix4fv(glGetUniformLocation(VolumeRenderingProgram->id, "uView"), 1, GL_FALSE, viewMatrix);
    glUniformMatrix4fv(glGetUniformLocation(VolumeRenderingProgram->id, "uProjection"), 1, GL_FALSE, perspectiveProjectionMatrix);

    // Set other uniforms
    if (mainCamera) {
        glUniform3fv(glGetUniformLocation(VolumeRenderingProgram->id, "uCameraPos"), 1, mainCamera->position);
    }
    glUniform1f(glGetUniformLocation(VolumeRenderingProgram->id, "uTime"), platformGetTime());
    glUniform2f(resLoc, 1920.0f, 1080.0f);

    // Upload cloud sphere data
    glUniform1i(glGetUniformLocation(VolumeRenderingProgram->id, "uNumCloudSpheres"), numCloudSpheres);
    glUniform4fv(glGetUniformLocation(VolumeRenderingProgram->id, "uCloudSpheres"), numCloudSpheres, cloudSpheres);

    // Bind 3D texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, cloudTexture);
    glUniform1i(glGetUniformLocation(VolumeRenderingProgram->id, "noiseTex3D"), 0);

    // Draw the cloud box
    glBindVertexArray(cloudVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glUseProgram(0);
}

void cleanupClouds()
{
    if (cloudTexture) {
        glDeleteTextures(1, &cloudTexture);
        cloudTexture = 0;
    }
    if (cloudVAO) {
        glDeleteVertexArrays(1, &cloudVAO);
        cloudVAO = 0;
    }
    if (cloudVBO) {
        glDeleteBuffers(1, &cloudVBO);
        cloudVBO = 0;
    }
}
