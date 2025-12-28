#include "MetalUtils.h"
using namespace lyra;

// helper function to infer MTLArgumentType from binding layout entry
static MTLDataType infer_buffer_data_type(const GPUBufferBindingLayout& entry)
{
    // for buffers, we use MTLDataTypePointer
    return MTLDataTypePointer;
}

static MTLArgumentAccess infer_buffer_access(const GPUBufferBindingLayout& entry)
{
    switch (entry.type) {
        case GPUBufferBindingType::UNIFORM:
            return MTLArgumentAccessReadOnly;
        case GPUBufferBindingType::STORAGE:
            return MTLArgumentAccessReadWrite;
        case GPUBufferBindingType::READ_ONLY_STORAGE:
            return MTLArgumentAccessReadOnly;
        default:
            return MTLArgumentAccessReadOnly;
    }
}

static MTLArgumentAccess infer_storage_texture_access(const GPUStorageTextureBindingLayout& entry)
{
    switch (entry.access) {
        case GPUStorageTextureAccess::WRITE_ONLY:
            return MTLArgumentAccessWriteOnly;
        case GPUStorageTextureAccess::READ_ONLY:
            return MTLArgumentAccessReadOnly;
        case GPUStorageTextureAccess::READ_WRITE:
            return MTLArgumentAccessReadWrite;
        default:
            return MTLArgumentAccessReadWrite;
    }
}

MetalBindGroupLayout::MetalBindGroupLayout() {}

MetalBindGroupLayout::MetalBindGroupLayout(const GPUBindGroupLayoutDescriptor& desc)
{
    auto rhi = get_rhi();

    NSMutableArray<MTLArgumentDescriptor*>* args = [NSMutableArray new];

    for (auto& entry : desc.entries) {
        MTLArgumentDescriptor* arg = [MTLArgumentDescriptor new];
        arg.index                  = entry.binding.index;
        arg.arrayLength            = entry.count;

        switch (entry.type) {
            case GPUResourceType::BUFFER:
                arg.dataType = infer_buffer_data_type(entry.buffer);
                arg.access   = infer_buffer_access(entry.buffer);
                break;

            case GPUResourceType::SAMPLER:
                arg.dataType = MTLDataTypeSampler;
                arg.access   = MTLArgumentAccessReadOnly;
                break;

            case GPUResourceType::TEXTURE:
                arg.dataType = MTLDataTypeTexture;
                arg.access   = MTLArgumentAccessReadOnly;
                break;

            case GPUResourceType::STORAGE_TEXTURE:
                arg.dataType = MTLDataTypeTexture;
                arg.access   = infer_storage_texture_access(entry.storage_texture);
                break;

            case GPUResourceType::ACCELERATION_STRUCTURE:
                // Metal 3+ acceleration structure support
                arg.dataType = MTLDataTypePointer;
                arg.access   = MTLArgumentAccessReadOnly;
                break;

            default:
                get_logger()->error("Unsupported binding resource type in bind group layout");
                continue;
        }

        [args addObject:arg];
    }

    argument_descriptors = args;

    // create encoder to determine encoded length
    if ([args count] > 0) {
        encoder        = [rhi->device newArgumentEncoderWithArguments:args];
        encoded_length = [encoder encodedLength];
    }
}

void MetalBindGroupLayout::destroy()
{
    argument_descriptors = nil;
    encoder              = nil;
    encoded_length       = 0;
}

MetalPipelineLayout::MetalPipelineLayout() {}

MetalPipelineLayout::MetalPipelineLayout(const GPUPipelineLayoutDescriptor& desc)
{
    // store bind group layout handles
    for (const auto& handle : desc.bind_group_layouts) {
        bind_group_layouts.push_back(handle);
    }

    // store push constant ranges
    for (const auto& range : desc.push_constant_ranges) {
        push_constant_ranges.push_back(range);
    }
}

void MetalPipelineLayout::destroy()
{
    bind_group_layouts.clear();
    push_constant_ranges.clear();
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
