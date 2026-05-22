# Framebuffer Manager Usage Guide

## Overview

The framebuffer manager (`framebuffer.h/cpp`) provides a flexible, error-checked API for managing render-to-texture operations. It replaces manual framebuffer handling with a high-level interface that supports:

- **2D color textures** (for render targets)
- **Depth textures** (for shadow maps)
- **Depth renderbuffers** (efficient depth-only FBOs)
- **Cubemap textures** (for environment capture)
- **2D array textures** (for cascaded shadows, layered effects)
- **Multiple Render Targets (MRT)** (render to multiple colors simultaneously)
- **Automatic validation** (checks framebuffer completeness)
- **Automatic resizing** (when screen changes)
- **State tracking** (knows what's bound)

---

## Basic Usage Pattern

```cpp
// 1. Create a framebuffer
Framebuffer* fb = fb_Create(width, height, "MyEffect");

// 2. Attach textures
fb_AttachColorTexture(fb, 0, GL_RGBA16F);      // Color attachment 0
fb_AttachDepthTexture(fb, GL_DEPTH_COMPONENT24); // Depth texture

// 3. Render to it
fb_Bind(fb);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glUseProgram(myShader->id);
renderQuad();

// 4. Use the result in next pass
fb_Unbind();
GLuint resultTexture = fb_GetColorTexture(fb, 0);
glBindTexture(GL_TEXTURE_2D, resultTexture);

// 5. Cleanup
fb_Destroy(fb);
```

---

## Common Patterns

### Pattern 1: Simple Screen-Space Effect (SSAO, Bloom, etc)

```cpp
struct SSAOPass {
    Framebuffer* aoFB;
    Framebuffer* blurFB;
};

void ssao_Init(SSAOPass* pass, int width, int height) {
    // AO generation pass
    pass->aoFB = fb_Create(width / 2, height / 2, "SSAO_AO");
    fb_AttachColorTexture(pass->aoFB, 0, GL_R16F);
    
    // Blur pass
    pass->blurFB = fb_Create(width / 2, height / 2, "SSAO_Blur");
    fb_AttachColorTexture(pass->blurFB, 0, GL_R16F);
}

void ssao_Render(SSAOPass* pass, GLuint normalTexture, GLuint depthTexture) {
    // Pass 1: Generate AO
    fb_Bind(pass->aoFB);
    fb_ClearColor(pass->aoFB, 1.0f, 1.0f, 1.0f, 1.0f);
    glUseProgram(aoShader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    renderQuad();
    
    // Pass 2: Blur
    fb_Bind(pass->blurFB);
    glUseProgram(blurShader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_GetColorTexture(pass->aoFB, 0));
    renderQuad();
    
    fb_Unbind();
}

GLuint ssao_GetResult(SSAOPass* pass) {
    return fb_GetColorTexture(pass->blurFB, 0);
}

void ssao_Shutdown(SSAOPass* pass) {
    fb_Destroy(pass->aoFB);
    fb_Destroy(pass->blurFB);
}
```

---

### Pattern 2: Cascaded Shadow Maps

```cpp
struct ShadowPass {
    Framebuffer* shadowFB;
    int cascades;
};

void shadow_Init(ShadowPass* pass, int resolution, int numCascades) {
    pass->cascades = numCascades;
    pass->shadowFB = fb_Create(resolution, resolution, "ShadowMaps");
    
    // Use 2D array texture for all cascades
    fb_AttachDepthArray(pass->shadowFB, numCascades, GL_DEPTH_COMPONENT32F);
}

void shadow_Render(ShadowPass* pass, SceneNode* root, vec3 lightDir) {
    fb_Bind(pass->shadowFB);
    glUseProgram(depthShader->id);
    
    for (int cascade = 0; cascade < pass->cascades; ++cascade) {
        // Set layer for this cascade
        glUniform1i(glGetUniformLocation(depthShader->id, "layer"), cascade);
        
        // Set per-cascade view/projection
        mat4 cascadeView = computeCascadeView(lightDir, cascade);
        mat4 cascadeProj = computeCascadeProj(cascade);
        
        glUniformMatrix4fv(..., 1, GL_FALSE, cascadeView);
        glUniformMatrix4fv(..., 1, GL_FALSE, cascadeProj);
        
        // Clear depth for this cascade
        glClear(GL_DEPTH_BUFFER_BIT);
        
        // Render scene
        sg_DrawNode(root, cascadeView, cascadeProj, nullptr);
    }
    
    fb_Unbind();
}

GLuint shadow_GetDepth(ShadowPass* pass) {
    return fb_GetDepthTexture(pass->shadowFB);
}
```

---

### Pattern 3: Cubemap Capture (Refactored from existing code)

```cpp
struct CubemapPass {
    Framebuffer* captureFB;
};

void cubemap_Init(CubemapPass* pass, int resolution) {
    pass->captureFB = fb_Create(resolution, resolution, "CubemapCapture");
    fb_AttachCubemapColor(pass->captureFB, GL_RGBA16F);
    fb_AttachCubemapDepth(pass->captureFB, GL_DEPTH_COMPONENT24);
}

void cubemap_Render(CubemapPass* pass, SceneNode* root, vec3 center) {
    fb_Bind(pass->captureFB);
    glViewport(0, 0, fb_GetWidth(pass->captureFB), fb_GetHeight(pass->captureFB));
    
    mat4 projection = perspective(90.0f, 1.0f, 0.1f, 10000.0f);
    mat4 views[6];
    getCubemapCaptureMatrices(&projection, views);
    
    // Translate views to capture center
    mat4 translate = vmath::translate(-center);
    for (int i = 0; i < 6; ++i) {
        views[i] = views[i] * translate;
    }
    
    GLuint cubemap = fb_GetColorTexture(pass->captureFB, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    
    for (int face = 0; face < 6; ++face) {
        // Bind this face as render target
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemap, 0);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        sg_DrawNode(root, views[face], projection, nullptr);
    }
    
    fb_Unbind();
}
```

---

### Pattern 4: Multiple Render Targets (G-Buffer)

```cpp
struct GBuffer {
    Framebuffer* gbufferFB;
};

void gbuffer_Init(GBuffer* gb, int width, int height) {
    gb->gbufferFB = fb_Create(width, height, "GBuffer");
    
    // Position + metadata
    fb_AttachColorTexture(gb->gbufferFB, 0, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    
    // Normal + shininess
    fb_AttachColorTexture(gb->gbufferFB, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    
    // Albedo + AO
    fb_AttachColorTexture(gb->gbufferFB, 2, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    
    // Depth
    fb_AttachDepthRenderbuffer(gb->gbufferFB, GL_DEPTH_COMPONENT24);
    
    // Tell GPU we're writing to all 3 color attachments
    int drawBuffers[] = {0, 1, 2};
    fb_SetDrawBuffers(gb->gbufferFB, drawBuffers, 3);
}

void gbuffer_Render(GBuffer* gb, SceneNode* root, mat4 view, mat4 projection) {
    fb_Bind(gb->gbufferFB);
    fb_Clear(gb->gbufferFB, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glUseProgram(gbufferShader->id);
    sg_DrawNode(root, view, projection, nullptr);
    
    fb_Unbind();
}

// Later: deferred shading pass reads from all three
void deferred_Light(GBuffer* gb) {
    glUseProgram(lightingShader->id);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_GetColorTexture(gb->gbufferFB, 0)); // Position
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fb_GetColorTexture(gb->gbufferFB, 1)); // Normal
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, fb_GetColorTexture(gb->gbufferFB, 2)); // Albedo
    
    renderQuad(); // Render to screen
}
```

---

## API Reference

### Creation
```cpp
Framebuffer* fb_Create(int width, int height, const char* name);
void fb_Destroy(Framebuffer* fb);
```

### Attachments
```cpp
// Color
void fb_AttachColorTexture(Framebuffer* fb, int index, GLenum internalFormat, 
                           GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);

// Depth (texture or RBO)
void fb_AttachDepthTexture(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);
void fb_AttachDepthRenderbuffer(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Cubemap
void fb_AttachCubemapColor(Framebuffer* fb, GLenum internalFormat = GL_RGB16F);
void fb_AttachCubemapDepth(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Arrays (cascades, layers)
void fb_AttachColorArray(Framebuffer* fb, int index, int layers, GLenum internalFormat);
void fb_AttachDepthArray(Framebuffer* fb, int layers, GLenum internalFormat);

// Stencil
void fb_AttachStencilRenderbuffer(Framebuffer* fb);
```

### Binding
```cpp
void fb_Bind(Framebuffer* fb);          // Bind for read & write
void fb_BindRead(Framebuffer* fb);      // Bind for reading (blits, etc)
void fb_BindDraw(Framebuffer* fb);      // Bind for writing only
void fb_Unbind();                       // Bind to screen
void fb_SetDrawBuffers(Framebuffer* fb, const int* indices, int count); // MRT
```

### Queries
```cpp
GLuint fb_GetColorTexture(Framebuffer* fb, int index);
GLuint fb_GetDepthTexture(Framebuffer* fb);
GLuint fb_GetDepthRenderbuffer(Framebuffer* fb);
GLuint fb_GetStencilRenderbuffer(Framebuffer* fb);

int fb_GetWidth(Framebuffer* fb);
int fb_GetHeight(Framebuffer* fb);

bool fb_IsValid(Framebuffer* fb);
const char* fb_GetStatus(Framebuffer* fb);
```

### Modification
```cpp
void fb_Resize(Framebuffer* fb, int newWidth, int newHeight);
void fb_Clear(Framebuffer* fb, GLbitfield mask);
void fb_ClearColor(Framebuffer* fb, float r, float g, float b, float a);
void fb_ClearColorIndexed(Framebuffer* fb, int index, float r, float g, float b, float a);
```

### Copy Operations
```cpp
void fb_BlitColor(Framebuffer* src, int srcIndex, Framebuffer* dst, int dstIndex);
void fb_BlitDepth(Framebuffer* src, Framebuffer* dst);
```

### Debug
```cpp
void fb_PrintInfo(Framebuffer* fb);
GLuint fb_GetCurrentBound();
```

---

## Best Practices

1. **Always validate**: The manager auto-validates. Check logs if attachment fails.

2. **Reuse framebuffers**: Don't create/destroy every frame. Create once, reuse.

3. **Use the right attachment type**:
   - **Texture** if you need to sample it later
   - **RBO** if it's write-only (faster)

4. **Resolution management**: Call `fb_Resize()` when viewport changes (not needed if fixed size).

5. **Multiple colors**: Use `fb_SetDrawBuffers()` to control which attachments are written.

6. **Cubemaps**: Bind per-face inside a loop (see example above).

7. **Arrays**: Use for cascades or layered rendering. GPU renders to layer via geometry shader or instancing.

8. **Debug**: Call `fb_PrintInfo()` to see what's attached.

---

## Migrating Existing Code

### Before (pbr.cpp pattern):
```cpp
unsigned int captureFBO, captureRBO;
createCaptureFBO(&captureFBO, &captureRBO, 512);
glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ...);
glClear(...);
renderCube();
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glDeleteFramebuffers(1, &captureFBO);
```

### After (with manager):
```cpp
Framebuffer* captureFB = fb_Create(512, 512, "IBL_Capture");
fb_AttachColorTexture(captureFB, 0, GL_RGB16F);
fb_AttachDepthRenderbuffer(captureFB);

fb_Bind(captureFB);
fb_Clear(captureFB, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
renderCube();
fb_Unbind();

fb_Destroy(captureFB);
```

---

## Next Steps

1. ✅ Framebuffer manager built and validated
2. ⏭️ Refactor existing effects (pbr.cpp, skybox.cpp, dynamic_cubemap.cpp)
3. ⏭️ Implement SSAO effect
4. ⏭️ Implement shadow mapping
5. ⏭️ Add post-processing pipeline

Start with SSAO to validate the system works end-to-end!
