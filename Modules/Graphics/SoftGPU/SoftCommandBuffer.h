#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMANDBUFFER_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMANDBUFFER_H

#include "SoftCommon.h"
#include "SoftDevice.h"
#include "SoftRasterizer.h"
#include "SoftRaytracing.h"

using namespace lyra;

namespace lyra::softgpu
{
    struct SoftBoundVertexBuffer
    {
        GPUBufferHandle buffer;
        GPUSize64       offset = 0;
        GPUSize64       size   = 0;
    };

    struct SoftBoundIndexBuffer
    {
        GPUBufferHandle buffer;
        GPUIndexFormat  format = GPUIndexFormat::UINT32;
        GPUSize64       offset = 0;
        GPUSize64       size   = 0;
    };

    struct SoftBoundBindGroup
    {
        GPUBindGroupHandle handle;
        Vector<uint>       dynamic_offsets;
    };

    struct ExecutionContext
    {
        SoftViewport         viewport{};
        SoftScissor          scissor{};
        SoftRenderPipeline*  render_pipeline  = nullptr;
        SoftComputePipeline* compute_pipeline = nullptr;

        HashMap<uint, SoftBoundBindGroup>    bind_groups;
        HashMap<uint, SoftBoundVertexBuffer> vertex_buffers;
        SoftBoundIndexBuffer                 index_buffer;
        Vector<ubyte>                        immediates_data;
        uint                                 stencil_reference = 0;

        Vector<SoftTexture*> color_attachments;
        SoftTexture*         color_attachment         = nullptr;
        SoftTexture*         depth_stencil_attachment = nullptr;
    };

    class SoftCommandBuffer
    {
    public:
        SoftCommandBuffer() = default;

        void reset();

        // recording API
        void begin_render_pass(const GPURenderPassDescriptor& desc);
        void end_render_pass();

        void set_viewport(float x, float y, float w, float h, float min_d, float max_d);
        void set_scissor(int x, int y, int w, int h);

        void set_render_pipeline(GPURenderPipelineHandle pipeline);
        void set_compute_pipeline(GPUComputePipelineHandle pipeline);
        void set_raytracing_pipeline(GPURayTracingPipelineHandle pipeline);

        void set_bind_group(uint index, GPUBindGroupHandle bind_group, TypedView<uint> dynamic_offsets = {});
        void set_stencil_reference(uint reference);
        void set_immediates(uint offset, uint size, const void* data);
        void set_vertex_buffer(uint slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
        void set_index_buffer(GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size);

        void draw(uint vertex_count, uint instance_count, uint first_vertex, uint first_instance);
        void draw_indexed(uint index_count, uint instance_count, uint first_index, int base_vertex, uint first_instance);
        void draw_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);
        void draw_indexed_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);

        void dispatch_workgroups(uint x, uint y, uint z);
        void dispatch_workgroups_indirect(GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset);

        void copy_buffer_to_buffer(GPUBufferHandle src, GPUSize64 src_off, GPUBufferHandle dst, GPUSize64 dst_off, GPUSize64 size);
        void copy_buffer_to_texture(const GPUTexelCopyBufferInfo& src, const GPUTexelCopyTextureInfo& dst, GPUExtent3D copy_size);
        void copy_texture_to_buffer(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyBufferInfo& dst, const GPUExtent3D& copy_size);
        void copy_texture_to_texture(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyTextureInfo& dst, const GPUExtent3D& copy_size);

        void clear_buffer(GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
        void clear_texture(GPUTextureHandle texture, const GPUTextureSubresourceRange& range);

        void build_blases(GPUBufferHandle scratch, GPUBlasBuildEntries entries);
        void build_tlases(GPUBufferHandle scratch, GPUTlasBuildEntries entries);

        void execute_commands(SoftDevice& device);

    private:
        Vector<Function<void(SoftDevice&, ExecutionContext&)>> commands;
    };
} // namespace lyra::softgpu

namespace cmd
{
    using namespace lyra;

    void insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker);
    void push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group);
    void pop_debug_group(GPUCommandEncoderHandle cmdbuffer);
    void wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync);
    void signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync);
    void begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor);
    void end_render_pass(GPUCommandEncoderHandle cmdbuffer);
    void set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline);
    void set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline);
    void set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline);
    void set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets offsets);
    void set_immediates(GPUCommandEncoderHandle cmdbuffer, uint offset, uint size, const void* data);
    void set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size);
    void set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
    void draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance);
    void draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance);
    void draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);
    void draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count);
    void dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z);
    void dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset);
    void copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size);
    void copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size);
    void copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size);
    void copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size);
    void clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size);
    void clear_texture(GPUCommandEncoderHandle cmdbuffer, GPUTextureHandle texture, const GPUTextureSubresourceRange& range);
    void set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth);
    void set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h);
    void set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color);
    void set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference);
    void begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query);
    void end_occlusion_query(GPUCommandEncoderHandle cmdbuffer);
    void write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index);
    void write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index, GPUBlasHandle blas);
    void resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination, GPUSize64 destination_offset);
    void memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers barriers);
    void buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers barriers);
    void texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers barriers);
    void build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries);
    void build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries);
    void copy_blas(GPUCommandEncoderHandle cmdbuffer, GPUBlasHandle source, GPUBlasHandle destination);
} // namespace cmd

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMANDBUFFER_H
