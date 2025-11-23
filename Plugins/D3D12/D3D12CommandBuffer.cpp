// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

void D3D12CommandBuffer::wait(const D3D12Fence& fence, GPUBarrierSyncFlags)
{
    // NOTE: D3D12 does not support fine-grain control at pipeline stage level,
    // therefore sync flags is not used.

    // fence target value cannot never be smaller than 0,
    // therefore waiting for fence target == 0 will always be satisfied.
    // This is to avoid D3D12 debug layer warning.
    if (fence.target == 0)
        return;

    wait_fences.push_back(FenceOps{
        fence.fence,
        fence.target,
    });
}

void D3D12CommandBuffer::signal(const D3D12Fence& fence, GPUBarrierSyncFlags)
{
    // NOTE: D3D12 does not support fine-grain control at pipeline stage level,
    // therefore sync flags is not used.

    signal_fences.push_back(FenceOps{
        fence.fence,
        fence.target,
    });
}

void D3D12CommandBuffer::submit()
{
    // GPU wait - make the command queue wait for the fence (current value)
    for (auto& fence : wait_fences)
        command_queue->Wait(fence.fence, fence.value);

    // submit the command list to the queue
    ID3D12CommandList* command_lists[] = {command_buffer};
    command_queue->ExecuteCommandLists(1, command_lists);

    // signal from GPU - command queue will signal when it reaches this point
    for (auto& fence : signal_fences)
        command_queue->Signal(fence.fence, fence.value);
}

void D3D12CommandBuffer::reset()
{
    // clear fences
    wait_fences.clear();
    signal_fences.clear();

    // clear bound pso
    pso.pipeline = nullptr;

    // reset command buffer before it can be used again
    command_buffer->Reset(command_allocator, nullptr);
}

void D3D12CommandBuffer::destroy()
{
    command_buffer->Release();
    command_buffer = nullptr;
}

void D3D12CommandBuffer::begin()
{
    // nothing here
}

void D3D12CommandBuffer::end()
{
    ThrowIfFailed(command_buffer->Close());
}

void cmd::insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label)
{
#if 0 // FIXME: Use PIXSetMarker instead
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.command_buffer->SetMarker(0x007fffff, marker_label, sizeof(marker_label));
#endif
}

void cmd::push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label)
{
#if 0 // FIXME: Use PIXBeginEvent instead
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.command_buffer->BeginEvent(0xff7f00ff, group_label, sizeof(group_label));
#endif
}

void cmd::pop_debug_group(GPUCommandEncoderHandle cmdbuffer)
{
#if 0 // FIXME: Use PIXEndEvent instead
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.command_buffer->EndEvent();
#endif
}

void cmd::wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
{
    auto  rhi = get_rhi();
    auto& fen = fetch_resource(rhi->fences, fence);
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.wait(fen, sync);
}

void cmd::signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
{
    auto  rhi = get_rhi();
    auto& fen = fetch_resource(rhi->fences, fence);
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.signal(fen, sync);
}

void cmd::begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor)
{
    assert(!descriptor.color_attachments.empty());

    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    UINT max_render_targets = std::min((uint)D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, (uint)descriptor.color_attachments.size());
    UINT num_render_targets = 0;
    bool has_depth_stencil  = descriptor.depth_stencil_attachment.view.valid();

    // set up render pass parameters
    D3D12_RENDER_PASS_RENDER_TARGET_DESC rt_descs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    for (uint i = 0; i < max_render_targets; i++) {
        auto& attachment = descriptor.color_attachments[i];
        auto& rt_desc    = rt_descs[num_render_targets];

        // set CPU descriptor handle for the RTV
        rt_desc.cpuDescriptor = fetch_resource(rhi->views, attachment.view).rtv_view.handle;

        // set beginning access
        if (attachment.load_op == GPULoadOp::CLEAR) {
            rt_desc.BeginningAccess.Type                      = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
            rt_desc.BeginningAccess.Clear.ClearValue.Color[0] = attachment.clear_value.r;
            rt_desc.BeginningAccess.Clear.ClearValue.Color[1] = attachment.clear_value.g;
            rt_desc.BeginningAccess.Clear.ClearValue.Color[2] = attachment.clear_value.b;
            rt_desc.BeginningAccess.Clear.ClearValue.Color[3] = attachment.clear_value.a;
        } else if (attachment.load_op == GPULoadOp::LOAD) {
            rt_desc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        } else { // GPULoadOp::DONT_CARE
            rt_desc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        }

        // set ending access
        if (attachment.store_op == GPUStoreOp::STORE) {
            rt_desc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        } else {
            rt_desc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        }

        num_render_targets++;
    }

    // set up depth stencil parameters
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC ds_desc = {};
    if (has_depth_stencil) {
        auto view = fetch_resource(rhi->views, descriptor.depth_stencil_attachment.view).dsv_view.handle;

        // set CPU descriptor handle for the DSV
        ds_desc.cpuDescriptor = view;

        // set depth beginning access
        if (descriptor.depth_stencil_attachment.depth_load_op == GPULoadOp::CLEAR) {
            ds_desc.DepthBeginningAccess.Type                                = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
            ds_desc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = descriptor.depth_stencil_attachment.depth_clear_value;
        } else if (descriptor.depth_stencil_attachment.depth_load_op == GPULoadOp::LOAD) {
            ds_desc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        } else {
            ds_desc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        }

        // set stencil beginning access
        if (descriptor.depth_stencil_attachment.stencil_load_op == GPULoadOp::CLEAR) {
            ds_desc.StencilBeginningAccess.Type                                  = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
            ds_desc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = descriptor.depth_stencil_attachment.stencil_clear_value;
        } else if (descriptor.depth_stencil_attachment.stencil_load_op == GPULoadOp::LOAD) {
            ds_desc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        } else {
            ds_desc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        }

        // set depth ending access
        if (descriptor.depth_stencil_attachment.depth_store_op == GPUStoreOp::STORE) {
            ds_desc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        } else {
            ds_desc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        }

        // set stencil ending access
        if (descriptor.depth_stencil_attachment.stencil_store_op == GPUStoreOp::STORE) {
            ds_desc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        } else {
            ds_desc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        }
    }

    // NOTE: Use default viewport and scissor

    // begin the render pass
    // D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES - if you need UAV writes during render pass
    // D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS - for tiled rendering scenarios
    // D3D12_RENDER_PASS_FLAG_RESUMING_PASS - for resuming suspended passes
    D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;

    ID3D12GraphicsCommandList4* command_list = static_cast<ID3D12GraphicsCommandList4*>(cmd.command_buffer);
    command_list->BeginRenderPass(
        num_render_targets,
        num_render_targets > 0 ? rt_descs : nullptr,
        has_depth_stencil ? &ds_desc : nullptr,
        flags);
}

void cmd::end_render_pass(GPUCommandEncoderHandle cmdbuffer)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    ID3D12GraphicsCommandList4* command_list = static_cast<ID3D12GraphicsCommandList4*>(cmd.command_buffer);
    command_list->EndRenderPass();
}

void cmd::set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (cmd.pso.pipeline != &pip) {
        cmd.pso.compute  = false;
        cmd.pso.pipeline = &pip;
        cmd.pso.layout   = &fetch_resource(rhi->pipeline_layouts, pip.layout);
        cmd.command_buffer->SetGraphicsRootSignature(cmd.pso.layout->layout);
        cmd.command_buffer->SetPipelineState(pip.pipeline);
        cmd.command_buffer->IASetPrimitiveTopology(pip.topology);
    }
}

void cmd::set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (cmd.pso.pipeline != &pip) {
        cmd.pso.compute  = true;
        cmd.pso.pipeline = &pip;
        cmd.pso.layout   = &fetch_resource(rhi->pipeline_layouts, pip.layout);
        cmd.command_buffer->SetComputeRootSignature(cmd.pso.layout->layout);
        cmd.command_buffer->SetPipelineState(pip.pipeline);
    }
}

void cmd::set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (cmd.pso.pipeline != &pip) {
        cmd.pso.compute  = false;
        cmd.pso.pipeline = &pip;
        cmd.pso.layout   = &fetch_resource(rhi->pipeline_layouts, pip.layout);
        cmd.command_buffer->SetComputeRootSignature(cmd.pso.layout->layout);
        cmd.command_buffer->SetPipelineState(pip.pipeline);
    }
}

void cmd::set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets dynamic_offsets)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto  des = astype<D3D12BindGroup*>(bind_group);

    const auto& info = cmd.pso.layout->bindgroups.at(index);
    if (info.has_default_root_parameter()) {
        auto handle = rhi->gpu_default_heap.gpu(des->default_index);
        if (cmd.pso.compute) {
            cmd.command_buffer->SetComputeRootDescriptorTable(info.default_root_parameter, handle);
        } else {
            cmd.command_buffer->SetGraphicsRootDescriptorTable(info.default_root_parameter, handle);
        }
    }
    if (info.has_sampler_root_parameter()) {
        auto handle = rhi->gpu_sampler_heap.gpu(des->sampler_index);
        if (cmd.pso.compute) {
            cmd.command_buffer->SetComputeRootDescriptorTable(info.sampler_root_parameter, handle);
        } else {
            cmd.command_buffer->SetGraphicsRootDescriptorTable(info.sampler_root_parameter, handle);
        }
    }

    if (dynamic_offsets.empty()) return;
    assert(des->dynamic_index != std::numeric_limits<uint16_t>::max());
    assert(info.has_dynamic_root_parameter());
    for (uint i = 0; i < dynamic_offsets.size(); i++) {
        auto& heap    = fetch_resource(rhi->bind_group_heaps, GPUBindGroupHeapHandle(des->heap_index));
        auto& dynamic = heap.dynamic_heap.at(des->dynamic_index + i);
        auto  address = dynamic.address + dynamic_offsets.at(i);
        if (dynamic.type == D3D12_ROOT_PARAMETER_TYPE_CBV) {
            if (cmd.pso.compute) {
                cmd.command_buffer->SetComputeRootConstantBufferView(info.dynamic_root_parameter + i, address);
            } else {
                cmd.command_buffer->SetGraphicsRootConstantBufferView(info.dynamic_root_parameter + i, address);
            }
        } else {
            if (cmd.pso.compute) {
                cmd.command_buffer->SetComputeRootUnorderedAccessView(info.dynamic_root_parameter + i, address);
            } else {
                cmd.command_buffer->SetGraphicsRootUnorderedAccessView(info.dynamic_root_parameter + i, address);
            }
        }
    }
}

void cmd::set_push_constants(GPUCommandEncoderHandle cmdbuffer, GPUShaderStageFlags visibility, uint offset, uint size, void* data)
{
    // we are not using this.
    (void)visibility;

    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    uint root_parameter = cmd.pso.layout->push_constant_root_parameter;
    assert(root_parameter != -1 && "The currently bound pipeline does not support push constants!");

    uint size_in_32b   = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    uint offset_in_32b = (offset + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    if (cmd.pso.compute) {
        cmd.command_buffer->SetComputeRoot32BitConstants(root_parameter, size_in_32b, data, offset_in_32b);
    } else {
        cmd.command_buffer->SetGraphicsRoot32BitConstants(root_parameter, size_in_32b, data, offset_in_32b);
    }
}

void cmd::set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);

    // create the index buffer view
    D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
    index_buffer_view.BufferLocation          = buf.buffer->GetGPUVirtualAddress() + offset;
    index_buffer_view.SizeInBytes             = static_cast<uint>(size == 0ull ? buf.size() : size);
    index_buffer_view.Format                  = d3d12enum(format);

    // set the index buffer
    cmd.command_buffer->IASetIndexBuffer(&index_buffer_view);
}

void cmd::set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);
    auto* pip = cmd.pso.pipeline;

    // create vertex buffer view
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
    vertex_buffer_view.BufferLocation           = buf.buffer->GetGPUVirtualAddress() + offset;
    vertex_buffer_view.SizeInBytes              = static_cast<uint>(size == 0ull ? buf.size() : size);
    vertex_buffer_view.StrideInBytes            = pip->vertex_buffer_strides.at(slot);

    // set the vertex buffer at the specified slot
    cmd.command_buffer->IASetVertexBuffers(slot, 1, &vertex_buffer_view);
}

void cmd::draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // draw instanced
    cmd.command_buffer->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void cmd::draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // draw indexed
    cmd.command_buffer->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
}

void cmd::draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    auto* lay = cmd.pso.layout;

    // indirect draw instanced
    lay->create_draw_indirect_signature();
    cmd.command_buffer->ExecuteIndirect(lay->signatures.draw_indirect.Get(), draw_count, buf.buffer, indirect_offset, nullptr, 0);
}

void cmd::draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    auto* lay = cmd.pso.layout;

    // indirect draw indexed
    lay->create_draw_indexed_indirect_signature();
    cmd.command_buffer->ExecuteIndirect(lay->signatures.draw_indexed_indirect.Get(), draw_count, buf.buffer, indirect_offset, nullptr, 0);
}

void cmd::dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // dispatch
    cmd.command_buffer->Dispatch(x, y, z);
}

void cmd::dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    auto* lay = cmd.pso.layout;

    // dispatch indirect
    lay->create_dispatch_indirect_signature();
    cmd.command_buffer->ExecuteIndirect(lay->signatures.dispatch_indirect.Get(), 1, buf.buffer, indirect_offset, nullptr, 0);
}

void cmd::copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& src = fetch_resource(rhi->buffers, source);
    auto& dst = fetch_resource(rhi->buffers, destination);

    // copy buffer to buffer
    cmd.command_buffer->CopyBufferRegion(dst.buffer, destination_offset, src.buffer, source_offset, size);
}

void cmd::copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& src = fetch_resource(rhi->buffers, source.buffer);
    auto& dst = fetch_resource(rhi->textures, destination.texture);

    // setup texture copy location for source (buffer)
    D3D12_TEXTURE_COPY_LOCATION src_location        = {};
    src_location.pResource                          = src.buffer;
    src_location.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_location.PlacedFootprint.Offset             = source.offset;
    src_location.PlacedFootprint.Footprint.Format   = dst.format;
    src_location.PlacedFootprint.Footprint.Width    = copy_size.width;
    src_location.PlacedFootprint.Footprint.Height   = copy_size.height;
    src_location.PlacedFootprint.Footprint.Depth    = copy_size.depth;
    src_location.PlacedFootprint.Footprint.RowPitch = infer_row_pitch(dst.format, dst.area.width, source.bytes_per_row);

    // setup texture copy location for destination (texture)
    D3D12_TEXTURE_COPY_LOCATION dst_location = {};
    dst_location.pResource                   = dst.texture;
    dst_location.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_location.SubresourceIndex            = destination.mip_level;

    // setup box for the copy region
    D3D12_BOX src_box = {};
    src_box.left      = 0;
    src_box.top       = 0;
    src_box.front     = 0;
    src_box.right     = copy_size.width;
    src_box.bottom    = copy_size.height;
    src_box.back      = copy_size.depth;

    // copy buffer to texture
    cmd.command_buffer->CopyTextureRegion(&dst_location, destination.origin.x, destination.origin.y, destination.origin.z, &src_location, &src_box);
}

void cmd::copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& src = fetch_resource(rhi->textures, source.texture);
    auto& dst = fetch_resource(rhi->buffers, destination.buffer);

    // setup texture copy location for source (texture)
    D3D12_TEXTURE_COPY_LOCATION src_location = {};
    src_location.pResource                   = src.texture;
    src_location.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_location.SubresourceIndex            = source.mip_level;

    // setup texture copy location for destination (buffer)
    D3D12_TEXTURE_COPY_LOCATION dst_location        = {};
    dst_location.pResource                          = dst.buffer;
    dst_location.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_location.PlacedFootprint.Offset             = destination.offset;
    dst_location.PlacedFootprint.Footprint.Format   = src.format;
    dst_location.PlacedFootprint.Footprint.Width    = copy_size.width;
    dst_location.PlacedFootprint.Footprint.Height   = copy_size.height;
    dst_location.PlacedFootprint.Footprint.Depth    = copy_size.depth;
    dst_location.PlacedFootprint.Footprint.RowPitch = infer_row_pitch(src.format, src.area.width, destination.bytes_per_row);

    // setup box for the copy region
    D3D12_BOX src_box = {};
    src_box.left      = source.origin.x;
    src_box.top       = source.origin.y;
    src_box.front     = source.origin.z;
    src_box.right     = source.origin.x + copy_size.width;
    src_box.bottom    = source.origin.y + copy_size.height;
    src_box.back      = source.origin.z + copy_size.depth;

    // copy texture to buffer
    cmd.command_buffer->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, &src_box);
}

void cmd::copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& src = fetch_resource(rhi->textures, source.texture);
    auto& dst = fetch_resource(rhi->textures, destination.texture);

    // setup texture copy location for source (texture)
    D3D12_TEXTURE_COPY_LOCATION src_location = {};
    src_location.pResource                   = src.texture;
    src_location.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_location.SubresourceIndex            = source.mip_level;

    // setup texture copy location for destination (texture)
    D3D12_TEXTURE_COPY_LOCATION dst_location = {};
    dst_location.pResource                   = dst.texture;
    dst_location.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_location.SubresourceIndex            = destination.mip_level;

    // setup box for the copy region
    D3D12_BOX src_box = {};
    src_box.left      = source.origin.x;
    src_box.top       = source.origin.y;
    src_box.front     = source.origin.z;
    src_box.right     = source.origin.x + copy_size.width;
    src_box.bottom    = source.origin.y + copy_size.height;
    src_box.back      = source.origin.z + copy_size.depth;

    // copy texture to texture
    cmd.command_buffer->CopyTextureRegion(&dst_location, destination.origin.x, destination.origin.y, destination.origin.z, &src_location, &src_box);
}

void cmd::clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);

    // for small buffers or patterns
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER write_params{};
    write_params.Dest  = buf.buffer->GetGPUVirtualAddress() + offset;
    write_params.Value = 0;

    D3D12_WRITEBUFFERIMMEDIATE_MODE write_mode = D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;

    // fill buffer
    auto command_list = static_cast<ID3D12GraphicsCommandList2*>(cmd.command_buffer);
    command_list->WriteBufferImmediate(1, &write_params, &write_mode);
}

void cmd::set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // setup viewport
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX       = x;
    viewport.TopLeftY       = y;
    viewport.Width          = w;
    viewport.Height         = h;
    viewport.MinDepth       = min_depth;
    viewport.MaxDepth       = max_depth;

    // set the viewport on the command list
    cmd.command_buffer->RSSetViewports(1, &viewport);
}

void cmd::set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // setup d3d12 scissor rect
    D3D12_RECT scissor_rect = {};
    scissor_rect.left       = x;
    scissor_rect.top        = y;
    scissor_rect.right      = x + w;
    scissor_rect.bottom     = y + h;

    // set the scissor rect on the command list
    cmd.command_buffer->RSSetScissorRects(1, &scissor_rect);
}

void cmd::set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    float blend_color[4] = {
        color.r,
        color.g,
        color.b,
        color.a,
    };

    // set the stencil reference value
    cmd.command_buffer->OMSetBlendFactor(blend_color);
}

void cmd::set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // set the stencil reference value
    cmd.command_buffer->OMSetStencilRef(reference);
}

void cmd::begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query_index)
{
    // NOTE: No obvious way to support it now.
    assert(!!!"cmd::begin_occlusion_query(...) is currently not implemented!");
}

void cmd::end_occlusion_query(GPUCommandEncoderHandle cmdbuffer)
{
    // NOTE: No obvious way to support it now.
    assert(!!!"cmd::end_occlusion_query(...) is currently not implemented!");
}

void cmd::write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index)
{
    // NOTE: No obvious way to support it now.
    assert(!!!"cmd::write_timestamp(...) is currently not implemented!");
}

void cmd::write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index, GPUBlasHandle blas)
{
    // NOTE: No obvious way to support it now.
    assert(!!!"cmd::write_blas_properties(...) is currently not implemented!");
}

void cmd::resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination, GPUSize64 destination_offset)
{
    // NOTE: No obvious way to support it now.
    assert(!!!"cmd::resolve_query_set(...) is currently not implemented!");
}

void cmd::memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // for memory/alias barriers, use global uav barrier
    // this ensures all uav accesses complete before proceeding
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource          = nullptr; // global barrier for all resources

    // issue a single global uav barrier
    // webgpu memory barriers map to global uav barriers in d3d12
    cmd.command_buffer->ResourceBarrier(1, &barrier);

    // note: count and barriers array are not used since d3d12 global barriers
    // don't have the same granularity as webgpu memory barriers
    // multiple webgpu barriers collapse to a single global d3d12 barrier
}

void cmd::buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // allocate array for d3d12 resource barriers
    Vector<D3D12_BUFFER_BARRIER> d3d12_barriers;
    d3d12_barriers.reserve(barriers.size());

    for (auto& barrier : barriers) {
        d3d12_barriers.emplace_back();

        auto& d3d12_barrier = d3d12_barriers.back();

        // setup enhanced buffer barrier
        d3d12_barrier.SyncBefore   = d3d12enum(barrier.src_sync);
        d3d12_barrier.SyncAfter    = d3d12enum(barrier.dst_sync);
        d3d12_barrier.AccessBefore = d3d12enum(barrier.src_access);
        d3d12_barrier.AccessAfter  = d3d12enum(barrier.dst_access);
        d3d12_barrier.pResource    = fetch_resource(rhi->buffers, barrier.buffer).buffer;
        d3d12_barrier.Offset       = barrier.offset;
        d3d12_barrier.Size         = barrier.size;
    }

    D3D12_BARRIER_GROUP barrier_group = {};
    barrier_group.Type                = D3D12_BARRIER_TYPE_BUFFER;
    barrier_group.NumBarriers         = static_cast<uint>(barriers.size());
    barrier_group.pBufferBarriers     = d3d12_barriers.data();

    // issue the resource barriers
    auto command_list = static_cast<ID3D12GraphicsCommandList7*>(cmd.command_buffer);
    command_list->Barrier(1, &barrier_group);
}

void cmd::texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(cmdbuffer);

    // allocate array for d3d12 resource barriers
    Vector<D3D12_TEXTURE_BARRIER> d3d12_barriers;
    d3d12_barriers.reserve(barriers.size());

    for (auto& barrier : barriers) {
        d3d12_barriers.emplace_back();

        auto& d3d12_barrier = d3d12_barriers.back();

        // setup enhanced buffer barrier
        d3d12_barrier.SyncBefore                        = d3d12enum(barrier.src_sync);
        d3d12_barrier.SyncAfter                         = d3d12enum(barrier.dst_sync);
        d3d12_barrier.AccessBefore                      = d3d12enum(barrier.src_access);
        d3d12_barrier.AccessAfter                       = d3d12enum(barrier.dst_access);
        d3d12_barrier.pResource                         = fetch_resource(rhi->textures, barrier.texture).texture;
        d3d12_barrier.LayoutBefore                      = d3d12enum(barrier.src_layout);
        d3d12_barrier.LayoutAfter                       = d3d12enum(barrier.dst_layout);
        d3d12_barrier.Subresources.FirstArraySlice      = barrier.subresources.base_array_layer;
        d3d12_barrier.Subresources.NumArraySlices       = barrier.subresources.array_layers;
        d3d12_barrier.Subresources.IndexOrFirstMipLevel = barrier.subresources.base_mip_level;
        d3d12_barrier.Subresources.NumMipLevels         = barrier.subresources.mip_level_count;
        d3d12_barrier.Subresources.FirstPlane           = 0;
        d3d12_barrier.Subresources.NumPlanes            = 1;
    }

    D3D12_BARRIER_GROUP barrier_group = {};
    barrier_group.Type                = D3D12_BARRIER_TYPE_TEXTURE;
    barrier_group.NumBarriers         = static_cast<uint>(barriers.size());
    barrier_group.pTextureBarriers    = d3d12_barriers.data();

    // issue the resource barriers
    auto command_list = static_cast<ID3D12GraphicsCommandList7*>(cmd.command_buffer);
    command_list->Barrier(1, &barrier_group);
}

void cmd::build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries)
{
    assert(!!!"cmd::build_tlases(...) is currently not implemented!");
}

void cmd::build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries)
{
    assert(!!!"cmd::build_blases(...) is currently not implemented!");
}
