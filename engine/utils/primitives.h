#pragma once
#include "engine/platform.h"

// Mesh primitives
void renderCube();
void renderQuad();
void renderSphere();
void renderPlane(float width, float length);

// Shared cubemap capture helpers
void createCaptureFBO(unsigned int* fbo, unsigned int* rbo, int size);
void restoreViewport();
void getCubemapCaptureMatrices(mat4* projOut, mat4 viewsOut[6]);
