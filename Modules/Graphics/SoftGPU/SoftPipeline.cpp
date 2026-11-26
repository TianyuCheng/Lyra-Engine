#include "SoftPipeline.h"
#include "SoftDevice.h"

using namespace lyra;
using namespace lyra::softgpu;

SoftPipelineLayout::SoftPipelineLayout(const GPUPipelineLayoutDescriptor& desc)
    : bind_group_layouts(desc.bind_group_layouts.begin(), desc.bind_group_layouts.end()),
      immediate_size(desc.immediate_size),
      label(desc.label)
{
    // nothing
}

SoftBindGroupLayout::SoftBindGroupLayout(const GPUBindGroupLayoutDescriptor& descriptor)
    : desc(descriptor), label(descriptor.label)
{
    // nothing
}

SoftBindGroup::SoftBindGroup(const GPUBindGroupDescriptor& descriptor)
    : desc(descriptor), label(descriptor.label)
{
    entries.reserve(descriptor.entries.size());
    for (const auto& entry : descriptor.entries) {
        entries.push_back(entry);
    }
}

const GPUBindGroupEntry* SoftBindGroup::find_entry(uint binding) const
{
    for (const auto& entry : entries) {
        if (entry.binding == binding) {
            return &entry;
        }
    }
    return nullptr;
}

SoftRenderPipeline::SoftRenderPipeline(const GPURenderPipelineDescriptor& desc)
    : layout(desc.layout),
      primitive(desc.primitive),
      depth_stencil(desc.depth_stencil),
      multisample(desc.multisample),
      label(desc.label)
{
    if (desc.vertex.buffers.data() && desc.vertex.buffers.size() > 0) {
        for (const auto& vb : desc.vertex.buffers) {
            SoftVertexBufferLayout svb{};
            svb.array_stride = vb.array_stride;
            svb.step_mode    = vb.step_mode;
            for (const auto& attr : vb.attributes) {
                SoftVertexAttribute sa{};
                sa.format          = attr.format;
                sa.offset          = attr.offset;
                sa.shader_location = attr.shader_location;
                svb.attributes.push_back(sa);
            }
            vertex_buffers.push_back(svb);
        }
    }
    if (desc.fragment.targets.data() && desc.fragment.targets.size() > 0) {
        color_targets.assign(desc.fragment.targets.begin(), desc.fragment.targets.end());
    }

    if (desc.vertex.module.valid()) {
        vshader_module = desc.vertex.module;
        if (desc.vertex.entry_point) ventry_point = desc.vertex.entry_point;
        auto* dev = get_device();
        auto* sm  = dev ? dev->shader_modules.find(desc.vertex.module.to_slotmap_handle<SoftShaderModule>()) : nullptr;
        if (sm) {
            vshader_func = sm->host_func;
        }
    }
    if (desc.fragment.module.valid()) {
        fshader_module = desc.fragment.module;
        if (desc.fragment.entry_point) fentry_point = desc.fragment.entry_point;
        auto* dev = get_device();
        auto* sm  = dev ? dev->shader_modules.find(desc.fragment.module.to_slotmap_handle<SoftShaderModule>()) : nullptr;
        if (sm) {
            fshader_func = sm->host_func;
            depth_offset = sm->depth_offset;
        }
    }
}

SoftComputePipeline::SoftComputePipeline(const GPUComputePipelineDescriptor& desc)
    : layout(desc.layout),
      compute(desc.compute),
      label(desc.label)
{
    if (desc.compute.module.valid()) {
        auto* dev = get_device();
        auto* sm  = dev ? dev->shader_modules.find(desc.compute.module.to_slotmap_handle<SoftShaderModule>()) : nullptr;
        if (sm) {
            cshader_func = sm->host_func;
        }
    }
}

SoftRayTracingPipeline::SoftRayTracingPipeline(const GPURayTracingPipelineDescriptor& desc)
    : layout(desc.layout),
      max_recursion_depth(desc.max_recursion_depth),
      label(desc.label)
{
}
