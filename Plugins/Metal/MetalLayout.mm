#include "MetalUtils.h"
#import <Metal/MTLArgument.h>
#include <unordered_map>

#ifndef MTLDataTypeAccelerationStructure
#define MTLDataTypeAccelerationStructure ((MTLDataType)73)
#endif

namespace {
MTLDataType get_mtl_data_type(GPUResourceType type) {
    switch (type) {
        case GPUResourceType::BUFFER: return MTLDataTypePointer;
        case GPUResourceType::TEXTURE:
        case GPUResourceType::STORAGE_TEXTURE: return MTLDataTypeTexture;
        case GPUResourceType::SAMPLER: return MTLDataTypeSampler;
        case GPUResourceType::ACCELERATION_STRUCTURE: return MTLDataTypeAccelerationStructure;
        default: return MTLDataTypeNone;
    }
}
} // namespace

using namespace lyra;

MetalBindGroupLayout::MetalBindGroupLayout() {}

MetalBindGroupLayout::MetalBindGroupLayout(const GPUBindGroupLayoutDescriptor& desc)
{
    // store entries for pipeline layout mapping
    for (auto& entry : desc.entries) {
        entries.push_back(entry);
    }
    if (entries.empty()) {
        return;
    }

    is_argument_buffer = entries[0].binding.from_argument_buffer;
    
    if (is_argument_buffer) {
        auto rhi = get_rhi();
        NSMutableArray* arguments = [NSMutableArray new];
        for (const auto& entry : entries) {
            if (entry.binding.from_argument_buffer != is_argument_buffer) {
                 throw GPUValidationError("Mixing argument buffer and direct bindings in one bind group layout is not supported.");
            }
            MTLArgumentDescriptor* arg = [MTLArgumentDescriptor new];
            arg.index = entry.binding.index;
            arg.dataType = get_mtl_data_type(entry.type);
            arg.arrayLength = entry.count;
            arg.access = MTLBindingAccessReadOnly;

            if (arg.dataType == MTLDataTypeTexture) {
                arg.textureType = mtlenum(entry.texture.view_dimension);
            }
            [arguments addObject:arg];
        }
        arg_encoder = [rhi->device newArgumentEncoderWithArguments:arguments];
    }
}

void MetalBindGroupLayout::destroy()
{
    entries.clear();
    arg_encoder = nil;
}

MetalPipelineLayout::MetalPipelineLayout()
{
    // do nothing
}

MetalPipelineLayout::MetalPipelineLayout(const GPUPipelineLayoutDescriptor& desc)
{
    auto rhi = get_rhi();

    // store push constant ranges
    for (const auto& range : desc.push_constant_ranges) {
        push_constant_ranges.push_back(range);
    }

    // generate flat mapping
    uint32_t current_buffer_index  = 0;
    uint32_t current_texture_index = 0;
    uint32_t current_sampler_index = 0;

    for (uint32_t set = 0; set < desc.bind_group_layouts.size(); ++set) {
        GPUBindGroupLayoutHandle handle = desc.bind_group_layouts[set];
        bind_group_layouts.push_back(handle);

        auto& layout = fetch_resource(rhi->bind_group_layouts, handle);

        if (layout.is_argument_buffer) {
            uint32_t key = (set << 16) | 0; // Use binding 0 as representative for the set
            buffer_indices[key] = current_buffer_index++;
        } else {
            for (const auto& entry : layout.entries) {
                uint32_t key = (set << 16) | entry.binding.index;

                switch (entry.type) {
                    case GPUResourceType::BUFFER:
                    case GPUResourceType::ACCELERATION_STRUCTURE:
                        buffer_indices[key] = current_buffer_index++;
                        break;
                    case GPUResourceType::TEXTURE:
                    case GPUResourceType::STORAGE_TEXTURE:
                        texture_indices[key] = current_texture_index++;
                        break;
                    case GPUResourceType::SAMPLER:
                        sampler_indices[key] = current_sampler_index++;
                        break;
                }
            }
        }
    }

    max_buffer_index  = current_buffer_index;
    max_texture_index = current_texture_index;
    max_sampler_index = current_sampler_index;

    // bounds checking
    if (max_buffer_index >= METAL_PushConstantBufferIndex) {
        get_logger()->error("Pipeline layout exceeds available Metal buffer slots! Used: {}, Available: <{}",
            max_buffer_index, METAL_PushConstantBufferIndex);
        throw GPUValidationError("Pipeline layout exceeds available Metal buffer slots");
    }
}

void MetalPipelineLayout::destroy()
{
    bind_group_layouts.clear();
    push_constant_ranges.clear();
    buffer_indices.clear();
    texture_indices.clear();
    sampler_indices.clear();
}

bool api::create_bind_group_layout(GPUBindGroupLayoutHandle& handle, const GPUBindGroupLayoutDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalBindGroupLayout(desc);
    auto ind = rhi->bind_group_layouts.add(obj);
    handle   = GPUBindGroupLayoutHandle(ind);
    return obj.valid();
}

void api::delete_bind_group_layout(GPUBindGroupLayoutHandle handle)
{
    get_rhi()->bind_group_layouts.remove(handle.value);
}

bool api::create_pipeline_layout(GPUPipelineLayoutHandle& handle, const GPUPipelineLayoutDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalPipelineLayout(desc);
    auto ind = rhi->pipeline_layouts.add(obj);
    handle   = GPUPipelineLayoutHandle(ind);
    return obj.valid();
}

void api::delete_pipeline_layout(GPUPipelineLayoutHandle handle)
{
    get_rhi()->pipeline_layouts.remove(handle.value);
}
