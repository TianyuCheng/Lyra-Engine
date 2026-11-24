#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

FrameGraphAllocator::FrameGraphAllocator(uint frames_in_flight)
    : frames_in_flight(frames_in_flight)
{
}

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

GPUBindGroup FrameGraphAllocator::allocate(const GPUBindGroupDescriptor& descriptor, GPUDevice dev)
{
    auto device = dev.valid() ? dev : RHI::get_current_device();
    auto desc   = descriptor;

    if (!desc.heap.valid()) {
        if (bind_group_heaps.empty()) {
            bind_group_heaps.resize(std::max(1u, frames_in_flight));
            for (auto& heap : bind_group_heaps) {
                auto heap_desc      = GPUBindGroupHeapDescriptor{};
                heap_desc.page_size = 4096;
                heap                = device.create_bind_group_heap(heap_desc);
            }
        }
        if (current_frame_slot < bind_group_heaps.size() && bind_group_heaps[current_frame_slot].valid()) {
            desc.heap = bind_group_heaps[current_frame_slot].handle;
        }
    }

    return device.create_bind_group(desc);
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
    absl::erase_if(textures, [](const auto& item) {
        return item.second.empty();
    });

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
    absl::erase_if(buffers, [](const auto& item) {
        return item.second.empty();
    });
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
    absl::erase_if(textures, [](const auto& item) {
        return item.second.empty();
    });
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
    absl::erase_if(buffers, [](const auto& item) {
        return item.second.empty();
    });
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
    absl::erase_if(textures, [](const auto& item) {
        return item.second.empty();
    });

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
    absl::erase_if(buffers, [](const auto& item) {
        return item.second.empty();
    });

    if (frames_in_flight > 0 && !bind_group_heaps.empty()) {
        current_frame_slot = (current_frame_slot + 1) % frames_in_flight;
        if (bind_group_heaps[current_frame_slot].valid()) {
            bind_group_heaps[current_frame_slot].reset();
        }
    }
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

uint FrameGraphAllocator::allocated_bind_group_count() const
{
    uint count = 0;
    for (const auto& heap : bind_group_heaps) {
        if (heap.valid()) {
            count++;
        }
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

    for (auto& heap : bind_group_heaps) {
        if (heap.valid() && api) {
            heap.destroy();
        }
    }
    bind_group_heaps.clear();
    current_frame_slot = 0;

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
