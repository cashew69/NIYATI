#ifndef EDITOR_UTILS_H
#define EDITOR_UTILS_H

#include "engine/engine.h"

// Globals for Editor primitives (C style)
static GLuint gridVAO = 0;
static GLuint gridVBO = 0;
static int gridLineCount = 0;

static GLuint axisVAO = 0;
static GLuint axisVBO = 0;

static GLuint debugLineVAO = 0;
static GLuint debugLineVBO = 0;

void drawDebugLine(vec3 start, vec3 end, vec3 color, mat4 view, mat4 proj) {
    if (!lineShaderProgram) return;
    
    float vertices[] = {
        start[0], start[1], start[2], color[0], color[1], color[2],
        end[0], end[1], end[2], color[0], color[1], color[2]
    };
    
    if (debugLineVAO == 0) {
        glGenVertexArrays(1, &debugLineVAO);
        glGenBuffers(1, &debugLineVBO);
    }
    
    glBindVertexArray(debugLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, debugLineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_COLOR);
    
    glUseProgram(lineShaderProgram->id);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "projection"), 1, GL_FALSE, proj);
    mat4 model = mat4::identity();
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "model"), 1, GL_FALSE, model);
    
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
    glUseProgram(0);
}


void initEditorPrimitives() {
    // 1. Grid (C syntax: using dynamic allocation or fixed array)
    int size = 100;
    int step = 5;
    int numLines = ((size * 2) / step + 1) * 2;
    int numVertices = numLines * 2;
    int dataSize = numVertices * 6; // 3 pos + 3 color
    float* gridVertices = (float*)malloc(dataSize * sizeof(float));
    
    int index = 0;
    for (int i = -size; i <= size; i += step) {
        // Horizontal lines (along X, fixed Z)
        gridVertices[index++] = (float)i;
        gridVertices[index++] = 0.0f;
        gridVertices[index++] = (float)-size;
        gridVertices[index++] = 0.3f; // Gray R
        gridVertices[index++] = 0.3f; // Gray G
        gridVertices[index++] = 0.3f; // Gray B

        gridVertices[index++] = (float)i;
        gridVertices[index++] = 0.0f;
        gridVertices[index++] = (float)size;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;

        // Vertical lines (along Z, fixed X)
        gridVertices[index++] = (float)-size;
        gridVertices[index++] = 0.0f;
        gridVertices[index++] = (float)i;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;

        gridVertices[index++] = (float)size;
        gridVertices[index++] = 0.0f;
        gridVertices[index++] = (float)i;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;
        gridVertices[index++] = 0.3f;
    }
    gridLineCount = numVertices;

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, dataSize * sizeof(float), gridVertices, GL_STATIC_DRAW);
    
    // Position
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    
    // Color
    glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_COLOR);

    free(gridVertices);

    // 2. Axis (XYZ -> RGB)
    float axisData[] = {
        // X POS             COLOR
        0.0f,  0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
        20.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
        
        // Y POS             COLOR
        0.0f,  0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
        0.0f,  20.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        
        // Z POS             COLOR
        0.0f,  0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
        0.0f,  0.0f, 20.0f,  0.0f, 0.0f, 1.0f
    };
    
    glGenVertexArrays(1, &axisVAO);
    glGenBuffers(1, &axisVBO);
    glBindVertexArray(axisVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisData), axisData, GL_STATIC_DRAW);
    
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    
    glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(ATTRIB_COLOR);

    glBindVertexArray(0);
}

void renderEditorPrimitives(mat4 view, mat4 proj) {
    if (!lineShaderProgram) return;
    
    glUseProgram(lineShaderProgram->id);
    
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "projection"), 1, GL_FALSE, proj);
    
    mat4 m = mat4::identity();
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "model"), 1, GL_FALSE, m);


    // 1. Draw Grid
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridLineCount);

    // 2. Draw Axis
    // Clear depth so axis is always visible on top
    glClear(GL_DEPTH_BUFFER_BIT); 
    glBindVertexArray(axisVAO);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 6);
    glLineWidth(1.0f);

    glBindVertexArray(0);
    glUseProgram(0);
}

#endif
