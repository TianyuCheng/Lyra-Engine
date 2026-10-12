#include <atomic>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>

using namespace lyra;

namespace
{
    std::atomic<uint64_t> g_handle_counter{1};

    template <typename T>
    auto make_mock_handle() -> T
    {
        uint64_t id = g_handle_counter.fetch_add(1);
        return T(id);
    }

    struct MockBufferStorage
    {
        GPUSize64       size = 0;
        Vector<uint8_t> memory;
        bool            mapped = false;
    };

    HashMap<uint64_t, MockBufferStorage> g_buffers;

    auto get_buffer_id(GPUBufferHandle handle) -> uint64_t
    {
        return handle.value;
    }

    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("Null", LogLevel::info);
        return LOGGER;
    }
} // namespace

namespace api
{
    auto get_api_name() -> CString
    {
        return "Null";
    }

    bool create_instance(const RHIDescriptor&)
    {
        get_logger()->trace("create_instance()");
        return true;
    }

    void delete_instance()
    {
        get_logger()->trace("delete_instance()");
    }

    bool create_adapter(GPUAdapterProps& props, const GPUAdapterDescriptor&)
    {
        get_logger()->trace("create_adapter()");
        props.info.description = "Null GPU Device";
        props.info.vendor      = "Lyra";
        props.info.device      = "Null";
        return true;
    }

    void delete_adapter()
    {
        get_logger()->trace("delete_adapter()");
    }

    bool create_surface(GPUSurfaceHandle& surface, const GPUSurfaceDescriptor&)
    {
        surface = make_mock_handle<GPUSurfaceHandle>();
        get_logger()->trace("create_surface() -> handle={}", surface.value);
        return true;
    }

    void delete_surface(GPUSurfaceHandle surface)
    {
        get_logger()->trace("delete_surface(handle={})", surface.value);
    }

    bool get_surface_extent(GPUSurfaceHandle surface, GPUExtent2D& extent)
    {
        extent.width  = 1280;
        extent.height = 720;
        get_logger()->trace("get_surface_extent(handle={}) -> 1280x720", surface.value);
        return true;
    }

    bool get_surface_format(GPUSurfaceHandle surface, GPUTextureFormat& format)
    {
        format = GPUTextureFormat::RGBA8UNORM;
        get_logger()->trace("get_surface_format(handle={}) -> RGBA8UNORM", surface.value);
        return true;
    }

    uint get_surface_frames(GPUSurfaceHandle surface)
    {
        get_logger()->trace("get_surface_frames(handle={}) -> 3", surface.value);
        return 3;
    }

    bool create_device(const GPUDeviceDescriptor& desc)
    {
        get_logger()->trace("create_device(label={})", desc.label ? desc.label : "none");
        return true;
    }

    void delete_device()
    {
        get_logger()->trace("delete_device()");
    }

    bool create_fence(GPUFenceHandle& fence)
    {
        fence = make_mock_handle<GPUFenceHandle>();
        get_logger()->trace("create_fence() -> handle={}", fence.value);
        return true;
    }

    void delete_fence(GPUFenceHandle fence)
    {
        get_logger()->trace("delete_fence(handle={})", fence.value);
    }

    bool create_buffer(GPUBufferHandle& buffer, const GPUBufferDescriptor& desc)
    {
        buffer      = make_mock_handle<GPUBufferHandle>();
        uint64_t id = get_buffer_id(buffer);

        MockBufferStorage storage;
        storage.size = desc.size;
        storage.memory.resize(desc.size, 0);
        storage.mapped = desc.mapped_at_creation;
        g_buffers[id]  = std::move(storage);

        get_logger()->trace("create_buffer(label={}, size={}, usage={:#x}) -> handle={}",
            desc.label ? desc.label : "none", desc.size, static_cast<uint>(desc.usage.value), buffer.value);
        return true;
    }

    void delete_buffer(GPUBufferHandle buffer)
    {
        uint64_t id = get_buffer_id(buffer);
        g_buffers.erase(id);
        get_logger()->trace("delete_buffer(handle={})", buffer.value);
    }

    bool create_sampler(GPUSamplerHandle& sampler, const GPUSamplerDescriptor& desc)
    {
        sampler = make_mock_handle<GPUSamplerHandle>();
        get_logger()->trace("create_sampler(label={}) -> handle={}",
            desc.label ? desc.label : "none", sampler.value);
        return true;
    }

    void delete_sampler(GPUSamplerHandle sampler)
    {
        get_logger()->trace("delete_sampler(handle={})", sampler.value);
    }

    bool create_texture(GPUTextureHandle& texture, const GPUTextureDescriptor& desc)
    {
        texture = make_mock_handle<GPUTextureHandle>();
        get_logger()->trace("create_texture(label={}, size={}x{}x{}) -> handle={}",
            desc.label ? desc.label : "none", desc.size.width, desc.size.height, desc.size.depth, texture.value);
        return true;
    }

    void delete_texture(GPUTextureHandle texture)
    {
        get_logger()->trace("delete_texture(handle={})", texture.value);
    }

    bool create_texture_view(GPUTextureViewHandle& view, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc)
    {
        view = make_mock_handle<GPUTextureViewHandle>();
        get_logger()->trace("create_texture_view(texture_handle={}, label={}) -> handle={}",
            texture.value, desc.label ? desc.label : "none", view.value);
        return true;
    }

    void delete_texture_view(GPUTextureViewHandle view)
    {
        get_logger()->trace("delete_texture_view(handle={})", view.value);
    }

    bool create_shader_module(GPUShaderModuleHandle& shader, const GPUShaderModuleDescriptor& desc)
    {
        shader = make_mock_handle<GPUShaderModuleHandle>();
        get_logger()->trace("create_shader_module(label={}) -> handle={}",
            desc.label ? desc.label : "none", shader.value);
        return true;
    }

    void delete_shader_module(GPUShaderModuleHandle shader)
    {
        get_logger()->trace("delete_shader_module(handle={})", shader.value);
    }

    bool create_query_set(GPUQuerySetHandle& query, const GPUQuerySetDescriptor& desc)
    {
        query = make_mock_handle<GPUQuerySetHandle>();
        get_logger()->trace("create_query_set(count={}) -> handle={}", desc.count, query.value);
        return true;
    }

    void delete_query_set(GPUQuerySetHandle query)
    {
        get_logger()->trace("delete_query_set(handle={})", query.value);
    }

    bool create_blas(GPUBlasHandle& blas, const GPUBlasDescriptor& desc, GPUBlasGeometrySizeDescriptors)
    {
        blas = make_mock_handle<GPUBlasHandle>();
        get_logger()->trace("create_blas(label={}) -> handle={}",
            desc.label ? desc.label : "none", blas.value);
        return true;
    }

    void delete_blas(GPUBlasHandle blas)
    {
        get_logger()->trace("delete_blas(handle={})", blas.value);
    }

    bool create_tlas(GPUTlasHandle& tlas, const GPUTlasDescriptor& desc)
    {
        tlas = make_mock_handle<GPUTlasHandle>();
        get_logger()->trace("create_tlas(label={}) -> handle={}",
            desc.label ? desc.label : "none", tlas.value);
        return true;
    }

    void delete_tlas(GPUTlasHandle tlas)
    {
        get_logger()->trace("delete_tlas(handle={})", tlas.value);
    }

    bool create_pipeline_layout(GPUPipelineLayoutHandle& layout, const GPUPipelineLayoutDescriptor& desc)
    {
        layout = make_mock_handle<GPUPipelineLayoutHandle>();
        get_logger()->trace("create_pipeline_layout(label={}) -> handle={}",
            desc.label ? desc.label : "none", layout.value);
        return true;
    }

    void delete_pipeline_layout(GPUPipelineLayoutHandle layout)
    {
        get_logger()->trace("delete_pipeline_layout(handle={})", layout.value);
    }

    bool create_render_pipeline(GPURenderPipelineHandle& pipeline, const GPURenderPipelineDescriptor& desc)
    {
        pipeline = make_mock_handle<GPURenderPipelineHandle>();
        get_logger()->trace("create_render_pipeline(label={}) -> handle={}",
            desc.label ? desc.label : "none", pipeline.value);
        return true;
    }

    void delete_render_pipeline(GPURenderPipelineHandle pipeline)
    {
        get_logger()->trace("delete_render_pipeline(handle={})", pipeline.value);
    }

    bool create_compute_pipeline(GPUComputePipelineHandle& pipeline, const GPUComputePipelineDescriptor& desc)
    {
        pipeline = make_mock_handle<GPUComputePipelineHandle>();
        get_logger()->trace("create_compute_pipeline(label={}) -> handle={}",
            desc.label ? desc.label : "none", pipeline.value);
        return true;
    }

    void delete_compute_pipeline(GPUComputePipelineHandle pipeline)
    {
        get_logger()->trace("delete_compute_pipeline(handle={})", pipeline.value);
    }

    bool create_raytracing_pipeline(GPURayTracingPipelineHandle& pipeline, const GPURayTracingPipelineDescriptor& desc)
    {
        pipeline = make_mock_handle<GPURayTracingPipelineHandle>();
        get_logger()->trace("create_raytracing_pipeline(label={}) -> handle={}",
            desc.label ? desc.label : "none", pipeline.value);
        return true;
    }

    void delete_raytracing_pipeline(GPURayTracingPipelineHandle pipeline)
    {
        get_logger()->trace("delete_raytracing_pipeline(handle={})", pipeline.value);
    }

    bool create_bind_group(GPUBindGroupHandle& group, const GPUBindGroupDescriptor& desc)
    {
        group = make_mock_handle<GPUBindGroupHandle>();
        get_logger()->trace("create_bind_group(label={}) -> handle={}",
            desc.label ? desc.label : "none", group.value);
        return true;
    }

    bool create_bind_group_layout(GPUBindGroupLayoutHandle& layout, const GPUBindGroupLayoutDescriptor& desc)
    {
        layout = make_mock_handle<GPUBindGroupLayoutHandle>();
        get_logger()->trace("create_bind_group_layout(label={}) -> handle={}",
            desc.label ? desc.label : "none", layout.value);
        return true;
    }

    void delete_bind_group_layout(GPUBindGroupLayoutHandle layout)
    {
        get_logger()->trace("delete_bind_group_layout(handle={})", layout.value);
    }

    bool create_bind_group_heap(GPUBindGroupHeapHandle& heap, const GPUBindGroupHeapDescriptor& desc)
    {
        heap = make_mock_handle<GPUBindGroupHeapHandle>();
        get_logger()->trace("create_bind_group_heap(label={}) -> handle={}",
            desc.label ? desc.label : "none", heap.value);
        return true;
    }

    void delete_bind_group_heap(GPUBindGroupHeapHandle heap)
    {
        get_logger()->trace("delete_bind_group_heap(handle={})", heap.value);
    }

    void reset_bind_group_heap(GPUBindGroupHeapHandle heap)
    {
        get_logger()->trace("reset_bind_group_heap(handle={})", heap.value);
    }

    void new_frame()
    {
        get_logger()->trace("new_frame()");
    }

    void end_frame()
    {
        get_logger()->trace("end_frame()");
    }

    bool acquire_next_frame(
        GPUSurfaceHandle      surface,
        GPUTextureHandle&     texture,
        GPUTextureViewHandle& view,
        GPUFenceHandle&,
        GPUFenceHandle&,
        bool& suboptimal)
    {
        texture    = make_mock_handle<GPUTextureHandle>();
        view       = make_mock_handle<GPUTextureViewHandle>();
        suboptimal = false;
        get_logger()->trace("acquire_next_frame(surface={})", surface.value);
        return true;
    }

    bool present_curr_frame(GPUSurfaceHandle surface)
    {
        get_logger()->trace("present_curr_frame(surface={})", surface.value);
        return true;
    }

    void get_mapped_state(GPUBufferHandle buffer, GPUMapState& state)
    {
        uint64_t id = get_buffer_id(buffer);
        auto     it = g_buffers.find(id);
        state       = (it != g_buffers.end() && it->second.mapped) ? GPUMapState::MAPPED : GPUMapState::UNMAPPED;
        get_logger()->trace("get_mapped_state(handle={})", buffer.value);
    }

    void get_mapped_range(GPUBufferHandle buffer, MappedBufferRange& range)
    {
        uint64_t id = get_buffer_id(buffer);
        auto     it = g_buffers.find(id);
        if (it != g_buffers.end() && !it->second.memory.empty()) {
            range.data = it->second.memory.data();
            range.size = it->second.memory.size();
        } else {
            range.data = nullptr;
            range.size = 0;
        }
        get_logger()->trace("get_mapped_range(handle={}, ptr={}, size={})",
            buffer.value, static_cast<const void*>(range.data), range.size);
    }

    void map_buffer(GPUBufferHandle buffer, GPUMapMode mode, GPUSize64 offset, GPUSize64 size)
    {
        uint64_t id = get_buffer_id(buffer);
        auto     it = g_buffers.find(id);
        if (it != g_buffers.end()) {
            it->second.mapped = true;
        }
        get_logger()->trace("map_buffer(handle={}, offset={}, size={})", buffer.value, offset, size);
    }

    void unmap_buffer(GPUBufferHandle buffer)
    {
        uint64_t id = get_buffer_id(buffer);
        auto     it = g_buffers.find(id);
        if (it != g_buffers.end()) {
            it->second.mapped = false;
        }
        get_logger()->trace("unmap_buffer(handle={})", buffer.value);
    }

    void wait_idle()
    {
        get_logger()->trace("wait_idle()");
    }

    void wait_fence(GPUFenceHandle fence)
    {
        get_logger()->trace("wait_fence(handle={})", fence.value);
    }

    bool get_blas_sizes(GPUBlasHandle blas, GPUBVHSizes& sizes)
    {
        sizes.bvh_size    = 1024;
        sizes.build_size  = 1024;
        sizes.update_size = 512;
        get_logger()->trace("get_blas_sizes(handle={})", blas.value);
        return true;
    }

    bool get_tlas_sizes(GPUTlasHandle tlas, GPUBVHSizes& sizes)
    {
        sizes.bvh_size    = 2048;
        sizes.build_size  = 2048;
        sizes.update_size = 1024;
        get_logger()->trace("get_tlas_sizes(handle={})", tlas.value);
        return true;
    }

    bool create_command_buffer(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBufferDescriptor& desc)
    {
        cmdbuffer = make_mock_handle<GPUCommandEncoderHandle>();
        get_logger()->trace("create_command_buffer(label={}) -> handle={}",
            desc.label ? desc.label : "none", cmdbuffer.value);
        return true;
    }

    bool create_command_bundle(GPUCommandEncoderHandle& cmdbuffer, const GPUCommandBundleDescriptor& desc)
    {
        cmdbuffer = make_mock_handle<GPUCommandEncoderHandle>();
        get_logger()->trace("create_command_bundle(label={}) -> handle={}",
            desc.label ? desc.label : "none", cmdbuffer.value);
        return true;
    }

    bool submit_command_buffer(GPUCommandEncoderHandle cmdbuffer)
    {
        get_logger()->trace("submit_command_buffer(handle={})", cmdbuffer.value);
        return true;
    }

    void cmd_insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label)
    {
        if (marker_label) {
            get_logger()->trace("cmd_insert_debug_marker(handle={}, label={})", cmdbuffer.value, marker_label);
        }
    }

    void cmd_push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label)
    {
        if (group_label) {
            get_logger()->trace("cmd_push_debug_group(handle={}, label={})", cmdbuffer.value, group_label);
        }
    }

    void cmd_pop_debug_group(GPUCommandEncoderHandle cmdbuffer)
    {
        get_logger()->trace("cmd_pop_debug_group(handle={})", cmdbuffer.value);
    }

    void cmd_wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
    {
        get_logger()->trace("cmd_wait_fence(handle={}, fence={})", cmdbuffer.value, fence.value);
    }

    void cmd_signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
    {
        get_logger()->trace("cmd_signal_fence(handle={}, fence={})", cmdbuffer.value, fence.value);
    }

    void cmd_begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& desc)
    {
        get_logger()->trace("cmd_begin_render_pass(handle={}, label={})",
            cmdbuffer.value, desc.label ? desc.label : "none");
    }

    void cmd_end_render_pass(GPUCommandEncoderHandle cmdbuffer)
    {
        get_logger()->trace("cmd_end_render_pass(handle={})", cmdbuffer.value);
    }

    void cmd_set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
    {
        get_logger()->trace("cmd_set_render_pipeline(handle={}, pipeline={})", cmdbuffer.value, pipeline.value);
    }

    void cmd_set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
    {
        get_logger()->trace("cmd_set_compute_pipeline(handle={}, pipeline={})", cmdbuffer.value, pipeline.value);
    }

    void cmd_set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
    {
        get_logger()->trace("cmd_set_raytracing_pipeline(handle={}, pipeline={})", cmdbuffer.value, pipeline.value);
    }

    void cmd_set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets dynamic_offsets)
    {
        get_logger()->trace("cmd_set_bind_group(handle={}, index={}, group={})",
            cmdbuffer.value, index, bind_group.value);
    }

    void cmd_set_immediates(GPUCommandEncoderHandle cmdbuffer, uint offset, uint size, const void* data)
    {
        get_logger()->trace("cmd_set_immediates(handle={}, offset={}, size={})", cmdbuffer.value, offset, size);
    }

    void cmd_set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
    {
        get_logger()->trace("cmd_set_index_buffer(handle={}, buffer={}, offset={}, size={})",
            cmdbuffer.value, buffer.value, offset, size);
    }

    void cmd_set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
    {
        get_logger()->trace("cmd_set_vertex_buffer(handle={}, slot={}, buffer={}, offset={}, size={})",
            cmdbuffer.value, slot, buffer.value, offset, size);
    }

    void cmd_draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
    {
        get_logger()->trace("cmd_draw(handle={}, vertices={}, instances={})", cmdbuffer.value, vertex_count, instance_count);
    }

    void cmd_draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
    {
        get_logger()->trace("cmd_draw_indexed(handle={}, indices={}, instances={})", cmdbuffer.value, index_count, instance_count);
    }

    void cmd_draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        get_logger()->trace("cmd_draw_indirect(handle={}, buffer={}, offset={})", cmdbuffer.value, indirect_buffer.value, indirect_offset);
    }

    void cmd_draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        get_logger()->trace("cmd_draw_indexed_indirect(handle={}, buffer={}, offset={})", cmdbuffer.value, indirect_buffer.value, indirect_offset);
    }

    void cmd_dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
    {
        get_logger()->trace("cmd_dispatch_workgroups(handle={}, x={}, y={}, z={})", cmdbuffer.value, x, y, z);
    }

    void cmd_dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
    {
        get_logger()->trace("cmd_dispatch_workgroups_indirect(handle={}, buffer={}, offset={})", cmdbuffer.value, indirect_buffer.value, indirect_offset);
    }

    void cmd_copy_buffer_to_buffer(
        GPUCommandEncoderHandle cmdbuffer,
        GPUBufferHandle         source,
        GPUSize64               source_offset,
        GPUBufferHandle         destination,
        GPUSize64               destination_offset,
        GPUSize64               size)
    {
        // perform actual memory copy between mock buffers if both exist
        uint64_t src_id = get_buffer_id(source);
        uint64_t dst_id = get_buffer_id(destination);

        auto src_it = g_buffers.find(src_id);
        auto dst_it = g_buffers.find(dst_id);
        if (src_it != g_buffers.end() && dst_it != g_buffers.end()) {
            if (source_offset + size <= src_it->second.memory.size() &&
                destination_offset + size <= dst_it->second.memory.size()) {
                std::memcpy(
                    dst_it->second.memory.data() + destination_offset,
                    src_it->second.memory.data() + source_offset,
                    size);
            }
        }

        get_logger()->trace("cmd_copy_buffer_to_buffer(handle={}, src={}, src_offset={}, dst={}, dst_offset={}, size={})",
            cmdbuffer.value, source.value, source_offset, destination.value, destination_offset, size);
    }

    void cmd_copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo&, const GPUTexelCopyTextureInfo&, GPUExtent3D)
    {
        get_logger()->trace("cmd_copy_buffer_to_texture(handle={})", cmdbuffer.value);
    }

    void cmd_copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo&, const GPUTexelCopyBufferInfo&, const GPUExtent3D&)
    {
        get_logger()->trace("cmd_copy_texture_to_buffer(handle={})", cmdbuffer.value);
    }

    void cmd_copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo&, const GPUTexelCopyTextureInfo&, const GPUExtent3D&)
    {
        get_logger()->trace("cmd_copy_texture_to_texture(handle={})", cmdbuffer.value);
    }

    void cmd_clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
    {
        get_logger()->trace("cmd_clear_buffer(handle={}, buffer={}, offset={}, size={})", cmdbuffer.value, buffer.value, offset, size);
    }

    void cmd_clear_texture(GPUCommandEncoderHandle cmdbuffer, GPUTextureHandle texture, const GPUTextureSubresourceRange&)
    {
        get_logger()->trace("cmd_clear_texture(handle={}, texture={})", cmdbuffer.value, texture.value);
    }

    void cmd_set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
    {
        get_logger()->trace("cmd_set_viewport(handle={}, x={}, y={}, w={}, h={})", cmdbuffer.value, x, y, w, h);
    }

    void cmd_set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
    {
        get_logger()->trace("cmd_set_scissor_rect(handle={}, x={}, y={}, w={}, h={})", cmdbuffer.value, x, y, w, h);
    }

    void cmd_set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color)
    {
        get_logger()->trace("cmd_set_blend_constant(handle={})", cmdbuffer.value);
    }

    void cmd_set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
    {
        get_logger()->trace("cmd_set_stencil_reference(handle={}, ref={})", cmdbuffer.value, reference);
    }

    void cmd_begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query_index)
    {
        get_logger()->trace("cmd_begin_occlusion_query(handle={}, index={})", cmdbuffer.value, query_index);
    }

    void cmd_end_occlusion_query(GPUCommandEncoderHandle cmdbuffer)
    {
        get_logger()->trace("cmd_end_occlusion_query(handle={})", cmdbuffer.value);
    }

    void cmd_write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index)
    {
        get_logger()->trace("cmd_write_timestamp(handle={}, set={}, index={})", cmdbuffer.value, query_set.value, query_index);
    }

    void cmd_write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index, GPUBlasHandle blas)
    {
        get_logger()->trace("cmd_write_blas_properties(handle={}, set={}, index={}, blas={})", cmdbuffer.value, query_set.value, query_index, blas.value);
    }

    void cmd_resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination, GPUSize64 destination_offset)
    {
        get_logger()->trace("cmd_resolve_query_set(handle={}, set={}, first={}, count={})", cmdbuffer.value, query_set.value, first_query, query_count);
    }

    void cmd_memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers)
    {
        get_logger()->trace("cmd_memory_barrier(handle={})", cmdbuffer.value);
    }

    void cmd_buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers)
    {
        get_logger()->trace("cmd_buffer_barrier(handle={})", cmdbuffer.value);
    }

    void cmd_texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers)
    {
        get_logger()->trace("cmd_texture_barrier(handle={})", cmdbuffer.value);
    }

    void cmd_build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries)
    {
        get_logger()->trace("cmd_build_tlases(handle={}, scratch={})", cmdbuffer.value, scratch_buffer.value);
    }

    void cmd_build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries)
    {
        get_logger()->trace("cmd_build_blases(handle={}, scratch={})", cmdbuffer.value, scratch_buffer.value);
    }

    void cmd_copy_blas(GPUCommandEncoderHandle cmdbuffer, GPUBlasHandle old_blas, GPUBlasHandle new_blas)
    {
        get_logger()->trace("cmd_copy_blas(handle={}, old={}, new={})", cmdbuffer.value, old_blas.value, new_blas.value);
    }
} // namespace api

namespace lyra::null
{
    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_NULL_VERBOSITY"));
    }

    void cleanup()
    {
        // do nothing
    }

    auto create() -> RenderAPI
    {
        RenderAPI api{};
        api.get_api_name                     = api::get_api_name;
        api.create_instance                  = api::create_instance;
        api.delete_instance                  = api::delete_instance;
        api.create_adapter                   = api::create_adapter;
        api.delete_adapter                   = api::delete_adapter;
        api.create_surface                   = api::create_surface;
        api.delete_surface                   = api::delete_surface;
        api.get_surface_extent               = api::get_surface_extent;
        api.get_surface_format               = api::get_surface_format;
        api.get_surface_frames               = api::get_surface_frames;
        api.create_device                    = api::create_device;
        api.delete_device                    = api::delete_device;
        api.create_fence                     = api::create_fence;
        api.delete_fence                     = api::delete_fence;
        api.create_buffer                    = api::create_buffer;
        api.delete_buffer                    = api::delete_buffer;
        api.create_sampler                   = api::create_sampler;
        api.delete_sampler                   = api::delete_sampler;
        api.create_texture                   = api::create_texture;
        api.delete_texture                   = api::delete_texture;
        api.create_texture_view              = api::create_texture_view;
        api.delete_texture_view              = api::delete_texture_view;
        api.create_shader_module             = api::create_shader_module;
        api.delete_shader_module             = api::delete_shader_module;
        api.create_query_set                 = api::create_query_set;
        api.delete_query_set                 = api::delete_query_set;
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
        api.create_bind_group_heap           = api::create_bind_group_heap;
        api.delete_bind_group_heap           = api::delete_bind_group_heap;
        api.reset_bind_group_heap            = api::reset_bind_group_heap;
        api.create_bind_group_layout         = api::create_bind_group_layout;
        api.delete_bind_group_layout         = api::delete_bind_group_layout;
        api.wait_idle                        = api::wait_idle;
        api.wait_fence                       = api::wait_fence;
        api.map_buffer                       = api::map_buffer;
        api.unmap_buffer                     = api::unmap_buffer;
        api.get_mapped_state                 = api::get_mapped_state;
        api.get_mapped_range                 = api::get_mapped_range;
        api.create_command_buffer            = api::create_command_buffer;
        api.create_command_bundle            = api::create_command_bundle;
        api.submit_command_buffer            = api::submit_command_buffer;
        api.get_blas_sizes                   = api::get_blas_sizes;
        api.get_tlas_sizes                   = api::get_tlas_sizes;
        api.new_frame                        = api::new_frame;
        api.end_frame                        = api::end_frame;
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

} // namespace lyra::null
