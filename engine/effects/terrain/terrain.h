#ifndef TERRAIN_H
#define TERRAIN_H

#include <GL/gl.h>

// Heightmap and normal map texture generation
// Use createTerrainTextures when you need both — runs noise generation only once.
void  createTerrainTextures(int width, int depth, float frequency, float heightScale,
                             GLuint* outHeightTex, GLuint* outNormalTex);
GLuint createHeightMapTexture(int width, int depth, float frequency, float heightScale);
GLuint createNormalMapTexture(int width, int depth, float frequency, float heightScale);

// Terrain mesh lifecycle
Mesh* createTerrainMesh();
void renderTerrain(GLint HeightMap, mat4 modelMatrix);
void regenerateTerrainMesh();
void switchTerrainMaterial(int materialIndex);

// PNG export
bool exportHeightmapToPNG(const char* filename, int width, int depth, float frequency);
bool exportNormalmapToPNG(const char* filename, int width, int depth, float frequency);

#endif // TERRAIN_H
