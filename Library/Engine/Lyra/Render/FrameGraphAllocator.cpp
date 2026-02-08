#include "FrameGraphAllocator.h"

using namespace lyra;

FGBufferObject FrameGraphAllocator::allocate(const GPUBufferDescriptor& descriptor)
{
    buffers.try_emplace(descriptor, FGBufferVector{});
    auto it = buffers.find(descriptor);

    // allocate from existing objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (!object.used) {
            object.used = true;
            return object.data;
        }
    }

    // allocate new object
    auto device = RHI::get_current_device();
    auto buffer = device.create_buffer(descriptor);
    objects.push_back({buffer.handle, true});
    return buffer.handle;
}

void FrameGraphAllocator::recycle(const GPUBufferDescriptor& descriptor, FGBufferObject buffer)
{
    buffers.try_emplace(descriptor, FGBufferVector{});
    auto it = buffers.find(descriptor);

    // find from existing allocated objects
    auto& objects = it->second;
    for (auto& object : objects)
        if (object.data == buffer)
            object.used = false;
}

FGTextureObject FrameGraphAllocator::allocate(const GPUTextureDescriptor& descriptor)
{
    textures.try_emplace(descriptor, FGTextureVector{});
    auto it = textures.find(descriptor);

    // allocate from existing objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (!object.used) {
            object.used = true;
            return object.data;
        }
    }

    // allocate new object
    auto device  = RHI::get_current_device();
    auto texture = device.create_texture(descriptor);
    auto view    = texture.create_view();
    auto handle  = std::make_pair(texture.handle, view.handle);
    objects.push_back({handle, true});
    return handle;
}

void FrameGraphAllocator::recycle(const GPUTextureDescriptor& descriptor, FGTextureObject texture)
{
    textures.try_emplace(descriptor, FGTextureVector{});
    auto it = textures.find(descriptor);

    // find from existing allocated objects
    auto& objects = it->second;
    for (auto& object : objects)
        if (object.data == texture)
            object.used = false;
}
