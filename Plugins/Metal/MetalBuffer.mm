#include "MetalUtils.h"

using namespace lyra;

// MetalBuffer implementations
MetalBuffer::MetalBuffer() {}

MetalBuffer::MetalBuffer(const GPUBufferDescriptor& desc)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        auto [options, storage_mode] = mtlenum(desc.usage);

        buffer             = [rhi->device newBufferWithLength:desc.size options:options];
        this->storage_mode = options;

        if (!buffer) {
            get_logger()->error("Failed to create Metal buffer of size {}", desc.size);
            return;
        }

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
