#ifndef NVDF_COMPRESSOR_H
#define NVDF_COMPRESSOR_H

#include <stdint.h>
#include <vector>

/**
 * Basic BC6H compressor (unsigned).
 * Encodes R16F (or R8 converted to 16F) voxel data into BC6H blocks.
 * Note: This is a simplified "fast" compressor for offline/tooling use.
 */
namespace NVDFCompressor {
    // Encodes a 4x4x1 region (16 voxels) from an R8 buffer into a 16-byte BC6H block.
    // The input 'voxels' should be a 4x4 patch from a larger 2D slice.
    void EncodeBC6HBlock(uint8_t* block, const uint8_t* voxels4x4);

    // Compresses a full 3D R8 volume into BC6H.
    // Output size is ((w+3)/4) * ((h+3)/4) * d * 16 bytes.
    std::vector<uint8_t> CompressVolume(uint32_t w, uint32_t h, uint32_t d, const uint8_t* r8Data);
}

#endif // NVDF_COMPRESSOR_H
