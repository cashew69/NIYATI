#include "vcloud_texture.h"
#include "engine/core/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
//  vcVCloud_LoadNVDF
//
//  Loads a .nvdf file into a GL_TEXTURE_3D.
//  Supports:
//    format 0 — R8 uint8   (written by the Blender SDF addon)
//    format 1 — BC6H       (GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
//
//  CHANGES vs original vcloud_texture.cpp:
//    • Renamed from vcVCloud_LoadBC6HNVDF → vcVCloud_LoadNVDF
//      (keep the old name as an alias if other code uses it)
//    • Added format=0 (R8) branch — the original rejected everything != BC6H
//    • Added glPixelStorei(GL_UNPACK_ALIGNMENT, 1) for the R8 upload
//      (without this, rows are 4-byte padded → garbled slices for odd widths)
// ─────────────────────────────────────────────────────────────────────────────

GLuint vcVCloud_LoadNVDF(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        LOG_E("vcVCloud_LoadNVDF: cannot open %s", path);
        return 0;
    }

    // ── Header ───────────────────────────────────────────────────────────────
    char     magic[4];
    uint32_t hdr[5];    // [version, w, h, d, format]

    if (fread(magic, 1, 4, f) != 4 || fread(hdr, sizeof(uint32_t), 5, f) != 5) {
        LOG_E("vcVCloud_LoadNVDF: bad header in %s", path);
        fclose(f);
        return 0;
    }

    if (magic[0] != 'N' || magic[1] != 'V' || magic[2] != 'D' || magic[3] != 'F') {
        LOG_E("vcVCloud_LoadNVDF: not an NVDF file: %s", path);
        fclose(f);
        return 0;
    }

    // uint32_t version = hdr[0];   // currently always 1
    uint32_t w   = hdr[1];
    uint32_t h   = hdr[2];
    uint32_t d   = hdr[3];
    uint32_t fmt = hdr[4];

    // ── Texture object ────────────────────────────────────────────────────────
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);

    // ── Format dispatch ───────────────────────────────────────────────────────
    if (fmt == 0)
    {
        // ── R8 uint8  (Blender SDF addon output) ─────────────────────────────
        size_t dataSize = (size_t)w * h * d;
        unsigned char* data = (unsigned char*)malloc(dataSize);
        if (!data) {
            LOG_E("vcVCloud_LoadNVDF: OOM allocating %zu bytes for %s",
                  dataSize, path);
            glDeleteTextures(1, &tex);
            fclose(f);
            return 0;
        }

        if (fread(data, 1, dataSize, f) != dataSize) {
            LOG_E("vcVCloud_LoadNVDF: short read R8 in %s (expected %zu bytes)",
                  path, dataSize);
            free(data);
            glDeleteTextures(1, &tex);
            fclose(f);
            return 0;
        }

        // CRITICAL: set alignment to 1 before upload.
        // GL default is 4-byte row alignment.  With 1-byte-per-voxel data this
        // pads each X-row to the next multiple of 4, shifting every row and
        // making the shape look completely garbled.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8, w, h, d);
        glTexSubImage3D(GL_TEXTURE_3D,
                        0,              // mip
                        0, 0, 0,        // x,y,z offset
                        w, h, d,        // dimensions
                        GL_RED, GL_UNSIGNED_BYTE, data);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default

        free(data);
        LOG_I("vcVCloud_LoadNVDF: R8 %dx%dx%d loaded from %s (id=%u)",
              w, h, d, path, tex);
    }
    else if (fmt == 1)
    {
        // ── BC6H compressed  (original path — unchanged) ─────────────────────
        uint32_t blocksW = (w + 3) / 4;
        uint32_t blocksH = (h + 3) / 4;
        size_t   compSz  = (size_t)blocksW * blocksH * d * 16;

        unsigned char* data = (unsigned char*)malloc(compSz);
        if (!data) {
            LOG_E("vcVCloud_LoadNVDF: OOM allocating %zu bytes BC6H for %s",
                  compSz, path);
            glDeleteTextures(1, &tex);
            fclose(f);
            return 0;
        }

        if (fread(data, 1, compSz, f) != compSz) {
            LOG_E("vcVCloud_LoadNVDF: short read BC6H in %s (expected %zu bytes)",
                  path, compSz);
            free(data);
            glDeleteTextures(1, &tex);
            fclose(f);
            return 0;
        }

        glCompressedTexImage3D(GL_TEXTURE_3D, 0,
                               GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
                               w, h, d, 0, (GLsizei)compSz, data);
        free(data);
        LOG_I("vcVCloud_LoadNVDF: BC6H %dx%dx%d loaded from %s (id=%u)",
              w, h, d, path, tex);
    }
    else
    {
        LOG_E("vcVCloud_LoadNVDF: unsupported format %u in %s  (0=R8, 1=BC6H)",
              fmt, path);
        glDeleteTextures(1, &tex);
        fclose(f);
        return 0;
    }

    // ── Sampling parameters (same as nvdfgen_EnsureNVDFTex) ──────────────────
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // S(X) and R(Z) are horizontal tile axes → REPEAT (cloud_compute samples uvxz.x/y there).
    // T(Y) is the cloud height axis, always fed clamped effH → CLAMP_TO_EDGE.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // X — horizontal tile
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Y — height (clamped)
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);        // Z — horizontal tile

    glBindTexture(GL_TEXTURE_3D, 0);
    fclose(f);
    return tex;
}

// ── Backwards-compatible alias ────────────────────────────────────────────────
// Any existing call sites that used the old name continue to work.
GLuint vcVCloud_LoadBC6HNVDF(const char* path)
{
    return vcVCloud_LoadNVDF(path);
}
