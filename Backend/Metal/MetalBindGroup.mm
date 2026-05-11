#include <new>
#include <memory>
#include "MetalUtils.h"

using namespace lyra;

#pragma region MetalBindGroup
MetalBindGroup::MetalBindGroup() {}

void MetalBindGroup::init(const GPUBindGroupDescriptor& desc)
{
    @autoreleasepool {
        auto  rhi    = get_rhi();
        auto& layout = fetch_resource(rhi->bind_group_layouts, desc.layout);

        if (layout.encoder) {
            init_argument_buffer(desc, layout);
        } else {
            init_direct_binding(desc);
        }
    }
}

void MetalBindGroup::destroy()
{
    @autoreleasepool {
        argument_buffer = nil;
    }
}

void MetalBindGroup::init_argument_buffer(const GPUBindGroupDescriptor& desc, const MetalBindGroupLayout& layout)
{
    auto rhi = get_rhi();

    entry_count = static_cast<uint32_t>(desc.entries.size());
    entries     = reinterpret_cast<MetalBindGroup::Entry*>(reinterpret_cast<uint8_t*>(this) + sizeof(MetalBindGroup));

    argument_buffer = [rhi->device newBufferWithLength:layout.encoder.encodedLength options:MTLResourceStorageModeShared];
    if (!argument_buffer) {
        throw GPUOutOfMemoryError("Failed to create argument buffer");
    }

    [layout.encoder setArgumentBuffer:argument_buffer offset:0];

    for (uint32_t i = 0; i < entry_count; ++i) {
        const auto& src = desc.entries[i];
        auto&       dst = entries[i];

        dst.binding = src.binding;
        dst.type    = src.type;
        dst.usage   = MTLResourceUsageRead;

        const GPUBindGroupLayoutEntry* layout_entry = nullptr;
        for (const auto& le : layout.entries) {
            if (le.binding.index == src.binding) {
                layout_entry = &le;
                break;
            }
        }

        if (layout_entry) {
            if (layout_entry->type == GPUResourceType::BUFFER) {
                if (layout_entry->buffer.type == GPUBufferBindingType::STORAGE) {
                    dst.usage = MTLResourceUsageRead | MTLResourceUsageWrite;
                }
            } else if (layout_entry->type == GPUResourceType::STORAGE_TEXTURE) {
                if (layout_entry->storage_texture.access == GPUStorageTextureAccess::WRITE_ONLY) {
                    dst.usage = MTLResourceUsageWrite;
                } else {
                    dst.usage = MTLResourceUsageRead;
                    if (layout_entry->storage_texture.access == GPUStorageTextureAccess::READ_WRITE) {
                        dst.usage |= MTLResourceUsageWrite;
                    }
                }
            }
        }

        switch (src.type) {
            case GPUResourceType::BUFFER:
            {
                auto& buffer = fetch_resource(rhi->buffers, src.buffer.buffer);
                [layout.encoder setBuffer:buffer.buffer offset:src.buffer.offset atIndex:src.binding];
                dst.buffer.buffer = buffer.buffer;
                dst.buffer.offset = src.buffer.offset;
                break;
            }
            case GPUResourceType::SAMPLER:
            {
                auto& sampler = fetch_resource(rhi->samplers, src.sampler);
                [layout.encoder setSamplerState:sampler.sampler atIndex:src.binding];
                dst.sampler = sampler.sampler;
                break;
            }
            case GPUResourceType::TEXTURE:
            case GPUResourceType::STORAGE_TEXTURE:
            {
                auto& view = fetch_resource(rhi->views, src.texture);
                [layout.encoder setTexture:view.texture atIndex:src.binding];
                dst.texture = view.texture;
                break;
            }
            case GPUResourceType::ACCELERATION_STRUCTURE:
            {
                if (!src.tlas.valid()) {
                    throw GPUValidationError("Invalid TLAS handle for acceleration structure binding");
                }
                auto& tlas = fetch_resource(rhi->tlases, src.tlas);
                [layout.encoder setAccelerationStructure:tlas.tlas atIndex:src.binding];
                dst.tlas = tlas.tlas;
                break;
            }
        }
    }
}

void MetalBindGroup::init_direct_binding(const GPUBindGroupDescriptor& desc)
{
    auto rhi = get_rhi();

    entry_count = static_cast<uint32_t>(desc.entries.size());
    entries     = reinterpret_cast<MetalBindGroup::Entry*>(reinterpret_cast<uint8_t*>(this) + sizeof(MetalBindGroup));

    for (uint32_t i = 0; i < entry_count; ++i) {
        const auto& src = desc.entries[i];
        auto&       dst = entries[i];

        dst.binding = src.binding;
        dst.type    = src.type;
        dst.usage   = MTLResourceUsageRead;

        switch (src.type) {
            case GPUResourceType::BUFFER:
            {
                auto& buffer      = fetch_resource(rhi->buffers, src.buffer.buffer);
                dst.buffer.buffer = buffer.buffer;
                dst.buffer.offset = src.buffer.offset;
                break;
            }
            case GPUResourceType::SAMPLER:
            {
                auto& sampler = fetch_resource(rhi->samplers, src.sampler);
                dst.sampler   = sampler.sampler;
                break;
            }
            case GPUResourceType::TEXTURE:
            case GPUResourceType::STORAGE_TEXTURE:
            {
                auto& view  = fetch_resource(rhi->views, src.texture);
                dst.texture = view.texture;
                break;
            }
            case GPUResourceType::ACCELERATION_STRUCTURE:
            {
                if (!src.tlas.valid()) {
                    throw GPUValidationError("Invalid TLAS handle for acceleration structure binding");
                }
                auto& tlas = fetch_resource(rhi->tlases, src.tlas);
                dst.tlas   = tlas.tlas;
                break;
            }
        }
    }
}

void MetalBindGroup::bind(MetalCommandBuffer& cmd, const MetalPipelineLayout& pipeline_layout, GPUIndex32 index, GPUBufferDynamicOffsets dynamic_offsets)
{
    auto  rhi           = get_rhi();
    auto  layout_handle = pipeline_layout.bind_group_layouts.at(index);
    auto& layout        = fetch_resource(rhi->bind_group_layouts, layout_handle);

    if (layout.encoder) {
        bind_argument_buffer(cmd, pipeline_layout, index);
    } else {
        bind_direct(cmd, pipeline_layout, index, dynamic_offsets);
    }
}

void MetalBindGroup::bind_argument_buffer(MetalCommandBuffer& cmd, const MetalPipelineLayout& pipeline_layout, GPUIndex32 index)
{
    uint32_t key = (index << 16) | 0;
    if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
        uint32_t slot = pipeline_layout.buffer_indices.at(key);
        if (cmd.render_encoder) {
            [cmd.render_encoder setVertexBuffer:this->argument_buffer offset:0 atIndex:slot];
            [cmd.render_encoder setFragmentBuffer:this->argument_buffer offset:0 atIndex:slot];
            declare_resource_usages(cmd.render_encoder);
        } else if (cmd.compute_encoder) {
            [cmd.compute_encoder setBuffer:this->argument_buffer offset:0 atIndex:slot];
            declare_resource_usages(cmd.compute_encoder);
        }
    }
}

void MetalBindGroup::bind_direct(MetalCommandBuffer& cmd, const MetalPipelineLayout& pipeline_layout, GPUIndex32 index, GPUBufferDynamicOffsets dynamic_offsets)
{
    for (uint32_t i = 0; i < this->entry_count; ++i) {
        const auto& entry = this->entries[i];
        uint32_t    key   = (index << 16) | entry.binding;

        switch (entry.type) {
            case GPUResourceType::BUFFER:
            {
                uint64_t final_offset = entry.buffer.offset;
                auto     it           = pipeline_layout.dynamic_binding_to_offset_index.find(key);
                if (it != pipeline_layout.dynamic_binding_to_offset_index.end()) {
                    uint32_t dynamic_offset_index = it->second;
                    if (dynamic_offset_index < dynamic_offsets.size()) {
                        final_offset += dynamic_offsets[dynamic_offset_index];
                    }
                }

                if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
                    uint32_t slot = pipeline_layout.buffer_indices.at(key);
                    if (cmd.render_encoder) {
                        [cmd.render_encoder setVertexBuffer:entry.buffer.buffer offset:final_offset atIndex:slot];
                        [cmd.render_encoder setFragmentBuffer:entry.buffer.buffer offset:final_offset atIndex:slot];
                    } else if (cmd.compute_encoder) {
                        [cmd.compute_encoder setBuffer:entry.buffer.buffer offset:final_offset atIndex:slot];
                    }
                }
                break;
            }
            case GPUResourceType::TEXTURE:
            case GPUResourceType::STORAGE_TEXTURE:
            {
                if (pipeline_layout.texture_indices.find(key) != pipeline_layout.texture_indices.end()) {
                    uint32_t slot = pipeline_layout.texture_indices.at(key);
                    if (cmd.render_encoder) {
                        [cmd.render_encoder setVertexTexture:entry.texture atIndex:slot];
                        [cmd.render_encoder setFragmentTexture:entry.texture atIndex:slot];
                    } else if (cmd.compute_encoder) {
                        [cmd.compute_encoder setTexture:entry.texture atIndex:slot];
                    }
                }
                break;
            }
            case GPUResourceType::SAMPLER:
            {
                if (pipeline_layout.sampler_indices.find(key) != pipeline_layout.sampler_indices.end()) {
                    uint32_t slot = pipeline_layout.sampler_indices.at(key);
                    if (cmd.render_encoder) {
                        [cmd.render_encoder setVertexSamplerState:entry.sampler atIndex:slot];
                        [cmd.render_encoder setFragmentSamplerState:entry.sampler atIndex:slot];
                    } else if (cmd.compute_encoder) {
                        [cmd.compute_encoder setSamplerState:entry.sampler atIndex:slot];
                    }
                }
                break;
            }
            case GPUResourceType::ACCELERATION_STRUCTURE:
            {
                if (@available(macOS 13.0, iOS 16.0, *)) {
                    if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
                        uint32_t slot = pipeline_layout.buffer_indices.at(key);
                        if (cmd.render_encoder) {
                            [cmd.render_encoder setVertexAccelerationStructure:entry.tlas atBufferIndex:slot];
                            [cmd.render_encoder setFragmentAccelerationStructure:entry.tlas atBufferIndex:slot];
                        } else if (cmd.compute_encoder) {
                            [cmd.compute_encoder setAccelerationStructure:entry.tlas atBufferIndex:slot];
                        }
                    }
                }
                break;
            }
        }
    }
}
#pragma endregion MetalBindGroup

#pragma region MetalBindGroupHeap
MetalBindGroupHeap::MetalBindGroupHeap()
{
    arena = std::make_shared<lyra::MemoryArena>(64 * 1024); // default 64KB page size
}

MetalBindGroupHeap::MetalBindGroupHeap(const GPUBindGroupHeapDescriptor& desc)
{
    arena = std::make_shared<lyra::MemoryArena>(desc.page_size > 0 ? desc.page_size : 64 * 1024);
}

void* MetalBindGroupHeap::allocate(size_t size, size_t alignment)
{
    return arena->allocate(size, alignment);
}

GPUBindGroupHandle MetalBindGroupHeap::allocate(const GPUBindGroupDescriptor& desc)
{
    size_t total_size = sizeof(MetalBindGroup) + desc.entries.size() * sizeof(MetalBindGroup::Entry);

    void* memory = allocate(total_size, alignof(MetalBindGroup));
    if (!memory) {
        get_logger()->error("Failed to allocate memory for bind group from arena");
        throw GPUOutOfMemoryError("Failed to allocate memory for bind group from arena");
    }

    MetalBindGroup* group = new (memory) MetalBindGroup();
    group->init(desc);
    return GPUBindGroupHandle(reinterpret_cast<uint64_t>(group));
}

void MetalBindGroupHeap::reset()
{
    arena->reset();
}

void MetalBindGroupHeap::destroy()
{

    arena->destroy();
}
#pragma endregion MetalBindGroupHeap
