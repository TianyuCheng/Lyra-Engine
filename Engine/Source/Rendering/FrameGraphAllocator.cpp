#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

FGBufferObject FrameGraphAllocator::allocate(const GPUBufferDescriptor& descriptor)
{
    buffers.try_emplace(descriptor, FGBufferVector{});
    auto it = buffers.find(descriptor);

    // allocate from existing objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (!object.used) {
            object.used          = true;
            object.unused_frames = 0;
            return object.data;
        }
    }

    // allocate new object
    auto device = RHI::get_current_device();
    auto buffer = device.create_buffer(descriptor);
    objects.push_back({buffer.handle, true, 0});
    return buffer.handle;
}

void FrameGraphAllocator::recycle(const GPUBufferDescriptor& descriptor, FGBufferObject buffer)
{
    buffers.try_emplace(descriptor, FGBufferVector{});
    auto it = buffers.find(descriptor);

    // find from existing allocated objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (object.data == buffer) {
            object.used          = false;
            object.unused_frames = 0;
            break;
        }
    }
}

FGTextureObject FrameGraphAllocator::allocate(const GPUTextureDescriptor& descriptor)
{
    textures.try_emplace(descriptor, FGTextureVector{});
    auto it = textures.find(descriptor);

    // allocate from existing objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (!object.used) {
            object.used          = true;
            object.unused_frames = 0;
            return object.data;
        }
    }

    // allocate new object
    auto device  = RHI::get_current_device();
    auto texture = device.create_texture(descriptor);
    auto view    = texture.create_view();
    auto handle  = FGTextureObject{texture.handle, view.handle};
    objects.push_back({handle, true, 0});
    return handle;
}

void FrameGraphAllocator::recycle(const GPUTextureDescriptor& descriptor, FGTextureObject texture)
{
    textures.try_emplace(descriptor, FGTextureVector{});
    auto it = textures.find(descriptor);

    // find from existing allocated objects
    auto& objects = it->second;
    for (auto& object : objects) {
        if (object.data == texture) {
            object.used          = false;
            object.unused_frames = 0;
            break;
        }
    }
}

void FrameGraphAllocator::reclaim_unused()
{
    auto* api = RHI::api();

    for (auto& [desc, vec] : textures) {
        std::erase_if(vec, [api](FGTextureEntry& entry) {
            if (!entry.used) {
                if (api) {
                    if (entry.data.view.valid()) {
                        api->delete_texture_view(entry.data.view);
                    }
                    if (entry.data.texture.valid()) {
                        api->delete_texture(entry.data.texture);
                    }
                }
                return true;
            }
            return false;
        });
    }
    absl::erase_if(textures, [](const auto& item) { return item.second.empty(); });

    for (auto& [desc, vec] : buffers) {
        std::erase_if(vec, [api](FGBufferEntry& entry) {
            if (!entry.used) {
                if (api && entry.data.valid()) {
                    api->delete_buffer(entry.data);
                }
                return true;
            }
            return false;
        });
    }
    absl::erase_if(buffers, [](const auto& item) { return item.second.empty(); });
}

void FrameGraphAllocator::reclaim_unused_if(FunctionRef<bool(const GPUTextureDescriptor&)> texture_pred)
{
    auto* api = RHI::api();

    for (auto& [desc, vec] : textures) {
        if (texture_pred(desc)) {
            std::erase_if(vec, [api](FGTextureEntry& entry) {
                if (!entry.used) {
                    if (api) {
                        if (entry.data.view.valid()) {
                            api->delete_texture_view(entry.data.view);
                        }
                        if (entry.data.texture.valid()) {
                            api->delete_texture(entry.data.texture);
                        }
                    }
                    return true;
                }
                return false;
            });
        }
    }
    absl::erase_if(textures, [](const auto& item) { return item.second.empty(); });
}

void FrameGraphAllocator::reclaim_unused_if(FunctionRef<bool(const GPUBufferDescriptor&)> buffer_pred)
{
    auto* api = RHI::api();

    for (auto& [desc, vec] : buffers) {
        if (buffer_pred(desc)) {
            std::erase_if(vec, [api](FGBufferEntry& entry) {
                if (!entry.used) {
                    if (api && entry.data.valid()) {
                        api->delete_buffer(entry.data);
                    }
                    return true;
                }
                return false;
            });
        }
    }
    absl::erase_if(buffers, [](const auto& item) { return item.second.empty(); });
}

void FrameGraphAllocator::reclaim_unused_if(FunctionRef<bool(const GPUTextureDescriptor&)> texture_pred,
    FunctionRef<bool(const GPUBufferDescriptor&)>                                          buffer_pred)
{
    reclaim_unused_if(texture_pred);
    reclaim_unused_if(buffer_pred);
}

void FrameGraphAllocator::tick(uint max_unused_frames)
{
    auto* api = RHI::api();

    for (auto& [desc, vec] : textures) {
        std::erase_if(vec, [api, max_unused_frames](FGTextureEntry& entry) {
            if (!entry.used) {
                entry.unused_frames++;
                if (entry.unused_frames >= max_unused_frames) {
                    if (api) {
                        if (entry.data.view.valid()) {
                            api->delete_texture_view(entry.data.view);
                        }
                        if (entry.data.texture.valid()) {
                            api->delete_texture(entry.data.texture);
                        }
                    }
                    return true;
                }
            }
            return false;
        });
    }
    absl::erase_if(textures, [](const auto& item) { return item.second.empty(); });

    for (auto& [desc, vec] : buffers) {
        std::erase_if(vec, [api, max_unused_frames](FGBufferEntry& entry) {
            if (!entry.used) {
                entry.unused_frames++;
                if (entry.unused_frames >= max_unused_frames) {
                    if (api && entry.data.valid()) {
                        api->delete_buffer(entry.data);
                    }
                    return true;
                }
            }
            return false;
        });
    }
    absl::erase_if(buffers, [](const auto& item) { return item.second.empty(); });
}

uint FrameGraphAllocator::allocated_texture_count() const
{
    uint count = 0;
    for (const auto& [desc, vec] : textures) {
        count += static_cast<uint>(vec.size());
    }
    return count;
}

uint FrameGraphAllocator::allocated_buffer_count() const
{
    uint count = 0;
    for (const auto& [desc, vec] : buffers) {
        count += static_cast<uint>(vec.size());
    }
    return count;
}

FrameGraphAllocator::~FrameGraphAllocator()
{
    clear();
}

void FrameGraphAllocator::clear()
{
    auto* api = RHI::api();
    if (!api) {
        buffers.clear();
        textures.clear();
        return;
    }

    for (auto& [desc, vec] : buffers) {
        for (auto& entry : vec) {
            if (entry.data.valid()) {
                api->delete_buffer(entry.data);
            }
        }
    }
    buffers.clear();

    for (auto& [desc, vec] : textures) {
        for (auto& entry : vec) {
            if (entry.data.view.valid()) {
                api->delete_texture_view(entry.data.view);
            }
            if (entry.data.texture.valid()) {
                api->delete_texture(entry.data.texture);
            }
        }
    }
    textures.clear();
}
