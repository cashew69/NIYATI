#ifndef TERRAIN_H
#define TERRAIN_H

#include <GL/gl.h>

// Heightmap and normal map texture generation
GLuint createHeightMapTexture(int width, int depth, float frequency, float heightScale);
GLuint createNormalMapTexture(int width, int depth, float frequency, float heightScale);

// Terrain mesh lifecycle
Mesh* createTerrainMesh();
void renderTerrain(GLint HeightMap);
void regenerateTerrainMesh();
void switchTerrainMaterial(int materialIndex);

// PNG export
bool exportHeightmapToPNG(const char* filename, int width, int depth, float frequency);
bool exportNormalmapToPNG(const char* filename, int width, int depth, float frequency);

#endif // TERRAIN_H
