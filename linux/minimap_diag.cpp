// minimap_diagnostic.cpp - Check what uniforms your shaders actually have

#define MINIMAP_SIZE 200
#define MINIMAP_MARGIN 10

struct MinimapConfig {
    int size;
    int x;
    int y;
    bool enabled;
};

MinimapConfig minimapConfig = {
    MINIMAP_SIZE,
    MINIMAP_MARGIN,
    MINIMAP_MARGIN,
    true
};

void initMinimap()
{
    printf("Minimap initialized\n");
}

void checkShaderUniforms(GLuint programId, const char* shaderName)
{
    printf("\n=== Checking uniforms for %s (ID: %d) ===\n", shaderName, programId);
    
    GLint numUniforms = 0;
    glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &numUniforms);
    printf("Total active uniforms: %d\n", numUniforms);
    
    for (GLint i = 0; i < numUniforms; i++) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        
        glGetActiveUniform(programId, i, sizeof(name), &length, &size, &type, name);
        GLint location = glGetUniformLocation(programId, name);
        
        printf("  [%d] %s (location: %d, size: %d, type: 0x%04X)\n", 
               i, name, location, size, type);
    }
    printf("=== End of uniforms ===\n\n");
}

void renderMinimapDiagnostic(GLint HeightMap)
{
    if (!minimapConfig.enabled) return;
    
    printf("\n========================================\n");
    printf("MINIMAP DIAGNOSTIC\n");
    printf("========================================\n");
    
    // Check all shader programs
    if (mainShaderProgram && mainShaderProgram->id) {
        checkShaderUniforms(mainShaderProgram->id, "mainShaderProgram");
    } else {
        printf("mainShaderProgram not available\n");
    }
    
    if (lineShaderProgram && lineShaderProgram->id) {
        checkShaderUniforms(lineShaderProgram->id, "lineShaderProgram");
    } else {
        printf("lineShaderProgram not available\n");
    }
    
    if (tessellationShaderProgram && tessellationShaderProgram->id) {
        checkShaderUniforms(tessellationShaderProgram->id, "tessellationShaderProgram");
    } else {
        printf("tessellationShaderProgram not available\n");
    }
    
    // Get viewport info
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    printf("\nCurrent viewport: (%d, %d, %d, %d)\n", 
           viewport[0], viewport[1], viewport[2], viewport[3]);
    
    // Check grass rendering setup
    printf("\nGrass rendering info:\n");
    printf("  INSTANCES: %d\n", INSTANCES);
    printf("  sceneMeshes[0].vao: %u\n", sceneMeshes[0].vao);
    printf("  sceneMeshes[0].indexCount: %zu\n", sceneMeshes[0].indexCount);
    
    // Check bounding boxes setup
    printf("\nBounding boxes info:\n");
    printf("  boundsVAO: %u\n", boundsVAO);
    printf("  PLANE_WIDTH: %d\n", PLANE_WIDTH);
    printf("  PLANE_DEPTH: %d\n", PLANE_DEPTH);
    printf("  Total chunks: %d\n", PLANE_WIDTH * PLANE_DEPTH);
    
    printf("\n========================================\n");
    printf("END DIAGNOSTIC\n");
    printf("========================================\n\n");
    
    // Only run diagnostic once
    minimapConfig.enabled = false;
    printf("Minimap disabled after diagnostic. Press M to show diagnostic again.\n");
}

void toggleMinimap()
{
    minimapConfig.enabled = !minimapConfig.enabled;
    printf("Minimap diagnostic %s\n", minimapConfig.enabled ? "ENABLED (will run next frame)" : "DISABLED");
}

void adjustMinimapSize(int delta) {}
void adjustMinimapZoom(float delta) {}
