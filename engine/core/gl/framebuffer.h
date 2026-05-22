#pragma once
#include <GL/glew.h>
#include <vector>

// Framebuffer attachment descriptor
struct FBAttachment {
    GLenum target;              // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY, etc
    GLuint resourceID;          // Texture or renderbuffer ID
    GLenum internalFormat;      // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT24, etc
    GLenum format;              // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT, etc
    GLenum type;                // GL_UNSIGNED_BYTE, GL_FLOAT, etc
    int width, height;
    int layers;                 // For array textures (0 = not an array)
    bool isRenderbuffer;        // True if RBO, false if texture
};

// Framebuffer object wrapper
struct Framebuffer {
    GLuint fboID;
    char name[64];
    int width, height;

    std::vector<FBAttachment> colorAttachments;  // Color attachments [0..15]
    FBAttachment depthAttachment;                // Depth attachment
    FBAttachment stencilAttachment;              // Stencil attachment

    bool isValid;
    GLenum status;
};

// ============================================================================
// Creation API
// ============================================================================

// Create a new framebuffer
Framebuffer* fb_Create(int width, int height, const char* name);

// Destroy a framebuffer and all its attachments
void fb_Destroy(Framebuffer* fb);

// ============================================================================
// Attachment API
// ============================================================================

// Attach a 2D color texture
void fb_AttachColorTexture(Framebuffer* fb, int colorIndex, GLenum internalFormat, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);

// Attach a 2D depth texture
void fb_AttachDepthTexture(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Attach a depth renderbuffer (more efficient for depth-only)
void fb_AttachDepthRenderbuffer(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Attach cubemap faces (for cubemap capture)
void fb_AttachCubemapColor(Framebuffer* fb, GLenum internalFormat = GL_RGB16F);
void fb_AttachCubemapDepth(Framebuffer* fb, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Attach 2D array texture (for cascading shadows, layered effects)
void fb_AttachColorArray(Framebuffer* fb, int colorIndex, int layers, GLenum internalFormat = GL_RGBA16F);
void fb_AttachDepthArray(Framebuffer* fb, int layers, GLenum internalFormat = GL_DEPTH_COMPONENT24);

// Attach stencil renderbuffer
void fb_AttachStencilRenderbuffer(Framebuffer* fb);

// ============================================================================
// Binding & State API
// ============================================================================

// Bind framebuffer for reading and writing
void fb_Bind(Framebuffer* fb);

// Bind framebuffer for reading only
void fb_BindRead(Framebuffer* fb);

// Bind framebuffer for writing only
void fb_BindDraw(Framebuffer* fb);

// Unbind framebuffer (bind to screen)
void fb_Unbind();

// Set which color attachments will be written to (MRT)
void fb_SetDrawBuffers(Framebuffer* fb, const int* indices, int count);

// ============================================================================
// Queries & Getters
// ============================================================================

// Get texture/RBO IDs
GLuint fb_GetColorTexture(Framebuffer* fb, int colorIndex);
GLuint fb_GetDepthTexture(Framebuffer* fb);
GLuint fb_GetDepthRenderbuffer(Framebuffer* fb);
GLuint fb_GetStencilRenderbuffer(Framebuffer* fb);

// Get dimensions
int fb_GetWidth(Framebuffer* fb);
int fb_GetHeight(Framebuffer* fb);

// Check validity
bool fb_IsValid(Framebuffer* fb);
const char* fb_GetStatus(Framebuffer* fb);

// ============================================================================
// Modification API
// ============================================================================

// Resize framebuffer and all attachments
void fb_Resize(Framebuffer* fb, int newWidth, int newHeight);

// Clear framebuffer
void fb_Clear(Framebuffer* fb, GLbitfield mask);
void fb_ClearColor(Framebuffer* fb, float r, float g, float b, float a);
void fb_ClearColorIndexed(Framebuffer* fb, int colorIndex, float r, float g, float b, float a);

// ============================================================================
// Copy operations
// ============================================================================

// Copy from one framebuffer to another
void fb_BlitColor(Framebuffer* srcFB, int srcColorIndex, Framebuffer* dstFB, int dstColorIndex);
void fb_BlitDepth(Framebuffer* srcFB, Framebuffer* dstFB);

// ============================================================================
// Utility
// ============================================================================

// Log framebuffer info
void fb_PrintInfo(Framebuffer* fb);

// Get the currently bound framebuffer ID (for debugging)
GLuint fb_GetCurrentBound();
