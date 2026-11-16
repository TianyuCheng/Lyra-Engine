#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTBUFFER_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTBUFFER_H

#include "SoftCommon.h"

namespace lyra::softgpu
{
    struct SoftBuffer
    {
        GPUSize64           size  = 0;
        GPUBufferUsageFlags usage = 0;
        Vector<uint8_t>     data;
        GPUMapState         map_state      = GPUMapState::UNMAPPED;
        GPUMapMode          map_mode       = GPUMapMode::READ;
        GPUSize64           mapped_offset  = 0;
        GPUSize64           mapped_size    = 0;
        uint64_t            device_address = 0;
        CString             label          = "";

        SoftBuffer() = default;
        explicit SoftBuffer(const GPUBufferDescriptor& desc);

        auto get_mapped_range(GPUSize64 offset = 0, GPUSize64 len = 0) -> MappedBufferRange;
        auto map(GPUMapMode mode, GPUSize64 offset, GPUSize64 len) -> void;
        auto unmap() -> void;
        auto destroy() -> void;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTBUFFER_H
