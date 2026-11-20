#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTCOMMANDBUFFER_H
#define LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTCOMMANDBUFFER_H

#include "SoftCommon.h"
#include "SoftDevice.h"
#include "SoftRasterizer.h"
#include "SoftRaytracing.h"

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
        Vector<uint32_t>   dynamic_offsets;
    };

    struct ExecutionContext
    {
        SoftViewport         viewport{};
        SoftScissor          scissor{};
        SoftRenderPipeline*  render_pipeline  = nullptr;
        SoftComputePipeline* compute_pipeline = nullptr;

        HashMap<uint32_t, SoftBoundBindGroup>    bind_groups;
        HashMap<uint32_t, SoftBoundVertexBuffer> vertex_buffers;
        SoftBoundIndexBuffer                     index_buffer;
        Vector<uint8_t>                          immediates_data;
        uint32_t                                 stencil_reference = 0;

        Vector<SoftTexture*> color_attachments;
        SoftTexture*         color_attachment         = nullptr;
        SoftTexture*         depth_stencil_attachment = nullptr;
    };

    class SoftCommandBuffer
    {
    public:
        SoftCommandBuffer() = default;

        auto reset() -> void;

        // recording API
        auto begin_render_pass(const GPURenderPassDescriptor& desc) -> void;
        auto end_render_pass() -> void;

        auto set_viewport(float x, float y, float w, float h, float min_d, float max_d) -> void;
        auto set_scissor(int32_t x, int32_t y, int32_t w, int32_t h) -> void;

        auto set_render_pipeline(GPURenderPipelineHandle pipeline) -> void;
        auto set_compute_pipeline(GPUComputePipelineHandle pipeline) -> void;
        auto set_raytracing_pipeline(GPURayTracingPipelineHandle pipeline) -> void;

        auto set_bind_group(uint32_t index, GPUBindGroupHandle bind_group, TypedView<uint32_t> dynamic_offsets = {}) -> void;
        auto set_stencil_reference(uint32_t reference) -> void;
        auto set_immediates(uint32_t offset, uint32_t size, const void* data) -> void;
        auto set_vertex_buffer(uint32_t slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size) -> void;
        auto set_index_buffer(GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size) -> void;

        auto draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) -> void;
        auto draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) -> void;

        auto dispatch_workgroups(uint32_t x, uint32_t y, uint32_t z) -> void;

        auto copy_buffer_to_buffer(GPUBufferHandle src, GPUSize64 src_off, GPUBufferHandle dst, GPUSize64 dst_off, GPUSize64 size) -> void;
        auto copy_buffer_to_texture(const GPUTexelCopyBufferInfo& src, const GPUTexelCopyTextureInfo& dst, GPUExtent3D copy_size) -> void;
        auto copy_texture_to_buffer(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyBufferInfo& dst, const GPUExtent3D& copy_size) -> void;
        auto copy_texture_to_texture(const GPUTexelCopyTextureInfo& src, const GPUTexelCopyTextureInfo& dst, const GPUExtent3D& copy_size) -> void;

        auto clear_buffer(GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size) -> void;
        auto clear_texture(GPUTextureHandle texture, const GPUTextureSubresourceRange& range) -> void;

        auto build_blases(GPUBufferHandle scratch, GPUBlasBuildEntries entries) -> void;
        auto build_tlases(GPUBufferHandle scratch, GPUTlasBuildEntries entries) -> void;

        auto execute_commands(SoftDevice& device) -> void;

    private:
        Vector<Function<void(SoftDevice&, ExecutionContext&)>> commands;
    };
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTWARE_SOFTCOMMANDBUFFER_H
