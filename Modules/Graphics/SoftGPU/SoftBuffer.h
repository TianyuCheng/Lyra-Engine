#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTBUFFER_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTBUFFER_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftBuffer
    {
        GPUSize64           size  = 0;
        GPUBufferUsageFlags usage = 0;
        Vector<ubyte>       data;
        GPUMapState         map_state      = GPUMapState::UNMAPPED;
        GPUMapMode          map_mode       = GPUMapMode::READ;
        GPUSize64           mapped_offset  = 0;
        GPUSize64           mapped_size    = 0;
        ulong               device_address = 0;
        CString             label          = "";

        SoftBuffer() = default;
        explicit SoftBuffer(const GPUBufferDescriptor& desc);

        MappedBufferRange get_mapped_range(GPUSize64 offset = 0, GPUSize64 len = 0);
        void              map(GPUMapMode mode, GPUSize64 offset, GPUSize64 len);
        void              unmap();
        void              destroy();
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTBUFFER_H
