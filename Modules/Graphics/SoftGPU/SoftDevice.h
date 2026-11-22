#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTDEVICE_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTDEVICE_H

#include "SoftCommon.h"
#include "SoftBuffer.h"
#include "SoftTexture.h"
#include "SoftSampler.h"
#include "SoftFence.h"
#include "SoftSwapchain.h"
#include "SoftRaytracing.h"
#include "SoftPipeline.h"

namespace lyra::softgpu
{
    class SoftCommandBuffer;

    struct SoftQuerySet
    {
        GPUQueryType  type;
        GPUSize32     count;
        Vector<ulong> results;
        CString       label = "";

        void destroy() {}
    };

    struct SoftDevice
    {
        GPUAdapterProps adapter_props;
        Atomic<bool>    initialized{false};

        SoftResourceManager<SoftBuffer>             buffers;
        SoftResourceManager<SoftTexture>            textures;
        SoftResourceManager<SoftTextureView>        texture_views;
        SoftResourceManager<SoftSampler>            samplers;
        SoftResourceManager<SoftFence>              fences;
        SoftResourceManager<SoftBlas>               blases;
        SoftResourceManager<SoftTlas>               tlases;
        SoftResourceManager<SoftPipelineLayout>     pipeline_layouts;
        SoftResourceManager<SoftBindGroupLayout>    bind_group_layouts;
        SoftResourceManager<SoftBindGroup>          bind_groups;
        SoftResourceManager<SoftShaderModule>       shader_modules;
        SoftResourceManager<SoftRenderPipeline>     render_pipelines;
        SoftResourceManager<SoftComputePipeline>    compute_pipelines;
        SoftResourceManager<SoftRayTracingPipeline> raytracing_pipelines;
        SoftResourceManager<SoftSwapchain>          swapchains;
        SoftResourceManager<SoftQuerySet>           query_sets;

        HashMap<ulong, Own<SoftCommandBuffer>> command_buffers;
        Atomic<ulong>                          cmd_counter{1};
        Atomic<ulong>                          heap_counter{1};

        SoftDevice() = default;

        bool init(const RHIDescriptor& descriptor);
        void destroy();

        SoftCommandBuffer& command(GPUCommandEncoderHandle handle);
    };

    SoftDevice* get_device();
} // namespace lyra::softgpu

namespace api
{
    using namespace lyra;

    CString get_api_name();

    bool create_instance(const RHIDescriptor& desc);
    void delete_instance();

    bool create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& descriptor);
    void delete_adapter();

    bool create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor& desc);
    void delete_surface(GPUSurfaceHandle surface);
    bool get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent);
    bool get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format);
    uint get_surface_frames(GPUSurfaceHandle surface);

    bool create_device(const GPUDeviceDescriptor& desc);
    void delete_device();

    bool create_fence(GPUFenceHandle& fence);
    void delete_fence(GPUFenceHandle fence);

    bool create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc);
    void delete_buffer(GPUBufferHandle buffer);
    void map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size);
    void unmap_buffer(GPUBufferHandle buffer);
    void get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range);
    void get_mapped_state(GPUBufferHandle buffer, GPUMapState& state);

    bool create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc);
    void delete_sampler(GPUSamplerHandle sampler);

    bool create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc);
    void delete_texture(GPUTextureHandle texture);
    bool create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc);
    void delete_texture_view(GPUTextureViewHandle view);

    bool create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc);
    void delete_shader_module(GPUShaderModuleHandle shader);

    bool create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& descriptor, GPUBlasGeometrySizeDescriptors sizes);
    void delete_blas(GPUBlasHandle blas);
    bool get_blas_sizes(GPUBlasHandle blas, GPUBVHSizes& sizes);

    bool create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& descriptor);
    void delete_tlas(GPUTlasHandle tlas);
    bool get_tlas_sizes(GPUTlasHandle tlas, GPUBVHSizes& sizes);

    bool create_query_set(GPUQuerySetHandle& query_set, const GPUQuerySetDescriptor& descriptor);
    void delete_query_set(GPUQuerySetHandle query_set);

    bool create_bind_group_layout(GPUBindGroupLayoutHandle& handle, const GPUBindGroupLayoutDescriptor& desc);
    void delete_bind_group_layout(GPUBindGroupLayoutHandle handle);

    bool create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc);
    void delete_pipeline_layout(GPUPipelineLayoutHandle layout);

    bool create_render_pipeline(GPURenderPipelineHandle& handle, const GPURenderPipelineDescriptor& desc);
    void delete_render_pipeline(GPURenderPipelineHandle pipeline);
    bool create_compute_pipeline(GPUComputePipelineHandle& handle, const GPUComputePipelineDescriptor& desc);
    void delete_compute_pipeline(GPUComputePipelineHandle pipeline);
    bool create_raytracing_pipeline(GPURayTracingPipelineHandle& handle, const GPURayTracingPipelineDescriptor& desc);
    void delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline);

    void new_frame();
    void end_frame();

    bool acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view, GPUFenceHandle& image_available_fence, GPUFenceHandle& render_complete_fence, bool& suboptimal);
    bool present_curr_frame(GPUSurfaceHandle surface);

    bool create_bind_group(GPUBindGroupHandle& bind_group, const GPUBindGroupDescriptor& desc);
    bool create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor& desc);
    void delete_bind_group_heap(GPUBindGroupHeapHandle heap);
    void reset_bind_group_heap(GPUBindGroupHeapHandle heap);

    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor& descriptor);
    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor& descriptor);
    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer);

    void wait_idle();
    void wait_fence(GPUFenceHandle handle);
    bool poll_fence(GPUFenceHandle handle);
} // namespace api

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTDEVICE_H
