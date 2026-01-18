#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_RHI_TYPES_H
#define LYRA_LIBRARY_PLUGIN_RHI_TYPES_H

// reference: https://gpuweb.github.io/gpuweb/#

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Plugin/RHI/RHIEnums.h>
#include <Lyra/Plugin/RHI/RHIUtils.h>
#include <Lyra/Plugin/RHI/RHIDescs.h>
#include <Lyra/Plugin/RHI/RHIError.h>

namespace lyra
{
    struct RenderAPI;

    struct GPUObjectBase
    {
        CString label = "";
    };

    struct GPUFence : public GPUObjectBase
    {
        GPUFenceHandle handle;

        // implicit conversion
        FORCE_INLINE GPUFence() : handle() {}
        FORCE_INLINE GPUFence(GPUFenceHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUFenceHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void wait() const;

        void destroy();
    };

    struct GPUSampler : public GPUObjectBase
    {
        GPUSamplerHandle handle;

        // implicit conversion
        FORCE_INLINE GPUSampler() : handle() {}
        FORCE_INLINE GPUSampler(GPUSamplerHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUSamplerHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUShaderModule : public GPUObjectBase
    {
        GPUShaderModuleHandle handle;

        // implicit conversion
        FORCE_INLINE GPUShaderModule() : handle() {}
        FORCE_INLINE GPUShaderModule(GPUShaderModuleHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUShaderModuleHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUTextureView : public GPUObjectBase
    {
        GPUTextureViewHandle handle;

        // implicit conversion
        FORCE_INLINE GPUTextureView() : handle() {}
        FORCE_INLINE GPUTextureView(GPUTextureViewHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUTextureViewHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUTexture : public GPUObjectBase
    {
        GPUTextureHandle handle;

        GPUIntegerCoordinateOut width;
        GPUIntegerCoordinateOut height;
        GPUIntegerCoordinate    depth;
        GPUIntegerCoordinate    array_layers;
        GPUIntegerCoordinate    mip_level_count;
        GPUSize32Out            sample_count;
        GPUTextureDimension     dimension;
        GPUTextureFormat        format;
        GPUTextureUsageFlags    usage;

        // implicit conversion
        FORCE_INLINE operator GPUTextureHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        auto create_view() -> GPUTextureView;

        auto create_view(GPUTextureViewDescriptor descriptor) -> GPUTextureView;

        void destroy();
    };

    struct GPUBuffer : public GPUObjectBase
    {
        GPUBufferHandle handle;

        GPUSize64Out        size  = 0;
        GPUBufferUsageFlags usage = 0;

        // implicit conversion
        FORCE_INLINE operator GPUBufferHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        template <typename T>
        FORCE_INLINE auto get_mapped_range() const -> TypedBufferRange<T>
        {
            auto range  = get_mapped_range();
            auto typed  = TypedBufferRange<T>{};
            typed.data  = reinterpret_cast<T*>(range.data);
            typed.count = range.size / sizeof(T);
            return typed;
        }

        auto get_mapped_range() const -> MappedBufferRange;

        auto get_mapped_state() const -> GPUMapState;

        void map(GPUMapMode mode, GPUSize64 offset = 0, GPUSize64 size = 0);

        void unmap();

        void destroy();
    };

    struct GPUQuerySet : public GPUObjectBase
    {
        GPUQuerySetHandle handle;

        GPUQueryType type;
        GPUSize32    count;

        // implicit conversion
        FORCE_INLINE operator GPUQuerySetHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        auto destroy() -> void;
    };

    struct GPUTlas : public GPUObjectBase
    {
        GPUTlasHandle handle;

        // implicit conversion
        FORCE_INLINE GPUTlas() : handle() {}
        FORCE_INLINE GPUTlas(GPUTlasHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUTlasHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        auto destroy() -> void;
    };

    struct GPUBlas : public GPUObjectBase
    {
        GPUBlasHandle handle;

        // implicit conversion
        FORCE_INLINE GPUBlas() : handle() {}
        FORCE_INLINE GPUBlas(GPUBlasHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUBlasHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        auto destroy() -> void;
    };

    struct GPUBindGroup : public GPUObjectBase
    {
        GPUBindGroupHandle handle;

        // NOTE: no manual deletion of GPUBindGroup,
        // because these are automatically recycled by GC.

        // implicit conversion
        FORCE_INLINE GPUBindGroup() : handle() {}
        FORCE_INLINE GPUBindGroup(GPUBindGroupHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUBindGroupHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }
    };

    struct GPUBindGroupHeap : public GPUObjectBase
    {
        GPUBindGroupHeapHandle handle;

        // implicit conversion
        FORCE_INLINE GPUBindGroupHeap() : handle() {}
        FORCE_INLINE GPUBindGroupHeap(GPUBindGroupHeapHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUBindGroupHeapHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUBindGroupLayout : public GPUObjectBase
    {
        GPUBindGroupLayoutHandle handle;

        // implicit conversion
        FORCE_INLINE GPUBindGroupLayout() : handle() {}
        FORCE_INLINE GPUBindGroupLayout(GPUBindGroupLayoutHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUBindGroupLayoutHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUPipelineLayout : public GPUObjectBase
    {
        GPUPipelineLayoutHandle handle;

        // implicit conversion
        FORCE_INLINE GPUPipelineLayout() : handle() {}
        FORCE_INLINE GPUPipelineLayout(GPUPipelineLayoutHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUPipelineLayoutHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPURenderPipeline : public GPUObjectBase
    {
        GPURenderPipelineHandle handle;

        // implicit conversion
        FORCE_INLINE GPURenderPipeline() : handle() {}
        FORCE_INLINE GPURenderPipeline(GPURenderPipelineHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPURenderPipelineHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUComputePipeline : public GPUObjectBase
    {
        GPUComputePipelineHandle handle;

        // implicit conversion
        FORCE_INLINE GPUComputePipeline() : handle() {}
        FORCE_INLINE GPUComputePipeline(GPUComputePipelineHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUComputePipelineHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPURayTracingPipeline : public GPUObjectBase
    {
        GPURayTracingPipelineHandle handle;

        // implicit conversion
        FORCE_INLINE GPURayTracingPipeline() : handle() {}
        FORCE_INLINE GPURayTracingPipeline(GPURayTracingPipelineHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPURayTracingPipelineHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    struct GPUCommandEncoder : public GPUObjectBase
    {
        GPUCommandEncoderHandle handle;

        // implicit conversion
        FORCE_INLINE GPUCommandEncoder() : handle() {}
        FORCE_INLINE GPUCommandEncoder(GPUCommandEncoderHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUCommandEncoderHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void insert_debug_marker(CString marker_label) const;

        void push_debug_group(CString group_label) const;

        void pop_debug_group() const;

        void wait(const GPUFence& fence, GPUBarrierSyncFlags sync) const;

        void signal(const GPUFence& fence, GPUBarrierSyncFlags sync) const;

        void set_pipeline(const GPURenderPipeline& pipeline) const;

        void set_pipeline(const GPUComputePipeline& pipeline) const;

        void set_pipeline(const GPURayTracingPipeline& pipeline) const;

        void set_bind_group(GPUIndex32 index, const GPUBindGroup& bind_group, const Vector<GPUBufferDynamicOffset>& dynamic_offsets = {}) const;

        void set_push_constants(GPUShaderStageFlags visibility, uint offset, uint size, void* data) const;

        template <typename T>
        void set_push_constants(GPUShaderStageFlags visibility, uint offset, T data) const
        {
            set_push_constants(visibility, offset, sizeof(T), reinterpret_cast<uint8_t*>(&data));
        }

        void dispatch_workgroups(GPUSize32 x, GPUSize32 y = 1, GPUSize32 z = 1) const;

        void dispatch_workgroups_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset) const;

        void set_index_buffer(const GPUBuffer& buffer, GPUIndexFormat index_format, GPUSize64 offset = 0, GPUSize64 size = 0) const;

        void set_vertex_buffer(GPUIndex32 slot, const GPUBuffer& buffer, GPUSize64 offset = 0, GPUSize64 size = 0) const;

        void draw(GPUSize32 vertex_count, GPUSize32 instance_count = 1, GPUSize32 first_vertex = 0, GPUSize32 first_instance = 0) const;

        void draw_indexed(GPUSize32 index_count, GPUSize32 instance_count = 1, GPUSize32 first_index = 0, GPUSignedOffset32 base_vertex = 0, GPUSize32 first_instance = 0) const;

        void draw_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const;

        void draw_indexed_indirect(const GPUBuffer& indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count) const;

        void begin_render_pass(const GPURenderPassDescriptor& descriptor) const;

        void end_render_pass() const;

        void build_tlases(const GPUBuffer& scratch_buffer, GPUTlasBuildEntries entries) const;

        void build_blases(const GPUBuffer& scratch_buffer, GPUBlasBuildEntries entries) const;

        void copy_buffer_to_buffer(const GPUBuffer& source, const GPUBuffer& destination, GPUSize64 size) const;

        void copy_buffer_to_buffer(const GPUBuffer& source, GPUSize64 source_offset, const GPUBuffer& destination, GPUSize64 destination_offset, GPUSize64 size) const;

        void copy_buffer_to_texture(const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const;

        void copy_texture_to_buffer(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size) const;

        void copy_texture_to_texture(const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size) const;

        void clear_buffer(const GPUBuffer& buffer, GPUSize64 offset = 0, GPUSize64 size = 0) const;

        void set_viewport(float x, float y, float w, float h, float min_depth = 0.0f, float max_depth = 1.0f) const;

        void set_scissor_rect(GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h) const;

        void set_blend_constant(GPUColor color) const;

        void set_stencil_reference(GPUStencilValue reference) const;

        void begin_occlusion_query(GPUSize32 query_index) const;

        void end_occlusion_query() const;

        void write_timestamp(const GPUQuerySet& query_set, GPUSize32 query_index) const;

        void write_blas_properties(const GPUQuerySet& query_set, GPUSize32 query_index, const GPUBlas& blas) const;

        void resolve_query_set(GPUQuerySet query_set, GPUSize32 first_query, GPUSize32 query_count, const GPUBuffer& destination, GPUSize64 destination_offset) const;

        void resource_barrier(GPUBufferBarrier barrier) const;

        void resource_barrier(GPUTextureBarrier barrier) const;

        void resource_barrier(const Vector<GPUBufferBarrier>& barriers) const;

        void resource_barrier(const Vector<GPUTextureBarrier>& barriers) const;
    };

    struct GPUCommandBundle : public GPUCommandEncoder
    {
        // implicit conversion
        FORCE_INLINE GPUCommandBundle() : GPUCommandEncoder() {}
        FORCE_INLINE GPUCommandBundle(GPUCommandEncoderHandle handle) : GPUCommandEncoder(handle) {}
        FORCE_INLINE operator GPUCommandEncoderHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }
    };

    struct GPUCommandBuffer : public GPUCommandEncoder
    {
        // implicit conversion
        FORCE_INLINE GPUCommandBuffer() : GPUCommandEncoder() {}
        FORCE_INLINE GPUCommandBuffer(GPUCommandEncoderHandle handle) : GPUCommandEncoder(handle) {}
        FORCE_INLINE operator GPUCommandEncoderHandle() const { return handle; }

        FORCE_INLINE bool valid() const { return handle.valid(); }

        void submit() const;

        void execute_bundles(const Vector<GPUCommandBundle>& bundles) const;
    };

    struct GPUSurfaceTexture : public GPUObjectBase
    {
        GPUSurfaceHandle     surface;
        GPUTextureHandle     texture;
        GPUTextureViewHandle view;
        GPUFenceHandle       complete;
        GPUFenceHandle       available;
        bool                 suboptimal;

        // implicit conversion
        FORCE_INLINE operator GPUTextureViewHandle() const { return view; }

        void present() const;
    };

    struct GPUDevice : public GPUObjectBase
    {
        GPUAdapterInfo       adapter_info = {};
        GPUSupportedFeatures features     = {};

        auto create_fence() const -> GPUFence;

        auto create_buffer(const GPUBufferDescriptor& descriptor) const -> GPUBuffer;

        auto create_texture(const GPUTextureDescriptor& descriptor) const -> GPUTexture;

        auto create_sampler(const GPUSamplerDescriptor& descriptor) const -> GPUSampler;

        auto create_shader_module(const GPUShaderModuleDescriptor& descriptor) const -> GPUShaderModule;

        auto create_query_set(const GPUQuerySetDescriptor& descriptor) const -> GPUQuerySet;

        auto create_blas(const GPUBlasDescriptor& descriptor, const Vector<GPUBlasGeometrySizeDescriptor>& sizes) const -> GPUBlas;

        auto create_tlas(const GPUTlasDescriptor& descriptor) const -> GPUTlas;

        auto create_bind_group(const GPUBindGroupDescriptor& descriptor) const -> GPUBindGroup;

        auto create_bind_group_heap(const GPUBindGroupHeapDescriptor& descriptor) const -> GPUBindGroupHeap;

        auto create_bind_group_layout(const GPUBindGroupLayoutDescriptor& descriptor) const -> GPUBindGroupLayout;

        auto create_pipeline_layout(const GPUPipelineLayoutDescriptor& descriptor) const -> GPUPipelineLayout;

        auto create_render_pipeline(const GPURenderPipelineDescriptor& descriptor) const -> GPURenderPipeline;

        auto create_compute_pipeline(const GPUComputePipelineDescriptor& descriptor) const -> GPUComputePipeline;

        auto create_raytracing_pipeline(const GPURayTracingPipelineDescriptor& descriptor) const -> GPURayTracingPipeline;

        auto create_command_buffer(const GPUCommandBufferDescriptor& descriptor) const -> GPUCommandBuffer;

        auto create_command_bundle(const GPUCommandBundleDescriptor& descriptor) const -> GPUCommandBundle;

        auto get_blas_sizes(GPUBlasHandle blas) const -> GPUBVHSizes;

        auto get_tlas_sizes(GPUTlasHandle tlas) const -> GPUBVHSizes;

        auto wait() const -> void;

        auto wait(GPUFence fence) const -> void;

        auto destroy() const -> void;
    };

    struct GPUSurface : public GPUObjectBase
    {
        GPUSurfaceHandle handle;

        // implicit conversion
        FORCE_INLINE GPUSurface() : handle() {}
        FORCE_INLINE GPUSurface(GPUSurfaceHandle handle) : handle(handle) {}
        FORCE_INLINE operator GPUSurfaceHandle() const { return handle; }

        auto get_current_texture() const -> GPUSurfaceTexture;

        auto get_current_format() const -> GPUTextureFormat;

        auto get_current_extent() const -> GPUExtent2D;

        uint get_image_count() const;

        auto destroy() const -> void;
    };

    struct GPUAdapter : public GPUObjectBase
    {
        GPUAdapterInfo       info       = {};
        GPUSupportedFeatures features   = {};
        GPUSupportedLimits   limits     = {};
        GPUProperties        properties = {};

        auto request_device(const GPUDeviceDescriptor& descriptor) -> GPUDevice;
    };

    struct RHI
    {
        RHIFlags     flags = 0;
        RHIBackend   backend;
        WindowHandle window = {};

        static auto get_current_adapter() -> GPUAdapter&;

        static auto get_current_device() -> GPUDevice&;

        static auto init(const RHIDescriptor& descriptor) -> OwnedResource<RHI>;

        static auto api() -> RenderAPI*;

        static void wait();

        static void new_frame();

        static void end_frame();

        auto destroy() const -> void;

        auto request_adapter(const GPUAdapterDescriptor& descriptor = {}) const -> GPUAdapter;

        auto request_surface(const GPUSurfaceDescriptor& descriptor = {}) const -> GPUSurface;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_RHI_TYPES_H
