#include "MetalUtils.h"

using namespace lyra;

// MetalBuffer implementations
MetalBuffer::MetalBuffer() {}

MetalBuffer::MetalBuffer(const GPUBufferDescriptor& desc)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        auto [options, storage_mode] = mtlenum(desc.usage);
        if (desc.size == 0) {
            get_logger()->error("Failed to create Metal buffer of size 0!");
            throw GPUValidationError("Failed to create Metal buffer of size 0!");
        }

        buffer = [rhi->device newBufferWithLength:desc.size options:options];
        if (!buffer) {
            get_logger()->error("Failed to create Metal buffer of size {}", desc.size);
            throw GPUOutOfMemoryError("Failed to create Metal buffer!");
        }

        this->storage_mode = options;

        // for CPU-visible buffers, get the contents pointer
        if (storage_mode == MTLStorageModeShared || storage_mode == MTLStorageModeManaged) {
            if (desc.mapped_at_creation) {
                mapped_data = static_cast<uint8_t*>([buffer contents]);
                mapped_size = desc.size;
            }
        }

        // set debug label if provided
        if (desc.label && buffer) {
            rhi->set_debug_label(buffer, desc.label);
        }
    }
}

void MetalBuffer::map(GPUSize64 offset, GPUSize64 size)
{
    if (buffer) {
        mapped_data = static_cast<uint8_t*>([buffer contents]) + offset;
        mapped_size = size == 0 ? [buffer length] - offset : size;
    }
}

void MetalBuffer::unmap()
{
    // Metal buffers remain mapped
    mapped_size = 0;
}

void MetalBuffer::destroy()
{
    buffer      = nil;
    mapped_data = nullptr;
    mapped_size = 0;
}
