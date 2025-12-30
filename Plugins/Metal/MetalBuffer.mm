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

bool api::create_buffer(GPUBufferHandle& handle, const GPUBufferDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalBuffer(desc);
    auto ind = rhi->buffers.add(obj);
    handle   = GPUBufferHandle(ind);
    return obj.valid();
}

void api::delete_buffer(GPUBufferHandle handle)
{
    get_rhi()->buffers.remove(handle.value);
}

void api::map_buffer(GPUBufferHandle buffer, GPUMapMode, GPUSize64 offset, GPUSize64 size)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    buf.map(offset, size);
}

void api::unmap_buffer(GPUBufferHandle buffer)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    buf.unmap();
}

void api::get_mapped_state(GPUBufferHandle buffer, GPUMapState& state)
{
    auto& buf = fetch_resource(get_rhi()->buffers, buffer);
    state     = buf.mapped() ? GPUMapState::MAPPED : GPUMapState::UNMAPPED;
}

void api::get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range)
{
    auto& buf  = fetch_resource(get_rhi()->buffers, buffer);
    range.data = buf.mapped_data;
    range.size = buf.mapped_size;
}
