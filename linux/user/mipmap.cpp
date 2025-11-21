// minimap_debug.cpp - FIXED & WORKING VERSION
#define MINIMAP_SIZE 200
#define MINIMAP_MARGIN 10

struct MinimapConfig {
    int size = MINIMAP_SIZE;
    int x = MINIMAP_MARGIN;
    int y = MINIMAP_MARGIN;
    float worldSize = 700.0f;   // How many world units across the minimap shows
    float height = 500.0f;      // Camera height above ground
    bool enabled = true;
};

MinimapConfig minimapConfig;

void initMinimap()
{
    printf("=== MINIMAP INIT ===\n");
    printf("Field info:\n");
    printf(" PLANE_WIDTH: %d\n", PLANE_WIDTH);
    printf(" PLANE_DEPTH: %d\n", PLANE_DEPTH);
    printf(" CHUNKSCALE: %d\n", CHUNKSCALE);
    printf(" Total field size: %d x %d units\n", PLANE_WIDTH * CHUNKSCALE, PLANE_DEPTH * CHUNKSCALE);

    if (grass_positions && INSTANCES > 0) {
        printf("\nGrass positions:\n");
        printf(" First: (%.1f, %.1f, %.1f)\n", grass_positions[0][0], grass_positions[0][1], grass_positions[0][2]);
        printf(" Last:  (%.1f, %.1f, %.1f)\n", grass_positions[INSTANCES-1][0], grass_positions[INSTANCES-1][1], grass_positions[INSTANCES-1][2]);
    }

    printf(" Bounding box sample:\n");
    printf("  bounds[0]: ox=(%.1f,%.1f,%.1f) ez=(%.1f,%.1f,%.1f)\n",
           bounds[0].ox[0], bounds[0].ox[1], bounds[0].ox[2],
           bounds[0].ez[0], bounds[0].ez[1], bounds[0].ez[2]);

    int lastIdx = PLANE_WIDTH * PLANE_DEPTH - 1;
    printf("  bounds[%d]: ox=(%.1f,%.1f,%.1f) ez=(%.1f,%.1f,%.1f)\n",
           lastIdx,
           bounds[lastIdx].ox[0], bounds[lastIdx].ox[1], bounds[lastIdx].ox[2],
           bounds[lastIdx].ez[0], bounds[lastIdx].ez[1], bounds[lastIdx].ez[2]);
    printf("===================\n\n");
}

void renderMinimap(GLint HeightMap)
{

void drawMinimapBorder(int windowWidth, int windowHeight);
    if (!minimapConfig.enabled) return;

    static int frameCount = 0;
    frameCount++;
    bool shouldPrint = (frameCount % 60 == 1);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int windowWidth  = viewport[2];
    int windowHeight = viewport[3];

    int mx = minimapConfig.x;
    int my = windowHeight - minimapConfig.y - minimapConfig.size;

    // Set minimap viewport
    glViewport(mx, my, minimapConfig.size, minimapConfig.size);

    // CRITICAL: Clear color + depth in minimap area only
    glEnable(GL_SCISSOR_TEST);
    glScissor(mx, my, minimapConfig.size, minimapConfig.size);
    glClearColor(0.05f, 0.1f, 0.2f, 1.0f);                    // Dark blue = obvious minimap background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    glDisable(GL_DEPTH_TEST);  // 2D-like top-down view

    // Compute actual world bounds from chunk bounding boxes
    float minX = +INFINITY, maxX = -INFINITY;
    float minZ = +INFINITY, maxZ = -INFINITY;
    int totalChunks = PLANE_WIDTH * PLANE_DEPTH;
    for (int i = 0; i < totalChunks; ++i) {
        minX = fminf(minX, bounds[i].ox[0]);
        maxX = fmaxf(maxX, bounds[i].ez[0]);
        minZ = fminf(minZ, bounds[i].ox[2]);
        maxZ = fmaxf(maxZ, bounds[i].ez[2]);
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float worldW  = maxX - minX;
    float worldH  = maxZ - minZ;
    float aspect  = worldH / worldW;

    if (shouldPrint) {
        printf("\n=== MINIMAP FRAME %d ===\n", frameCount);
        printf("World bounds: X %.1f..%.1f  Z %.1f..%.1f  (%.0fx%.0f)\n", minX, maxX, minZ, maxZ, worldW, worldH);
        printf("Center: (%.1f, %.1f)  Aspect: %.3f\n", centerX, centerZ, aspect);
    }

    // Top-down camera
    vec3 eye    = vec3(centerX, minimapConfig.height, centerZ);
    vec3 target = vec3(centerX, 0.0f, centerZ);
    vec3 up     = vec3(0.0f, 1.0f, 0.0f);                     // CORRECT up vector!
    mat4 view   = vmath::lookat(eye, target, up);

    // Orthographic projection with correct aspect ratio
    float half = minimapConfig.worldSize * 0.5f;
    mat4 proj  = vmath::ortho(
        -half, half,
        -half * aspect, half * aspect,
        0.1f, 3000.0f
    );

    // Render Terrain
    if (tessellationShaderProgram && tessellationShaderProgram->id) {
        glUseProgram(tessellationShaderProgram->id);
        glUniformMatrix4fv(glGetUniformLocation(tessellationShaderProgram->id, "uProjection"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(tessellationShaderProgram->id, "uView"),       1, GL_FALSE, view);

        vec3 lightPos = vec3(centerX, 300.0f, centerZ);
        glUniform3fv(glGetUniformLocation(tessellationShaderProgram->id, "uLightPos"),  1, lightPos);
        glUniform3fv(glGetUniformLocation(tessellationShaderProgram->id, "uLightColor"), 1, vec3(2.0f));
        glUniform3fv(glGetUniformLocation(tessellationShaderProgram->id, "uViewPos"),   1, eye);

        renderTerrain(HeightMap);
        if (shouldPrint) printf("Terrain rendered\n");
    }

    // Render Grass (ALL instances!)
    if (mainShaderProgram && mainShaderProgram->id && INSTANCES > 0) {
        glUseProgram(mainShaderProgram->id);
        glUniformMatrix4fv(glGetUniformLocation(mainShaderProgram->id, "uProjection"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(mainShaderProgram->id, "uView"),       1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(mainShaderProgram->id, "uModel"),      1, GL_FALSE, mat4::identity());

        vec3 lightPos = vec3(centerX, 300.0f, centerZ);
        glUniform3fv(glGetUniformLocation(mainShaderProgram->id, "uLightPos"),  1, lightPos);
        glUniform3fv(glGetUniformLocation(mainShaderProgram->id, "uLightColor"), 1, vec3(2.0f));
        glUniform3fv(glGetUniformLocation(mainShaderProgram->id, "uViewPos"),   1, eye);

        setMaterialUniforms(mainShaderProgram, &sceneMeshes[0].material);

        glBindVertexArray(sceneMeshes[0].vao);
        glDrawElementsInstanced(GL_TRIANGLES, sceneMeshes[0].indexCount,
                                GL_UNSIGNED_INT, nullptr, INSTANCES);  // ALL GRASS
        glBindVertexArray(0);

        if (shouldPrint) printf("Grass rendered (%d instances)\n", INSTANCES);
    }

    // Render Chunk Grid (bounding boxes)
    if (lineShaderProgram && lineShaderProgram->id) {
        glUseProgram(lineShaderProgram->id);
        glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "model"),      1, GL_FALSE, mat4::identity());
        glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "view"),       1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "projection"), 1, GL_FALSE, proj);
        glUniform3f(glGetUniformLocation(lineShaderProgram->id, "lineColor"), 1.0f, 0.0f, 1.0f); // MAGENTA

        glBindVertexArray(boundsVAO);
        glDrawElements(GL_LINES, totalChunks * 8, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (shouldPrint) printf("Grid lines rendered (%d chunks)\n", totalChunks);
    }

    if (shouldPrint) printf("Minimap render complete\n\n");

    // Draw border (screen-space)
    drawMinimapBorder(windowWidth, windowHeight);

    // Restore main viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glEnable(GL_DEPTH_TEST);
}

void drawMinimapBorder(int windowWidth, int windowHeight)
{
    glViewport(0, 0, windowWidth, windowHeight);
    glDisable(GL_DEPTH_TEST);

    mat4 ortho = vmath::ortho(0.0f, (float)windowWidth, 0.0f, (float)windowHeight, -1.0f, 1.0f);

    if (!lineShaderProgram || !lineShaderProgram->id) return;

    glUseProgram(lineShaderProgram->id);
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "model"),      1, GL_FALSE, mat4::identity());
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "view"),       1, GL_FALSE, mat4::identity());
    glUniformMatrix4fv(glGetUniformLocation(lineShaderProgram->id, "projection"), 1, GL_FALSE, ortho);
    glUniform3f(glGetUniformLocation(lineShaderProgram->id, "lineColor"), 1.0f, 0.0f, 0.0f); // RED

    float x = (float)minimapConfig.x;
    float y = (float)(windowHeight - minimapConfig.y - minimapConfig.size);
    float s = (float)minimapConfig.size;

    vec3 verts[5] = {
        {x, y, 0}, {x+s, y, 0}, {x+s, y+s, 0}, {x, y+s, 0}, {x, y, 0}
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glLineWidth(3.0f);
    glDrawArrays(GL_LINE_STRIP, 0, 5);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glEnable(GL_DEPTH_TEST);
}

// Controls
void toggleMinimap() {
    minimapConfig.enabled = !minimapConfig.enabled;
    printf("Minimap %s\n", minimapConfig.enabled ? "ENABLED" : "DISABLED");
}

void adjustMinimapSize(int delta) {
    minimapConfig.size = std::clamp(minimapConfig.size + delta, 100, 600);
    printf("Minimap size: %d px\n", minimapConfig.size);
}

void adjustMinimapZoom(float delta) {
    minimapConfig.worldSize = std::clamp(minimapConfig.worldSize + delta, 100.0f, 3000.0f);
    printf("Minimap zoom: %.0f world units\n", minimapConfig.worldSize);
}
