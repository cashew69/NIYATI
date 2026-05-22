#include "framebuffer.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern FILE* gpFile;

// Track current bound framebuffer
static GLuint g_CurrentBoundFB = 0;

// ============================================================================
// Utility: Status String
// ============================================================================

static const char* getFramebufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
        case GL_FRAMEBUFFER_UNDEFINED: return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
        case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
        default: return "UNKNOWN_STATUS";
    }
}

// Validate framebuffer status
static bool validateFramebuffer(Framebuffer* fb) {
    if (!fb) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);
    fb->status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (fb->status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(gpFile, "[FB Error] %s: %s\n", fb->name, getFramebufferStatusString(fb->status));
        fb->isValid = false;
        return false;
    }

    fb->isValid = true;
    return true;
}

// ============================================================================
// Creation & Destruction
// ============================================================================

Framebuffer* fb_Create(int width, int height, const char* name) {
    if (width <= 0 || height <= 0) {
        fprintf(gpFile, "[FB Error] Invalid framebuffer size: %dx%d\n", width, height);
        return nullptr;
    }

    Framebuffer* fb = new Framebuffer();
    if (!fb) return nullptr;

    fb->fboID = 0;
    fb->width = width;
    fb->height = height;
    fb->isValid = false;
    fb->status = GL_FRAMEBUFFER_UNDEFINED;

    if (name) {
        strncpy(fb->name, name, 63);
        fb->name[63] = '\0';
    } else {
        strcpy(fb->name, "Unnamed");
    }

    // Create FBO
    glGenFramebuffers(1, &fb->fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Initialize attachment data
    fb->depthAttachment.resourceID = 0;
    fb->depthAttachment.isRenderbuffer = false;
    fb->stencilAttachment.resourceID = 0;
    fb->stencilAttachment.isRenderbuffer = false;

    fprintf(gpFile, "[FB] Created framebuffer '%s': %dx%d (ID: %u)\n", fb->name, width, height, fb->fboID);
    return fb;
}

void fb_Destroy(Framebuffer* fb) {
    if (!fb) return;

    // Delete all color textures/RBOs
    for (const auto& attachment : fb->colorAttachments) {
        if (attachment.resourceID > 0) {
            if (attachment.isRenderbuffer) {
                glDeleteRenderbuffers(1, &attachment.resourceID);
            } else {
                glDeleteTextures(1, &attachment.resourceID);
            }
        }
    }

    // Delete depth attachment
    if (fb->depthAttachment.resourceID > 0) {
        if (fb->depthAttachment.isRenderbuffer) {
            glDeleteRenderbuffers(1, &fb->depthAttachment.resourceID);
        } else {
            glDeleteTextures(1, &fb->depthAttachment.resourceID);
        }
    }

    // Delete stencil attachment
    if (fb->stencilAttachment.resourceID > 0) {
        glDeleteRenderbuffers(1, &fb->stencilAttachment.resourceID);
    }

    // Delete FBO
    if (fb->fboID > 0) {
        glDeleteFramebuffers(1, &fb->fboID);
    }

    fprintf(gpFile, "[FB] Destroyed framebuffer '%s'\n", fb->name);
    delete fb;
}

// ============================================================================
// Attachment: 2D Color Textures
// ============================================================================

void fb_AttachColorTexture(Framebuffer* fb, int colorIndex, GLenum internalFormat, GLenum format, GLenum type) {
    if (!fb || colorIndex < 0 || colorIndex > 15) {
        fprintf(gpFile, "[FB Error] Invalid color index: %d\n", colorIndex);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Create texture
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, fb->width, fb->height, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach to FBO
    GLenum attachment = GL_COLOR_ATTACHMENT0 + colorIndex;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);

    // Store attachment info
    while ((int)fb->colorAttachments.size() <= colorIndex) {
        FBAttachment empty = {};
        fb->colorAttachments.push_back(empty);
    }

    fb->colorAttachments[colorIndex].target = GL_TEXTURE_2D;
    fb->colorAttachments[colorIndex].resourceID = texture;
    fb->colorAttachments[colorIndex].internalFormat = internalFormat;
    fb->colorAttachments[colorIndex].format = format;
    fb->colorAttachments[colorIndex].type = type;
    fb->colorAttachments[colorIndex].width = fb->width;
    fb->colorAttachments[colorIndex].height = fb->height;
    fb->colorAttachments[colorIndex].isRenderbuffer = false;
    fb->colorAttachments[colorIndex].layers = 0;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached color texture to %s[%d]: format=%d, size=%dx%d\n",
            fb->name, colorIndex, internalFormat, fb->width, fb->height);
}

// ============================================================================
// Attachment: Depth Textures
// ============================================================================

void fb_AttachDepthTexture(Framebuffer* fb, GLenum internalFormat) {
    if (!fb) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Create depth texture
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, fb->width, fb->height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);

    fb->depthAttachment.target = GL_TEXTURE_2D;
    fb->depthAttachment.resourceID = texture;
    fb->depthAttachment.internalFormat = internalFormat;
    fb->depthAttachment.format = GL_DEPTH_COMPONENT;
    fb->depthAttachment.type = GL_FLOAT;
    fb->depthAttachment.width = fb->width;
    fb->depthAttachment.height = fb->height;
    fb->depthAttachment.isRenderbuffer = false;
    fb->depthAttachment.layers = 0;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached depth texture to %s\n", fb->name);
}

// ============================================================================
// Attachment: Depth Renderbuffer (efficient for depth-only)
// ============================================================================

void fb_AttachDepthRenderbuffer(Framebuffer* fb, GLenum internalFormat) {
    if (!fb) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Create depth RBO
    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, fb->width, fb->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    fb->depthAttachment.target = GL_RENDERBUFFER;
    fb->depthAttachment.resourceID = rbo;
    fb->depthAttachment.internalFormat = internalFormat;
    fb->depthAttachment.isRenderbuffer = true;
    fb->depthAttachment.width = fb->width;
    fb->depthAttachment.height = fb->height;
    fb->depthAttachment.layers = 0;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached depth RBO to %s\n", fb->name);
}

// ============================================================================
// Attachment: Stencil Renderbuffer
// ============================================================================

void fb_AttachStencilRenderbuffer(Framebuffer* fb) {
    if (!fb) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, fb->width, fb->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    fb->stencilAttachment.resourceID = rbo;
    fb->stencilAttachment.isRenderbuffer = true;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached stencil RBO to %s\n", fb->name);
}

// ============================================================================
// Attachment: Cubemap
// ============================================================================

void fb_AttachCubemapColor(Framebuffer* fb, GLenum internalFormat) {
    if (!fb) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Create cubemap texture
    GLuint cubemap = 0;
    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, fb->width, fb->height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Store cubemap as color attachment 0
    while ((int)fb->colorAttachments.size() <= 0) {
        FBAttachment empty = {};
        fb->colorAttachments.push_back(empty);
    }

    fb->colorAttachments[0].target = GL_TEXTURE_CUBE_MAP;
    fb->colorAttachments[0].resourceID = cubemap;
    fb->colorAttachments[0].internalFormat = internalFormat;
    fb->colorAttachments[0].isRenderbuffer = false;
    fb->colorAttachments[0].width = fb->width;
    fb->colorAttachments[0].height = fb->height;
    fb->colorAttachments[0].layers = 6;

    fprintf(gpFile, "[FB] Attached cubemap color to %s (will be bound per-face)\n", fb->name);
}

void fb_AttachCubemapDepth(Framebuffer* fb, GLenum internalFormat) {
    if (!fb) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    GLuint rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, fb->width, fb->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    fb->depthAttachment.resourceID = rbo;
    fb->depthAttachment.isRenderbuffer = true;
    fb->depthAttachment.layers = 6;

    fprintf(gpFile, "[FB] Attached cubemap depth RBO to %s\n", fb->name);
}

// ============================================================================
// Attachment: 2D Array Textures (cascades, layered effects)
// ============================================================================

void fb_AttachColorArray(Framebuffer* fb, int colorIndex, int layers, GLenum internalFormat) {
    if (!fb || colorIndex < 0 || colorIndex > 15 || layers <= 0) {
        fprintf(gpFile, "[FB Error] Invalid parameters for array attachment\n");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, fb->width, fb->height, layers, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum attachment = GL_COLOR_ATTACHMENT0 + colorIndex;
    glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, 0);

    while ((int)fb->colorAttachments.size() <= colorIndex) {
        FBAttachment empty = {};
        fb->colorAttachments.push_back(empty);
    }

    fb->colorAttachments[colorIndex].target = GL_TEXTURE_2D_ARRAY;
    fb->colorAttachments[colorIndex].resourceID = texture;
    fb->colorAttachments[colorIndex].internalFormat = internalFormat;
    fb->colorAttachments[colorIndex].width = fb->width;
    fb->colorAttachments[colorIndex].height = fb->height;
    fb->colorAttachments[colorIndex].layers = layers;
    fb->colorAttachments[colorIndex].isRenderbuffer = false;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached color array to %s[%d]: %d layers\n", fb->name, colorIndex, layers);
}

void fb_AttachDepthArray(Framebuffer* fb, int layers, GLenum internalFormat) {
    if (!fb || layers <= 0) {
        fprintf(gpFile, "[FB Error] Invalid layers for depth array: %d\n", layers);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, fb->width, fb->height, layers, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0);

    fb->depthAttachment.target = GL_TEXTURE_2D_ARRAY;
    fb->depthAttachment.resourceID = texture;
    fb->depthAttachment.internalFormat = internalFormat;
    fb->depthAttachment.width = fb->width;
    fb->depthAttachment.height = fb->height;
    fb->depthAttachment.layers = layers;
    fb->depthAttachment.isRenderbuffer = false;

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Attached depth array to %s: %d layers\n", fb->name, layers);
}

// ============================================================================
// Binding & State
// ============================================================================

void fb_Bind(Framebuffer* fb) {
    if (!fb) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        g_CurrentBoundFB = 0;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);
    g_CurrentBoundFB = fb->fboID;
}

void fb_BindRead(Framebuffer* fb) {
    if (!fb) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb->fboID);
}

void fb_BindDraw(Framebuffer* fb) {
    if (!fb) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        g_CurrentBoundFB = 0;
        return;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->fboID);
    g_CurrentBoundFB = fb->fboID;
}

void fb_Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_CurrentBoundFB = 0;
}

void fb_SetDrawBuffers(Framebuffer* fb, const int* indices, int count) {
    if (!fb || !indices || count <= 0) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    GLenum* attachments = (GLenum*)malloc(sizeof(GLenum) * count);
    for (int i = 0; i < count; ++i) {
        attachments[i] = GL_COLOR_ATTACHMENT0 + indices[i];
    }

    glDrawBuffers(count, attachments);
    free(attachments);
}

// ============================================================================
// Queries & Getters
// ============================================================================

GLuint fb_GetColorTexture(Framebuffer* fb, int colorIndex) {
    if (!fb || colorIndex < 0 || colorIndex >= (int)fb->colorAttachments.size()) {
        return 0;
    }
    return fb->colorAttachments[colorIndex].resourceID;
}

GLuint fb_GetDepthTexture(Framebuffer* fb) {
    if (!fb || fb->depthAttachment.isRenderbuffer) {
        return 0;
    }
    return fb->depthAttachment.resourceID;
}

GLuint fb_GetDepthRenderbuffer(Framebuffer* fb) {
    if (!fb || !fb->depthAttachment.isRenderbuffer) {
        return 0;
    }
    return fb->depthAttachment.resourceID;
}

GLuint fb_GetStencilRenderbuffer(Framebuffer* fb) {
    if (!fb) return 0;
    return fb->stencilAttachment.resourceID;
}

int fb_GetWidth(Framebuffer* fb) {
    return fb ? fb->width : 0;
}

int fb_GetHeight(Framebuffer* fb) {
    return fb ? fb->height : 0;
}

bool fb_IsValid(Framebuffer* fb) {
    return fb ? fb->isValid : false;
}

const char* fb_GetStatus(Framebuffer* fb) {
    return fb ? getFramebufferStatusString(fb->status) : "NULL";
}

// ============================================================================
// Modification
// ============================================================================

void fb_Resize(Framebuffer* fb, int newWidth, int newHeight) {
    if (!fb || newWidth <= 0 || newHeight <= 0) {
        fprintf(gpFile, "[FB Error] Invalid resize dimensions\n");
        return;
    }

    // Destroy old attachments
    for (auto& attachment : fb->colorAttachments) {
        if (attachment.resourceID > 0) {
            if (attachment.isRenderbuffer) {
                glDeleteRenderbuffers(1, &attachment.resourceID);
            } else {
                glDeleteTextures(1, &attachment.resourceID);
            }
        }
    }

    if (fb->depthAttachment.resourceID > 0) {
        if (fb->depthAttachment.isRenderbuffer) {
            glDeleteRenderbuffers(1, &fb->depthAttachment.resourceID);
        } else {
            glDeleteTextures(1, &fb->depthAttachment.resourceID);
        }
    }

    if (fb->stencilAttachment.resourceID > 0) {
        glDeleteRenderbuffers(1, &fb->stencilAttachment.resourceID);
    }

    // Update dimensions
    fb->width = newWidth;
    fb->height = newHeight;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);

    // Recreate color attachments
    for (size_t i = 0; i < fb->colorAttachments.size(); ++i) {
        FBAttachment& att = fb->colorAttachments[i];
        if (att.resourceID == 0) continue;

        if (att.target == GL_TEXTURE_2D) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, att.internalFormat, newWidth, newHeight, 0,
                         att.format, att.type, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
            att.resourceID = texture;
            att.width = newWidth;
            att.height = newHeight;
        }
        else if (att.target == GL_TEXTURE_2D_ARRAY) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, att.internalFormat, newWidth, newHeight, att.layers, 0,
                         att.format ? att.format : GL_RGBA, att.type ? att.type : GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
            glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, 0);
            att.resourceID = texture;
            att.width = newWidth;
            att.height = newHeight;
        }
    }

    // Recreate depth attachment
    if (fb->depthAttachment.resourceID > 0 || fb->depthAttachment.internalFormat != 0) {
        if (fb->depthAttachment.isRenderbuffer) {
            GLuint rbo = 0;
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            GLenum depthFormat = fb->depthAttachment.internalFormat ? fb->depthAttachment.internalFormat : GL_DEPTH_COMPONENT24;
            glRenderbufferStorage(GL_RENDERBUFFER, depthFormat, newWidth, newHeight);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
            fb->depthAttachment.resourceID = rbo;
        } else if (fb->depthAttachment.target == GL_TEXTURE_2D) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, fb->depthAttachment.internalFormat, newWidth, newHeight, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
            fb->depthAttachment.resourceID = texture;
        } else if (fb->depthAttachment.target == GL_TEXTURE_2D_ARRAY) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, fb->depthAttachment.internalFormat, newWidth, newHeight,
                         fb->depthAttachment.layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0);
            fb->depthAttachment.resourceID = texture;
        }
        fb->depthAttachment.width = newWidth;
        fb->depthAttachment.height = newHeight;
    }

    validateFramebuffer(fb);
    fprintf(gpFile, "[FB] Resized %s to %dx%d\n", fb->name, newWidth, newHeight);
}

// ============================================================================
// Clear Operations
// ============================================================================

void fb_Clear(Framebuffer* fb, GLbitfield mask) {
    if (!fb) return;
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);
    glClear(mask);
}

void fb_ClearColor(Framebuffer* fb, float r, float g, float b, float a) {
    if (!fb) return;
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void fb_ClearColorIndexed(Framebuffer* fb, int colorIndex, float r, float g, float b, float a) {
    if (!fb) return;
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fboID);
    glClearBufferfv(GL_COLOR, colorIndex, (float[]){r, g, b, a});
}

// ============================================================================
// Copy Operations
// ============================================================================

void fb_BlitColor(Framebuffer* srcFB, int srcColorIndex, Framebuffer* dstFB, int dstColorIndex) {
    if (!srcFB || !dstFB) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB->fboID);
    glReadBuffer(GL_COLOR_ATTACHMENT0 + srcColorIndex);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFB->fboID);
    glDrawBuffer(GL_COLOR_ATTACHMENT0 + dstColorIndex);

    glBlitFramebuffer(0, 0, srcFB->width, srcFB->height, 0, 0, dstFB->width, dstFB->height,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void fb_BlitDepth(Framebuffer* srcFB, Framebuffer* dstFB) {
    if (!srcFB || !dstFB) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB->fboID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFB->fboID);

    glBlitFramebuffer(0, 0, srcFB->width, srcFB->height, 0, 0, dstFB->width, dstFB->height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ============================================================================
// Utility
// ============================================================================

void fb_PrintInfo(Framebuffer* fb) {
    if (!fb) {
        fprintf(gpFile, "[FB] NULL framebuffer\n");
        return;
    }

    fprintf(gpFile, "\n[FB] ===== Framebuffer Info: %s =====\n", fb->name);
    fprintf(gpFile, "     ID: %u | Size: %dx%d | Valid: %s\n",
            fb->fboID, fb->width, fb->height, fb->isValid ? "YES" : "NO");
    fprintf(gpFile, "     Status: %s\n", getFramebufferStatusString(fb->status));

    fprintf(gpFile, "     Color Attachments: %zu\n", fb->colorAttachments.size());
    for (size_t i = 0; i < fb->colorAttachments.size(); ++i) {
        const FBAttachment& att = fb->colorAttachments[i];
        if (att.resourceID == 0) continue;

        const char* targetStr = att.target == GL_TEXTURE_2D ? "2D" :
                               att.target == GL_TEXTURE_CUBE_MAP ? "CUBE" :
                               att.target == GL_TEXTURE_2D_ARRAY ? "ARRAY" : "?";
        fprintf(gpFile, "       [%zu] %s (ID: %u, Format: %d, Layers: %d)\n",
                i, targetStr, att.resourceID, att.internalFormat, att.layers);
    }

    if (fb->depthAttachment.resourceID > 0) {
        const char* type = fb->depthAttachment.isRenderbuffer ? "RBO" : "Texture";
        fprintf(gpFile, "     Depth: %s (ID: %u, Format: %d)\n",
                type, fb->depthAttachment.resourceID, fb->depthAttachment.internalFormat);
    }

    if (fb->stencilAttachment.resourceID > 0) {
        fprintf(gpFile, "     Stencil: RBO (ID: %u)\n", fb->stencilAttachment.resourceID);
    }

    fprintf(gpFile, "[FB] ==============================\n\n");
}

GLuint fb_GetCurrentBound() {
    return g_CurrentBoundFB;
}
