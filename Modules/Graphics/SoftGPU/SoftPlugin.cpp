#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Compiler/SLCUtils.h>

#include "SoftCommon.h"
#include "SoftDevice.h"
#include "SoftCommandBuffer.h"

using namespace lyra;
using namespace lyra::softgpu;

namespace
{
    struct CommandBufferStorage
    {
        SoftCommandBuffer buffer;
    };

    HashMap<uint64_t, CommandBufferStorage> g_command_buffers;
    std::atomic<uint64_t>                   g_cmd_counter{1};

    auto get_cmd_buffer(GPUCommandEncoderHandle handle) -> SoftCommandBuffer&
    {
        return g_command_buffers[handle.value].buffer;
    }
} // namespace

namespace api
{
    auto get_api_name() -> CString
    {
        return "SoftGPU";
    }

    bool create_instance(const RHIDescriptor& desc)
    {
        get_logger()->trace("create_instance()");
        return get_device()->init(desc);
    }

    void delete_instance()
    {
        get_logger()->trace("delete_instance()");
        get_device()->destroy();
        g_command_buffers.clear();
    }

    bool create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor&)
    {
        adapter = get_device()->adapter_props;
        return true;
    }

    void delete_adapter()
    {
    }

    bool create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor& desc)
    {
        auto* dev  = get_device();
        auto  swap = SoftSwapchain(desc, dev->textures, dev->texture_views, dev->fences);
        auto  ind  = dev->swapchains.add(swap);
        surface    = GPUSurfaceHandle::create(ind);
        return true;
    }

    void delete_surface(GPUSurfaceHandle surface)
    {
        auto* dev    = get_device();
        auto  handle = surface.to_slotmap_handle<SoftSwapchain>();
        auto* swap   = dev->swapchains.find(handle);
        if (swap) {
            swap->destroy(dev->textures, dev->texture_views, dev->fences);
            dev->swapchains.remove(handle);
        }
    }

    bool get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent)
    {
        auto* dev  = get_device();
        auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
        if (!swap) return false;
        extent = swap->extent;
        return true;
    }

    bool get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format)
    {
        auto* dev  = get_device();
        auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
        if (!swap) return false;
        format = swap->format;
        return true;
    }

    uint get_surface_frames(GPUSurfaceHandle surface)
    {
        auto* dev  = get_device();
        auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
        if (!swap) return 0;
        return static_cast<uint>(swap->frames.size());
    }

    bool create_device(const GPUDeviceDescriptor&)
    {
        return true;
    }

    void delete_device()
    {
    }

    bool create_fence(GPUFenceHandle& fence)
    {
        auto ind = get_device()->fences.add(SoftFence());
        fence    = GPUFenceHandle::create(ind);
        return true;
    }

    void delete_fence(GPUFenceHandle fence)
    {
        get_device()->fences.remove(fence.to_slotmap_handle<SoftFence>());
    }

    bool create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc)
    {
        auto ind = get_device()->buffers.add(SoftBuffer(desc));
        buffer   = GPUBufferHandle::create(ind);
        return true;
    }

    void delete_buffer(GPUBufferHandle buffer)
    {
        auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b) {
            b->destroy();
            get_device()->buffers.remove(buffer.to_slotmap_handle<SoftBuffer>());
        }
    }

    bool create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc)
    {
        auto ind = get_device()->samplers.add(SoftSampler(desc));
        sampler  = GPUSamplerHandle::create(ind);
        return true;
    }

    void delete_sampler(GPUSamplerHandle sampler)
    {
        get_device()->samplers.remove(sampler.to_slotmap_handle<SoftSampler>());
    }

    bool create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc)
    {
        auto ind = get_device()->textures.add(SoftTexture(desc));
        texture  = GPUTextureHandle::create(ind);
        return true;
    }

    void delete_texture(GPUTextureHandle texture)
    {
        auto* t = get_device()->textures.find(texture.to_slotmap_handle<SoftTexture>());
        if (t) {
            t->destroy();
            get_device()->textures.remove(texture.to_slotmap_handle<SoftTexture>());
        }
    }

    bool create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc)
    {
        auto* dev = get_device();
        auto* tex = dev->textures.find(texture.to_slotmap_handle<SoftTexture>());
        if (!tex) return false;
        auto ind = dev->texture_views.add(SoftTextureView(texture, desc, *tex));
        view     = GPUTextureViewHandle::create(ind);
        return true;
    }

    void delete_texture_view(GPUTextureViewHandle view)
    {
        get_device()->texture_views.remove(view.to_slotmap_handle<SoftTextureView>());
    }

    bool create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc)
    {
        SoftShaderModule sm{};
        sm.label = desc.label;
        if (desc.data && desc.size > 0) {
            sm.bytecode.assign(desc.data, desc.data + desc.size);
            if (desc.size == sizeof(HostCallableShader)) {
                const auto* hcs = reinterpret_cast<const HostCallableShader*>(desc.data);
                if (hcs->magic == HOST_MAGIC) {
                    sm.host_library = hcs->library;
                    sm.host_func    = hcs->entry_func;
                    sm.param_count  = hcs->param_count;
                    std::memcpy(sm.params, hcs->params, sizeof(hcs->params));
                }
            }
        }
        auto ind = get_device()->shader_modules.add(sm);
        shader   = GPUShaderModuleHandle::create(ind);
        return true;
    }

    void delete_shader_module(GPUShaderModuleHandle shader)
    {
        get_device()->shader_modules.remove(shader.to_slotmap_handle<SoftShaderModule>());
    }

    bool create_query_set(GPUQuerySetHandle& query, const GPUQuerySetDescriptor& desc)
    {
        SoftQuerySet qs{};
        qs.type  = desc.type;
        qs.count = desc.count;
        qs.results.resize(desc.count, 0);
        qs.label = desc.label;
        auto ind = get_device()->query_sets.add(qs);
        query    = GPUQuerySetHandle::create(ind);
        return true;
    }

    void delete_query_set(GPUQuerySetHandle query)
    {
        get_device()->query_sets.remove(query.to_slotmap_handle<SoftQuerySet>());
    }

    bool create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors)
    {
        auto ind = get_device()->blases.add(SoftBlas(desc));
        blas     = GPUBlasHandle::create(ind);
        return true;
    }

    void delete_blas(GPUBlasHandle blas)
    {
        get_device()->blases.remove(blas.to_slotmap_handle<SoftBlas>());
    }

    bool create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& desc)
    {
        auto ind = get_device()->tlases.add(SoftTlas(desc));
        tlas     = GPUTlasHandle::create(ind);
        return true;
    }

    void delete_tlas(GPUTlasHandle tlas)
    {
        get_device()->tlases.remove(tlas.to_slotmap_handle<SoftTlas>());
    }

    bool create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc)
    {
        auto ind = get_device()->pipeline_layouts.add(SoftPipelineLayout(desc));
        layout   = GPUPipelineLayoutHandle::create(ind);
        return true;
    }

    void delete_pipeline_layout(GPUPipelineLayoutHandle layout)
    {
        get_device()->pipeline_layouts.remove(layout.to_slotmap_handle<SoftPipelineLayout>());
    }

    bool create_render_pipeline(GPURenderPipelineHandle& pipeline, const GPURenderPipelineDescriptor& desc)
    {
        auto ind = get_device()->render_pipelines.add(SoftRenderPipeline(desc));
        pipeline = GPURenderPipelineHandle::create(ind);
        return true;
    }

    void delete_render_pipeline(GPURenderPipelineHandle pipeline)
    {
        get_device()->render_pipelines.remove(pipeline.to_slotmap_handle<SoftRenderPipeline>());
    }

    bool create_compute_pipeline(GPUComputePipelineHandle& pipeline, const GPUComputePipelineDescriptor& desc)
    {
        auto ind = get_device()->compute_pipelines.add(SoftComputePipeline(desc));
        pipeline = GPUComputePipelineHandle::create(ind);
        return true;
    }

    void delete_compute_pipeline(GPUComputePipelineHandle pipeline)
    {
        get_device()->compute_pipelines.remove(pipeline.to_slotmap_handle<SoftComputePipeline>());
    }

    bool create_raytracing_pipeline(GPURayTracingPipelineHandle& pipeline, const GPURayTracingPipelineDescriptor& desc)
    {
        auto ind = get_device()->raytracing_pipelines.add(SoftRayTracingPipeline(desc));
        pipeline = GPURayTracingPipelineHandle::create(ind);
        return true;
    }

    void delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline)
    {
        get_device()->raytracing_pipelines.remove(pipeline.to_slotmap_handle<SoftRayTracingPipeline>());
    }

    bool create_bind_group(GPUBindGroupHandle& bg, const GPUBindGroupDescriptor& desc)
    {
        auto ind = get_device()->bind_groups.add(SoftBindGroup(desc));
        bg       = GPUBindGroupHandle::create(ind);
        return true;
    }

    bool create_bind_group_layout(GPUBindGroupLayoutHandle& layout, const GPUBindGroupLayoutDescriptor& desc)
    {
        auto ind = get_device()->bind_group_layouts.add(SoftBindGroupLayout(desc));
        layout   = GPUBindGroupLayoutHandle::create(ind);
        return true;
    }

    void delete_bind_group_layout(GPUBindGroupLayoutHandle layout)
    {
        get_device()->bind_group_layouts.remove(layout.to_slotmap_handle<SoftBindGroupLayout>());
    }

    bool create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor&)
    {
        static std::atomic<uint64_t> s_heap_counter{1};
        heap = GPUBindGroupHeapHandle(s_heap_counter.fetch_add(1));
        return true;
    }

    void delete_bind_group_heap(GPUBindGroupHeapHandle)
    {
    }

    void reset_bind_group_heap(GPUBindGroupHeapHandle)
    {
    }

    void new_frame()
    {
    }

    void end_frame()
    {
    }

    bool acquire_next_frame(GPUSurfaceHandle surface, GPUTextureHandle& texture, GPUTextureViewHandle& view,
        GPUFenceHandle& available, GPUFenceHandle& complete, bool& suboptimal)
    {
        auto* dev  = get_device();
        auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
        if (!swap) return false;
        return swap->acquire_next_frame(texture, view, available, complete, suboptimal);
    }

    bool present_curr_frame(GPUSurfaceHandle surface)
    {
        auto* dev  = get_device();
        auto* swap = dev->swapchains.find(surface.to_slotmap_handle<SoftSwapchain>());
        if (!swap) return false;
        return swap->present(dev->textures);
    }

    void get_mapped_state(GPUBufferHandle buffer, GPUMapState& state)
    {
        auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b)
            state = b->map_state;
        else
            state = GPUMapState::UNMAPPED;
    }

    void get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range)
    {
        auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b)
            range = b->get_mapped_range();
        else
            range = MappedBufferRange{};
    }

    void map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size)
    {
        auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b) b->map(mode, offset, size);
    }

    void unmap_buffer(GPUBufferHandle buffer)
    {
        auto* b = get_device()->buffers.find(buffer.to_slotmap_handle<SoftBuffer>());
        if (b) b->unmap();
    }

    void wait_idle()
    {
    }

    void wait_fence(GPUFenceHandle fence)
    {
        auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
        if (f) f->wait();
    }

    bool poll_fence(GPUFenceHandle fence)
    {
        auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
        if (f) return f->poll();
        return true;
    }

    bool get_blas_sizes(GPUBlasHandle, GPUBVHSizes& sizes)
    {
        sizes.bvh_size    = 1024 * 1024;
        sizes.build_size  = 1024 * 1024;
        sizes.update_size = 1024 * 1024;
        return true;
    }

    bool get_tlas_sizes(GPUTlasHandle, GPUBVHSizes& sizes)
    {
        sizes.bvh_size    = 1024 * 1024;
        sizes.build_size  = 1024 * 1024;
        sizes.update_size = 1024 * 1024;
        return true;
    }

    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor&)
    {
        uint64_t id = g_cmd_counter.fetch_add(1);
        cmdbuffer   = GPUCommandEncoderHandle(id);
        g_command_buffers[id].buffer.reset();
        return true;
    }

    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor&)
    {
        uint64_t id = g_cmd_counter.fetch_add(1);
        cmdbuffer   = GPUCommandEncoderHandle(id);
        g_command_buffers[id].buffer.reset();
        return true;
    }

    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer)
    {
        auto it = g_command_buffers.find(cmdbuffer.value);
        if (it != g_command_buffers.end()) {
            it->second.buffer.execute_commands(*get_device());
            it->second.buffer.reset();
        }
        return true;
    }

    void cmd_insert_debug_marker(GPUCommandEncoderHandle, CString) {}
    void cmd_push_debug_group(GPUCommandEncoderHandle, CString) {}
    void cmd_pop_debug_group(GPUCommandEncoderHandle) {}
    void cmd_wait_fence(GPUCommandEncoderHandle, GPUFenceHandle fence, GPUBarrierSyncFlags)
    {
        wait_fence(fence);
    }
    void cmd_signal_fence(GPUCommandEncoderHandle, GPUFenceHandle fence, GPUBarrierSyncFlags)
    {
        auto* f = get_device()->fences.find(fence.to_slotmap_handle<SoftFence>());
        if (f) f->signal();
    }

    void cmd_begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor)
    {
        get_cmd_buffer(cmdbuffer).begin_render_pass(descriptor);
    }

    void cmd_end_render_pass(GPUCommandEncoderHandle cmdbuffer)
    {
        get_cmd_buffer(cmdbuffer).end_render_pass();
    }

    void cmd_set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
    {
        get_cmd_buffer(cmdbuffer).set_render_pipeline(pipeline);
    }

    void cmd_set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
    {
        get_cmd_buffer(cmdbuffer).set_compute_pipeline(pipeline);
    }

    void cmd_set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
    {
        get_cmd_buffer(cmdbuffer).set_raytracing_pipeline(pipeline);
    }

    void cmd_set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets offsets)
    {
        get_cmd_buffer(cmdbuffer).set_bind_group(index, bind_group, offsets);
    }

    void cmd_set_immediates(GPUCommandEncoderHandle cmdbuffer, uint offset, uint size, const void* data)
    {
        get_cmd_buffer(cmdbuffer).set_immediates(offset, size, data);
    }

    void cmd_set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
    {
        get_cmd_buffer(cmdbuffer).set_index_buffer(buffer, format, offset, size);
    }

    void cmd_set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
    {
        get_cmd_buffer(cmdbuffer).set_vertex_buffer(slot, buffer, offset, size);
    }

    void cmd_draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
    {
        get_cmd_buffer(cmdbuffer).draw(vertex_count, instance_count, first_vertex, first_instance);
    }

    void cmd_draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
    {
        get_cmd_buffer(cmdbuffer).draw_indexed(index_count, instance_count, first_index, base_vertex, first_instance);
    }

    void cmd_draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        get_cmd_buffer(cmdbuffer).draw_indirect(indirect_buffer, indirect_offset, draw_count);
    }

    void cmd_draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        get_cmd_buffer(cmdbuffer).draw_indexed_indirect(indirect_buffer, indirect_offset, draw_count);
    }

    void cmd_dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
    {
        get_cmd_buffer(cmdbuffer).dispatch_workgroups(x, y, z);
    }

    void cmd_dispatch_workgroups_indirect(GPUCommandEncoderHandle, GPUBufferHandle, GPUSize64) {}

    void cmd_copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size)
    {
        get_cmd_buffer(cmdbuffer).copy_buffer_to_buffer(source, source_offset, destination, destination_offset, size);
    }

    void cmd_copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size)
    {
        get_cmd_buffer(cmdbuffer).copy_buffer_to_texture(source, destination, copy_size);
    }

    void cmd_copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size)
    {
        get_cmd_buffer(cmdbuffer).copy_texture_to_buffer(source, destination, copy_size);
    }

    void cmd_copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size)
    {
        get_cmd_buffer(cmdbuffer).copy_texture_to_texture(source, destination, copy_size);
    }

    void cmd_clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
    {
        get_cmd_buffer(cmdbuffer).clear_buffer(buffer, offset, size);
    }

    void cmd_clear_texture(GPUCommandEncoderHandle cmdbuffer, GPUTextureHandle texture, const GPUTextureSubresourceRange& range)
    {
        get_cmd_buffer(cmdbuffer).clear_texture(texture, range);
    }

    void cmd_set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
    {
        get_cmd_buffer(cmdbuffer).set_viewport(x, y, w, h, min_depth, max_depth);
    }

    void cmd_set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
    {
        get_cmd_buffer(cmdbuffer).set_scissor(x, y, w, h);
    }

    void cmd_set_blend_constant(GPUCommandEncoderHandle, GPUColor) {}
    void cmd_set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
    {
        get_cmd_buffer(cmdbuffer).set_stencil_reference(reference);
    }
    void cmd_begin_occlusion_query(GPUCommandEncoderHandle, GPUSize32) {}
    void cmd_end_occlusion_query(GPUCommandEncoderHandle) {}
    void cmd_write_timestamp(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32) {}
    void cmd_write_blas_properties(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32, GPUBlasHandle) {}
    void cmd_resolve_query_set(GPUCommandEncoderHandle, GPUQuerySetHandle, GPUSize32, GPUSize32, GPUBufferHandle, GPUSize64) {}

    void cmd_memory_barrier(GPUCommandEncoderHandle, GPUMemoryBarriers) {}
    void cmd_buffer_barrier(GPUCommandEncoderHandle, GPUBufferBarriers) {}
    void cmd_texture_barrier(GPUCommandEncoderHandle, GPUTextureBarriers) {}

    void cmd_build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries)
    {
        get_cmd_buffer(cmdbuffer).build_tlases(scratch_buffer, entries);
    }

    void cmd_build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries)
    {
        get_cmd_buffer(cmdbuffer).build_blases(scratch_buffer, entries);
    }

    void cmd_copy_blas(GPUCommandEncoderHandle, GPUBlasHandle, GPUBlasHandle) {}
} // namespace api

LYRA_EXPORT auto prepare() -> void
{
    // prepare software backend
}

LYRA_EXPORT auto cleanup() -> void
{
    // cleanup software backend
}

LYRA_EXPORT auto create() -> RenderAPI
{
    auto api                             = RenderAPI{};
    api.get_api_name                     = api::get_api_name;
    api.create_instance                  = api::create_instance;
    api.delete_instance                  = api::delete_instance;
    api.create_adapter                   = api::create_adapter;
    api.delete_adapter                   = api::delete_adapter;
    api.create_device                    = api::create_device;
    api.delete_device                    = api::delete_device;
    api.create_surface                   = api::create_surface;
    api.delete_surface                   = api::delete_surface;
    api.get_surface_extent               = api::get_surface_extent;
    api.get_surface_format               = api::get_surface_format;
    api.get_surface_frames               = api::get_surface_frames;
    api.create_buffer                    = api::create_buffer;
    api.delete_buffer                    = api::delete_buffer;
    api.create_texture                   = api::create_texture;
    api.delete_texture                   = api::delete_texture;
    api.create_texture_view              = api::create_texture_view;
    api.delete_texture_view              = api::delete_texture_view;
    api.create_sampler                   = api::create_sampler;
    api.delete_sampler                   = api::delete_sampler;
    api.create_fence                     = api::create_fence;
    api.delete_fence                     = api::delete_fence;
    api.create_shader_module             = api::create_shader_module;
    api.delete_shader_module             = api::delete_shader_module;
    api.create_blas                      = api::create_blas;
    api.delete_blas                      = api::delete_blas;
    api.create_tlas                      = api::create_tlas;
    api.delete_tlas                      = api::delete_tlas;
    api.create_pipeline_layout           = api::create_pipeline_layout;
    api.delete_pipeline_layout           = api::delete_pipeline_layout;
    api.create_render_pipeline           = api::create_render_pipeline;
    api.delete_render_pipeline           = api::delete_render_pipeline;
    api.create_compute_pipeline          = api::create_compute_pipeline;
    api.delete_compute_pipeline          = api::delete_compute_pipeline;
    api.create_raytracing_pipeline       = api::create_raytracing_pipeline;
    api.delete_raytracing_pipeline       = api::delete_raytracing_pipeline;
    api.create_bind_group                = api::create_bind_group;
    api.create_bind_group_layout         = api::create_bind_group_layout;
    api.delete_bind_group_layout         = api::delete_bind_group_layout;
    api.create_bind_group_heap           = api::create_bind_group_heap;
    api.delete_bind_group_heap           = api::delete_bind_group_heap;
    api.reset_bind_group_heap            = api::reset_bind_group_heap;
    api.wait_idle                        = api::wait_idle;
    api.wait_fence                       = api::wait_fence;
    api.poll_fence                       = api::poll_fence;
    api.new_frame                        = api::new_frame;
    api.end_frame                        = api::end_frame;
    api.map_buffer                       = api::map_buffer;
    api.unmap_buffer                     = api::unmap_buffer;
    api.get_mapped_state                 = api::get_mapped_state;
    api.get_mapped_range                 = api::get_mapped_range;
    api.create_command_buffer            = api::create_command_buffer;
    api.create_command_bundle            = api::create_command_bundle;
    api.submit_command_buffer            = api::submit_command_buffer;
    api.get_blas_sizes                   = api::get_blas_sizes;
    api.get_tlas_sizes                   = api::get_tlas_sizes;
    api.acquire_next_frame               = api::acquire_next_frame;
    api.present_curr_frame               = api::present_curr_frame;
    api.cmd_insert_debug_marker          = api::cmd_insert_debug_marker;
    api.cmd_push_debug_group             = api::cmd_push_debug_group;
    api.cmd_pop_debug_group              = api::cmd_pop_debug_group;
    api.cmd_wait_fence                   = api::cmd_wait_fence;
    api.cmd_signal_fence                 = api::cmd_signal_fence;
    api.cmd_begin_render_pass            = api::cmd_begin_render_pass;
    api.cmd_end_render_pass              = api::cmd_end_render_pass;
    api.cmd_set_render_pipeline          = api::cmd_set_render_pipeline;
    api.cmd_set_compute_pipeline         = api::cmd_set_compute_pipeline;
    api.cmd_set_raytracing_pipeline      = api::cmd_set_raytracing_pipeline;
    api.cmd_set_bind_group               = api::cmd_set_bind_group;
    api.cmd_set_immediates               = api::cmd_set_immediates;
    api.cmd_set_index_buffer             = api::cmd_set_index_buffer;
    api.cmd_set_vertex_buffer            = api::cmd_set_vertex_buffer;
    api.cmd_draw                         = api::cmd_draw;
    api.cmd_draw_indexed                 = api::cmd_draw_indexed;
    api.cmd_draw_indirect                = api::cmd_draw_indirect;
    api.cmd_draw_indexed_indirect        = api::cmd_draw_indexed_indirect;
    api.cmd_dispatch_workgroups          = api::cmd_dispatch_workgroups;
    api.cmd_dispatch_workgroups_indirect = api::cmd_dispatch_workgroups_indirect;
    api.cmd_copy_buffer_to_buffer        = api::cmd_copy_buffer_to_buffer;
    api.cmd_copy_buffer_to_texture       = api::cmd_copy_buffer_to_texture;
    api.cmd_copy_texture_to_buffer       = api::cmd_copy_texture_to_buffer;
    api.cmd_copy_texture_to_texture      = api::cmd_copy_texture_to_texture;
    api.cmd_clear_buffer                 = api::cmd_clear_buffer;
    api.cmd_clear_texture                = api::cmd_clear_texture;
    api.cmd_set_viewport                 = api::cmd_set_viewport;
    api.cmd_set_scissor_rect             = api::cmd_set_scissor_rect;
    api.cmd_set_blend_constant           = api::cmd_set_blend_constant;
    api.cmd_set_stencil_reference        = api::cmd_set_stencil_reference;
    api.cmd_begin_occlusion_query        = api::cmd_begin_occlusion_query;
    api.cmd_end_occlusion_query          = api::cmd_end_occlusion_query;
    api.cmd_write_timestamp              = api::cmd_write_timestamp;
    api.cmd_write_blas_properties        = api::cmd_write_blas_properties;
    api.cmd_resolve_query_set            = api::cmd_resolve_query_set;
    api.cmd_memory_barrier               = api::cmd_memory_barrier;
    api.cmd_buffer_barrier               = api::cmd_buffer_barrier;
    api.cmd_texture_barrier              = api::cmd_texture_barrier;
    api.cmd_build_tlases                 = api::cmd_build_tlases;
    api.cmd_build_blases                 = api::cmd_build_blases;
    api.cmd_copy_blas                    = api::cmd_copy_blas;
    return api;
}
