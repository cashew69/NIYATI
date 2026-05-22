#ifndef VCLOUD_TEXTURE_H
#define VCLOUD_TEXTURE_H

#include <GL/glew.h>

/**
 * Loads a BC6H compressed 3D texture from an NVDF file.
 * The NVDF format for compressed textures is expected to have:
 * - Magic: "NVDF" (4 bytes)
 * - Version: 1 (uint32)
 * - Width: (uint32)
 * - Height: (uint32)
 * - Depth: (uint32)
 * - Format: 1 (uint32, where 1 indicates BC6H)
 * - Data: Raw BC6H blocks (16 bytes per 4x4 block)
 *
 * @param path Path to the .nvdf file.
 * @return OpenGL texture handle, or 0 on failure.
 */
GLuint vcVCloud_LoadBC6HNVDF(const char* path);

#endif // VCLOUD_TEXTURE_H
