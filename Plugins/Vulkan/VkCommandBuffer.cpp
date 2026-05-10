#include <limits>
#include <algorithm>
#include "VkUtils.h"

void VulkanCommandBuffer::wait(const VulkanSemaphore& fence, GPUBarrierSyncFlags sync)
{
    wait_semaphores.push_back(VkSemaphoreSubmitInfo{});

    auto& submit_info       = wait_semaphores.back();
    submit_info.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submit_info.pNext       = nullptr;
    submit_info.semaphore   = fence.semaphore;
    submit_info.value       = fence.type == VK_SEMAPHORE_TYPE_BINARY ? 0 : fence.target;
    submit_info.stageMask   = vkenum(sync);
    submit_info.deviceIndex = 0;
}

void VulkanCommandBuffer::signal(const VulkanSemaphore& fence, GPUBarrierSyncFlags sync)
{
    auto rhi = get_rhi();
    signal_semaphores.push_back(VkSemaphoreSubmitInfo{});

    auto& submit_info       = signal_semaphores.back();
    submit_info.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submit_info.pNext       = nullptr;
    submit_info.semaphore   = fence.semaphore;
    submit_info.value       = fence.type == VK_SEMAPHORE_TYPE_BINARY ? 0 : fence.target;
    submit_info.stageMask   = vkenum(sync);
    submit_info.deviceIndex = 0;

    // attach the inflight fence if this command buffer signals render complete semaphore
    auto& frame    = rhi->current_frame();
    auto& complete = fetch_resource(rhi->fences, frame.render_complete_semaphore);
    if (fence.semaphore == complete.semaphore)
        this->fence = frame.inflight_fence;
}

void VulkanCommandBuffer::reset()
{
    for (auto& buffer : temporary_buffers)
        buffer.destroy();

    temporary_buffers.clear();
}

void VulkanCommandBuffer::submit()
{
    auto rhi = get_rhi();

    auto cmd_submit_info          = VkCommandBufferSubmitInfo{};
    cmd_submit_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmd_submit_info.pNext         = nullptr;
    cmd_submit_info.commandBuffer = command_buffer;
    cmd_submit_info.deviceMask    = 0;

    auto submit_info                     = VkSubmitInfo2{};
    submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.pNext                    = nullptr;
    submit_info.flags                    = 0;
    submit_info.commandBufferInfoCount   = 1;
    submit_info.pCommandBufferInfos      = &cmd_submit_info;
    submit_info.waitSemaphoreInfoCount   = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphoreInfos      = wait_semaphores.data();
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphoreInfos    = signal_semaphores.data();

    vk_check(rhi->vtable.vkQueueSubmit2KHR(command_queue, 1, &submit_info, fence.fence));
}

void VulkanCommandBuffer::begin()
{
    auto rhi = get_rhi();

    auto begin_info             = VkCommandBufferBeginInfo{};
    begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext            = nullptr;
    begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    vk_check(rhi->vtable.vkBeginCommandBuffer(command_buffer, &begin_info));
}

void VulkanCommandBuffer::end()
{
    auto rhi = get_rhi();
    vk_check(rhi->vtable.vkEndCommandBuffer(command_buffer));
}

void cmd::insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label)
{
    if (vkCmdInsertDebugUtilsLabelEXT) {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        auto label_info       = VkDebugUtilsLabelEXT{};
        label_info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        label_info.pLabelName = marker_label;
        label_info.color[0]   = 0.0f;
        label_info.color[1]   = 0.5f;
        label_info.color[2]   = 1.0f;
        label_info.color[3]   = 1.0f;
        vkCmdInsertDebugUtilsLabelEXT(cmd.command_buffer, &label_info);
    }
}

void cmd::push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label)
{
    if (vkCmdBeginDebugUtilsLabelEXT) {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        auto label_info       = VkDebugUtilsLabelEXT{};
        label_info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        label_info.pLabelName = group_label;
        label_info.color[0]   = 1.0f;
        label_info.color[1]   = 0.5f;
        label_info.color[2]   = 0.0f;
        label_info.color[3]   = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cmd.command_buffer, &label_info);
    }
}

void cmd::pop_debug_group(GPUCommandEncoderHandle cmdbuffer)
{
    if (vkCmdEndDebugUtilsLabelEXT) {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        vkCmdEndDebugUtilsLabelEXT(cmd.command_buffer);
    }
}

void cmd::wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
{
    auto  rhi = get_rhi();
    auto& sem = fetch_resource(rhi->fences, fence);
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.wait(sem, sync);
}

void cmd::signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence, GPUBarrierSyncFlags sync)
{
    auto  rhi = get_rhi();
    auto& sem = fetch_resource(rhi->fences, fence);
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    cmd.signal(sem, sync);
}

void cmd::begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& descriptor)
{
    assert(!descriptor.color_attachments.empty());

    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    Vector<VkRenderingAttachmentInfo> color_attachments;
    for (auto& attachment : descriptor.color_attachments) {
        color_attachments.push_back({});

        auto& desc                       = color_attachments.back();
        desc.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        desc.pNext                       = nullptr;
        desc.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        desc.loadOp                      = vkenum(attachment.load_op);
        desc.storeOp                     = vkenum(attachment.store_op);
        desc.imageView                   = fetch_resource(rhi->views, attachment.view).view;
        desc.resolveImageView            = VK_NULL_HANDLE;
        desc.resolveMode                 = VK_RESOLVE_MODE_NONE;
        desc.clearValue.color.float32[0] = attachment.clear_value.r;
        desc.clearValue.color.float32[1] = attachment.clear_value.g;
        desc.clearValue.color.float32[2] = attachment.clear_value.b;
        desc.clearValue.color.float32[3] = attachment.clear_value.a;
    }

    VkRenderingAttachmentInfo depth_attachment{};
    VkRenderingAttachmentInfo stencil_attachment{};

    bool has_depth_stencil      = descriptor.depth_stencil_attachment.view.valid();
    bool has_depth_attachment   = false;
    bool has_stencil_attachment = false;
    if (has_depth_stencil) {
        auto& view = fetch_resource(rhi->views, descriptor.depth_stencil_attachment.view);

        has_depth_attachment   = view.aspects & VK_IMAGE_ASPECT_DEPTH_BIT;
        has_stencil_attachment = view.aspects & VK_IMAGE_ASPECT_STENCIL_BIT;

        auto layout = VK_IMAGE_LAYOUT_UNDEFINED;

        // clang-format off
        if      (has_depth_attachment && !has_stencil_attachment) layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        else if (!has_depth_attachment && has_stencil_attachment) layout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        else                                                      layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // clang-format on

        // depth attachment
        depth_attachment.sType                         = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.pNext                         = nullptr;
        depth_attachment.imageLayout                   = layout;
        depth_attachment.loadOp                        = vkenum(descriptor.depth_stencil_attachment.depth_load_op);
        depth_attachment.storeOp                       = vkenum(descriptor.depth_stencil_attachment.depth_store_op);
        depth_attachment.resolveImageView              = VK_NULL_HANDLE;
        depth_attachment.resolveMode                   = VK_RESOLVE_MODE_NONE;
        depth_attachment.clearValue.depthStencil.depth = descriptor.depth_stencil_attachment.depth_clear_value;
        depth_attachment.imageView                     = view.view;

        // stencil attachment
        stencil_attachment.sType                           = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        stencil_attachment.pNext                           = nullptr;
        stencil_attachment.imageLayout                     = layout;
        stencil_attachment.loadOp                          = vkenum(descriptor.depth_stencil_attachment.stencil_load_op);
        stencil_attachment.storeOp                         = vkenum(descriptor.depth_stencil_attachment.stencil_store_op);
        stencil_attachment.resolveImageView                = VK_NULL_HANDLE;
        stencil_attachment.resolveMode                     = VK_RESOLVE_MODE_NONE;
        stencil_attachment.clearValue.depthStencil.stencil = descriptor.depth_stencil_attachment.stencil_clear_value;
        stencil_attachment.imageView                       = view.view;
    }

    auto& view                = fetch_resource(rhi->views, descriptor.color_attachments.at(0).view);
    auto  render_area         = VkRect2D{};
    render_area.offset.x      = 0;
    render_area.offset.y      = 0;
    render_area.extent.width  = view.area.width;
    render_area.extent.height = view.area.height;

    auto rendering                 = VkRenderingInfo{};
    rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.pNext                = nullptr;
    rendering.renderArea           = render_area;
    rendering.layerCount           = 1;
    rendering.colorAttachmentCount = static_cast<uint32_t>(descriptor.color_attachments.size());
    rendering.pColorAttachments    = color_attachments.data();
    rendering.pDepthAttachment     = has_depth_attachment ? &depth_attachment : nullptr;
    rendering.pStencilAttachment   = has_stencil_attachment ? &stencil_attachment : nullptr;

    rhi->vtable.vkCmdBeginRenderingKHR(cmd.command_buffer, &rendering);

    // record the query set used for this render pass
    if (descriptor.occlusion_query_set.valid()) {
        cmd.query_set = fetch_resource(rhi->query_sets, descriptor.occlusion_query_set);
    }
}

void cmd::end_render_pass(GPUCommandEncoderHandle cmdbuffer)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    rhi->vtable.vkCmdEndRenderingKHR(cmd.command_buffer);
}

void cmd::set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (pip.pipeline != cmd.last_bound_pipeline) {
        cmd.last_bound_point    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        cmd.last_bound_layout   = pip.layout;
        cmd.last_bound_pipeline = pip.pipeline;
        rhi->vtable.vkCmdBindPipeline(cmd.command_buffer, cmd.last_bound_point, cmd.last_bound_pipeline);
    }
}

void cmd::set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (pip.pipeline != cmd.last_bound_pipeline) {
        cmd.last_bound_point    = VK_PIPELINE_BIND_POINT_COMPUTE;
        cmd.last_bound_layout   = pip.layout;
        cmd.last_bound_pipeline = pip.pipeline;
        rhi->vtable.vkCmdBindPipeline(cmd.command_buffer, cmd.last_bound_point, cmd.last_bound_pipeline);
    }
}

void cmd::set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& pip = fetch_resource(rhi->pipelines, pipeline);

    if (pip.pipeline != cmd.last_bound_pipeline) {
        cmd.last_bound_point    = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        cmd.last_bound_layout   = pip.layout;
        cmd.last_bound_pipeline = pip.pipeline;
        rhi->vtable.vkCmdBindPipeline(cmd.command_buffer, cmd.last_bound_point, cmd.last_bound_pipeline);
    }
}

void cmd::set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group, GPUBufferDynamicOffsets dynamic_offsets)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto  des = VkDescriptorSet(bind_group.value);
    rhi->vtable.vkCmdBindDescriptorSets(cmd.command_buffer, cmd.last_bound_point, cmd.last_bound_layout,
        index, 1, &des,
        static_cast<uint32_t>(dynamic_offsets.size()), dynamic_offsets.data());
}

void cmd::set_push_constants(GPUCommandEncoderHandle cmdbuffer, GPUShaderStageFlags visibility, uint offset, uint size, void* data)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    rhi->vtable.vkCmdPushConstants(cmd.command_buffer, cmd.last_bound_layout, vkenum(visibility), offset, size, data);
}

void cmd::set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUIndexFormat format, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);
    if (size == 0) {
        rhi->vtable.vkCmdBindIndexBuffer(cmd.command_buffer, buf.buffer, offset, vkenum(format));
    } else {
        rhi->vtable.vkCmdBindIndexBuffer2KHR(cmd.command_buffer, buf.buffer, offset, size, vkenum(format));
    }
}

void cmd::set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);
    if (size == 0) {
        rhi->vtable.vkCmdBindVertexBuffers(cmd.command_buffer, slot, 1, &buf.buffer, &offset);
    } else {
        rhi->vtable.vkCmdBindVertexBuffers2(cmd.command_buffer, slot, 1, &buf.buffer, &offset, &size, nullptr);
    }
}

void cmd::draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    rhi->vtable.vkCmdDraw(cmd.command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void cmd::draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    rhi->vtable.vkCmdDrawIndexed(cmd.command_buffer, index_count, instance_count, first_index, base_vertex, first_instance);
}

void cmd::draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    rhi->vtable.vkCmdDrawIndirect(cmd.command_buffer, buf.buffer, indirect_offset, draw_count, static_cast<uint32_t>(sizeof(VkDrawIndirectCommand)));
}

void cmd::draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    rhi->vtable.vkCmdDrawIndexedIndirect(cmd.command_buffer, buf.buffer, indirect_offset, draw_count, static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));
}

void cmd::dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    rhi->vtable.vkCmdDispatch(cmd.command_buffer, x, y, z);
}

void cmd::dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, indirect_buffer);
    rhi->vtable.vkCmdDispatchIndirect(cmd.command_buffer, buf.buffer, indirect_offset);
}

void cmd::copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source, GPUSize64 source_offset, GPUBufferHandle destination, GPUSize64 destination_offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& src = fetch_resource(rhi->buffers, source);
    auto& dst = fetch_resource(rhi->buffers, destination);

    auto copy      = VkBufferCopy{};
    copy.size      = size;
    copy.srcOffset = source_offset;
    copy.dstOffset = destination_offset;

    rhi->vtable.vkCmdCopyBuffer(cmd.command_buffer, src.buffer, dst.buffer, 1, &copy);
}

void cmd::copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& destination, GPUExtent3D copy_size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& src = fetch_resource(rhi->buffers, source.buffer);
    auto& dst = fetch_resource(rhi->textures, destination.texture);

    auto copy                            = VkBufferImageCopy{};
    copy.bufferOffset                    = source.offset;
    copy.bufferRowLength                 = infer_texture_row_length(dst.format, source.bytes_per_row);
    copy.bufferImageHeight               = source.rows_per_image;
    copy.imageOffset.x                   = destination.origin.x;
    copy.imageOffset.y                   = destination.origin.y;
    copy.imageOffset.z                   = destination.origin.z;
    copy.imageExtent.width               = copy_size.width;
    copy.imageExtent.height              = copy_size.height;
    copy.imageExtent.depth               = copy_size.depth;
    copy.imageSubresource.aspectMask     = vkenum(destination.aspect);
    copy.imageSubresource.mipLevel       = destination.mip_level;
    copy.imageSubresource.baseArrayLayer = 0; // TODO: Is WebGPU able to set this?
    copy.imageSubresource.layerCount     = 1; // TODO: Is WebGPU able to set this?

    auto layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    rhi->vtable.vkCmdCopyBufferToImage(cmd.command_buffer, src.buffer, dst.image, layout, 1, &copy);
}

void cmd::copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& destination, const GPUExtent3D& copy_size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& src = fetch_resource(rhi->textures, source.texture);
    auto& dst = fetch_resource(rhi->buffers, destination.buffer);

    if (destination.bytes_per_row != 0)
        assert("non-zero bytes per row is currently not implemented!");

    auto copy                            = VkBufferImageCopy{};
    copy.bufferOffset                    = destination.offset;
    copy.bufferRowLength                 = infer_texture_row_length(src.format, destination.bytes_per_row);
    copy.bufferImageHeight               = destination.rows_per_image;
    copy.imageOffset.x                   = source.origin.x;
    copy.imageOffset.y                   = source.origin.y;
    copy.imageOffset.z                   = source.origin.z;
    copy.imageExtent.width               = copy_size.width;
    copy.imageExtent.height              = copy_size.height;
    copy.imageExtent.depth               = copy_size.depth;
    copy.imageSubresource.aspectMask     = vkenum(source.aspect);
    copy.imageSubresource.mipLevel       = source.mip_level;
    copy.imageSubresource.baseArrayLayer = 0; // TODO: Is WebGPU able to set this?
    copy.imageSubresource.layerCount     = 1; // TODO: Is WebGPU able to set this?

    auto layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    rhi->vtable.vkCmdCopyImageToBuffer(cmd.command_buffer, src.image, layout, dst.buffer, 1, &copy);
}

void cmd::copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& destination, const GPUExtent3D& copy_size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& src = fetch_resource(rhi->textures, source.texture);
    auto& dst = fetch_resource(rhi->textures, destination.texture);

    auto copy                          = VkImageCopy{};
    copy.extent.width                  = copy_size.width;
    copy.extent.height                 = copy_size.height;
    copy.extent.depth                  = copy_size.depth;
    copy.srcOffset.x                   = source.origin.x;
    copy.srcOffset.y                   = source.origin.y;
    copy.srcOffset.z                   = source.origin.z;
    copy.dstOffset.x                   = destination.origin.x;
    copy.dstOffset.y                   = destination.origin.y;
    copy.dstOffset.z                   = destination.origin.z;
    copy.srcSubresource.aspectMask     = vkenum(source.aspect);
    copy.srcSubresource.mipLevel       = source.mip_level;
    copy.srcSubresource.baseArrayLayer = 0; // TODO: Is WebGPU able to set this?
    copy.srcSubresource.layerCount     = 1; // TODO: Is WebGPU able to set this?
    copy.dstSubresource.aspectMask     = vkenum(destination.aspect);
    copy.dstSubresource.mipLevel       = source.mip_level;
    copy.dstSubresource.baseArrayLayer = 0; // TODO: Is WebGPU able to set this?
    copy.dstSubresource.layerCount     = 1; // TODO: Is WebGPU able to set this?

    auto src_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    auto dst_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    rhi->vtable.vkCmdCopyImage(cmd.command_buffer, src.image, src_layout, dst.image, dst_layout, 1, &copy);
}

void cmd::clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, buffer);

    rhi->vtable.vkCmdFillBuffer(
        cmd.command_buffer,
        buf.buffer,
        offset,
        size == 0 ? VK_WHOLE_SIZE : size,
        0);
}

void cmd::set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    auto viewport     = VkViewport{};
    viewport.x        = x;
    viewport.y        = y;
    viewport.width    = w;
    viewport.height   = h;
    viewport.minDepth = min_depth;
    viewport.maxDepth = max_depth;

    rhi->vtable.vkCmdSetViewport(cmd.command_buffer, 0, 1, &viewport);
}

void cmd::set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    // Vulkan offsets are signed 32-bit integers, but must be >= 0.
    // Clamping to int32_t max to avoid overflow when casting.
    auto offset_x = std::clamp(static_cast<int32_t>(x), 0, std::numeric_limits<int32_t>::max());
    auto offset_y = std::clamp(static_cast<int32_t>(y), 0, std::numeric_limits<int32_t>::max());

    auto scissor          = VkRect2D{};
    scissor.offset.x      = offset_x;
    scissor.offset.y      = offset_y;
    scissor.extent.width  = w;
    scissor.extent.height = h;

    rhi->vtable.vkCmdSetScissor(cmd.command_buffer, 0, 1, &scissor);
}

void cmd::set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    float blend_color[4] = {
        color.r,
        color.g,
        color.b,
        color.a,
    };

    rhi->vtable.vkCmdSetBlendConstants(cmd.command_buffer, blend_color);
}

void cmd::set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    // NOTE: WebGPU applies stencil reference to both front and back uniformly to simplify the design.
    auto face = VK_STENCIL_FRONT_AND_BACK;
    rhi->vtable.vkCmdSetStencilReference(cmd.command_buffer, face, reference);
}

void cmd::begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query_index)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    assert(cmd.query_set.valid());

    cmd.query_index = query_index;
    rhi->vtable.vkCmdBeginQuery(cmd.command_buffer, cmd.query_set.pool, query_index, 0);
}

void cmd::end_occlusion_query(GPUCommandEncoderHandle cmdbuffer)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    assert(cmd.query_set.valid());
    assert(cmd.query_index.has_value());

    rhi->vtable.vkCmdEndQuery(cmd.command_buffer, cmd.query_set.pool, cmd.query_index.value());

    // reset query status
    cmd.query_set.pool = VK_NULL_HANDLE;
    cmd.query_index.reset();
}

void cmd::write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& qry = fetch_resource(rhi->query_sets, query_set);

    rhi->vtable.vkCmdWriteTimestamp(cmd.command_buffer, VK_PIPELINE_STAGE_NONE, qry.pool, query_index);
}

void cmd::write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 query_index, GPUBlasHandle blas)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& qry = fetch_resource(rhi->query_sets, query_set);
    auto& bvh = fetch_resource(rhi->blases, blas);

    rhi->vtable.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd.command_buffer, 1, &bvh.blas, qry.type, qry.pool, query_index);
}

void cmd::resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination, GPUSize64 destination_offset)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& qry = fetch_resource(rhi->query_sets, query_set);
    auto& buf = fetch_resource(rhi->buffers, destination);

    auto stride = sizeof(uint64_t);
    rhi->vtable.vkCmdCopyQueryPoolResults(cmd.command_buffer, qry.pool, first_query, query_count, buf.buffer, destination_offset, stride, 0);
}

void cmd::memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers barriers)
{
    Vector<VkMemoryBarrier2KHR> bars;
    for (auto& barrier : barriers) {
        auto b          = VkMemoryBarrier2KHR{};
        b.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
        b.pNext         = nullptr;
        b.srcStageMask  = vkenum(barrier.src_sync);
        b.dstStageMask  = vkenum(barrier.dst_sync);
        b.srcAccessMask = vkenum(barrier.src_access);
        b.dstAccessMask = vkenum(barrier.dst_access);
        bars.push_back(b);
    }

    auto dependency                     = VkDependencyInfoKHR{};
    dependency.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency.pNext                    = nullptr;
    dependency.dependencyFlags          = 0;
    dependency.memoryBarrierCount       = static_cast<uint32_t>(bars.size());
    dependency.pMemoryBarriers          = bars.data();
    dependency.bufferMemoryBarrierCount = 0;
    dependency.pBufferMemoryBarriers    = nullptr;
    dependency.imageMemoryBarrierCount  = 0;
    dependency.pImageMemoryBarriers     = nullptr;

    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    rhi->vtable.vkCmdPipelineBarrier2KHR(cmd.command_buffer, &dependency);
}

void cmd::buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    Vector<VkBufferMemoryBarrier2KHR> bars;
    for (auto& barrier : barriers) {
        auto b                = VkBufferMemoryBarrier2KHR{};
        b.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.pNext               = nullptr;
        b.srcStageMask        = vkenum(barrier.src_sync);
        b.dstStageMask        = vkenum(barrier.dst_sync);
        b.srcAccessMask       = vkenum(barrier.src_access);
        b.dstAccessMask       = vkenum(barrier.dst_access);
        b.buffer              = fetch_resource(rhi->buffers, barrier.buffer).buffer;
        b.offset              = barrier.offset;
        b.size                = barrier.size;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        bars.push_back(b);
    }

    auto dependency                     = VkDependencyInfoKHR{};
    dependency.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency.pNext                    = nullptr;
    dependency.dependencyFlags          = 0;
    dependency.memoryBarrierCount       = 0;
    dependency.pMemoryBarriers          = nullptr;
    dependency.bufferMemoryBarrierCount = static_cast<uint32_t>(bars.size());
    dependency.pBufferMemoryBarriers    = bars.data();
    dependency.imageMemoryBarrierCount  = 0;
    dependency.pImageMemoryBarriers     = nullptr;

    rhi->vtable.vkCmdPipelineBarrier2KHR(cmd.command_buffer, &dependency);
}

void cmd::texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    Vector<VkImageMemoryBarrier2KHR> bars;
    for (auto& barrier : barriers) {
        auto& t         = fetch_resource(rhi->textures, barrier.texture);
        auto  b         = VkImageMemoryBarrier2KHR{};
        b.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        b.pNext         = nullptr;
        b.srcStageMask  = vkenum(barrier.src_sync);
        b.dstStageMask  = vkenum(barrier.dst_sync);
        b.srcAccessMask = vkenum(barrier.src_access);
        b.dstAccessMask = vkenum(barrier.dst_access);
        b.image         = t.image;
        b.oldLayout     = vkenum(barrier.src_layout);
        b.newLayout     = vkenum(barrier.dst_layout);

        b.subresourceRange.aspectMask     = t.aspects;
        b.subresourceRange.baseArrayLayer = barrier.subresources.base_array_layer;
        b.subresourceRange.baseMipLevel   = barrier.subresources.base_mip_level;
        b.subresourceRange.layerCount     = barrier.subresources.array_layers;
        b.subresourceRange.levelCount     = barrier.subresources.mip_level_count;
        bars.push_back(b);
    }

    auto dependency                     = VkDependencyInfoKHR{};
    dependency.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependency.pNext                    = nullptr;
    dependency.dependencyFlags          = 0;
    dependency.memoryBarrierCount       = 0;
    dependency.pMemoryBarriers          = nullptr;
    dependency.bufferMemoryBarrierCount = 0;
    dependency.pBufferMemoryBarriers    = nullptr;
    dependency.imageMemoryBarrierCount  = static_cast<uint32_t>(bars.size());
    dependency.pImageMemoryBarriers     = bars.data();

    rhi->vtable.vkCmdPipelineBarrier2KHR(cmd.command_buffer, &dependency);
}

void cmd::build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUTlasBuildEntries entries)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, scratch_buffer);

    Vector<VkBufferMemoryBarrier>                       barriers;
    Vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos  = {};
    Vector<VkAccelerationStructureBuildRangeInfoKHR*>   build_ranges = {};

    barriers.reserve(entries.size());
    build_infos.reserve(entries.size());
    build_ranges.reserve(entries.size());

    VkDeviceAddress scratch_address = buf.device_address;
    for (auto& entry : entries) {
        auto& tlas = fetch_resource(rhi->tlases, entry.tlas);

        // create VkAccelerationStructureInstanceKHR on the fly
        auto address = tlas.staging.map<VkAccelerationStructureInstanceKHR>();
        for (auto& instance : entry.instances) {
            auto& blas = fetch_resource(rhi->blases, instance.blas);

            address->instanceCustomIndex                    = instance.custom_data;
            address->instanceShaderBindingTableRecordOffset = 0; // NOTE: We don't support more complex cases for now.
            address->mask                                   = instance.mask;
            address->flags                                  = blas.build.flags;
            address->accelerationStructureReference         = blas.reference;

            // copy transform matrix over
            // Vulkan uses 3x4 row-major transformation matrix
            // https://docs.vulkan.org/refpages/latest/refpages/source/VkTransformMatrixKHR.html
            memcpy(address->transform.matrix, instance.transform, sizeof(float) * 12);

            address++;
        }
        tlas.staging.unmap();

        // copy from staging buffer to instance buffer
        auto copy      = VkBufferCopy{};
        copy.size      = entry.instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        rhi->vtable.vkCmdCopyBuffer(cmd.command_buffer, tlas.staging.buffer, tlas.instances.buffer, 1, &copy);

        // prepare buffer barrier
        barriers.push_back({});
        auto& barrier               = barriers.back();
        barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext               = nullptr;
        barrier.offset              = 0u;
        barrier.size                = copy.size;
        barrier.buffer              = tlas.instances.buffer;
        barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;

        // prepare build info
        tlas.build.srcAccelerationStructure  = tlas.tlas;
        tlas.build.dstAccelerationStructure  = tlas.tlas;
        tlas.build.geometryCount             = 1;
        tlas.build.pGeometries               = &tlas.geometry;
        tlas.build.scratchData.deviceAddress = scratch_address;
        tlas.build.mode                      = (tlas.tlas == VK_NULL_HANDLE)
                                                   ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR
                                                   : vkenum(tlas.update_mode);

        // prepare build range
        tlas.range.firstVertex     = 0;
        tlas.range.primitiveCount  = static_cast<uint>(entry.instances.size());
        tlas.range.primitiveOffset = 0;
        tlas.range.transformOffset = 0;

        build_infos.push_back(tlas.build);
        build_ranges.push_back(&tlas.range);

        scratch_address += (tlas.build.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
                               ? tlas.sizes.buildScratchSize
                               : tlas.sizes.updateScratchSize;
    }

    // wait until copies have completed
    rhi->vtable.vkCmdPipelineBarrier(
        cmd.command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_DEPENDENCY_BY_REGION_BIT,
        0, nullptr,
        static_cast<uint32_t>(barriers.size()), barriers.data(),
        0, nullptr);

    rhi->vtable.vkCmdBuildAccelerationStructuresKHR(
        cmd.command_buffer,
        static_cast<uint32_t>(build_infos.size()),
        build_infos.data(),
        build_ranges.data());
}

void cmd::build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer, GPUBlasBuildEntries entries)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);
    auto& buf = fetch_resource(rhi->buffers, scratch_buffer);

    Vector<VkAccelerationStructureBuildGeometryInfoKHR> build_infos  = {};
    Vector<VkAccelerationStructureBuildRangeInfoKHR*>   build_ranges = {};

    build_infos.reserve(entries.size());
    build_ranges.reserve(entries.size());

    VkDeviceAddress scratch_address = buf.device_address;
    for (auto& entry : entries) {
        auto& blas = fetch_resource(rhi->blases, entry.blas);

        // geometry count check
        if (entry.geometries.triangles.size() > blas.geometries.size()) {
            get_logger()->error("Trying to build more geometries than BVH could hold!");
        }

        // update geometries and ranges
        uint geometry_count = static_cast<uint>(std::min(entry.geometries.triangles.size(), blas.geometries.size()));
        for (uint k = 0; k < geometry_count; k++) {
            auto& src_geometry = entry.geometries.triangles.at(k);
            auto& dst_geometry = blas.geometries.at(k);

            auto index_data = VkDeviceAddress{0};
            if (src_geometry.index_buffer.valid())
                index_data = fetch_resource(rhi->buffers, src_geometry.index_buffer).device_address;

            auto vertex_data = VkDeviceAddress{0};
            if (src_geometry.vertex_buffer.valid())
                vertex_data = fetch_resource(rhi->buffers, src_geometry.vertex_buffer).device_address;

            dst_geometry.sType                                       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            dst_geometry.geometryType                                = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            dst_geometry.geometry.triangles.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            dst_geometry.geometry.triangles.indexType                = vkenum(src_geometry.size.index_format);
            dst_geometry.geometry.triangles.indexData.deviceAddress  = index_data;
            dst_geometry.geometry.triangles.vertexStride             = src_geometry.vertex_stride;
            dst_geometry.geometry.triangles.vertexFormat             = vkenum(src_geometry.size.vertex_format);
            dst_geometry.geometry.triangles.vertexData.deviceAddress = vertex_data;
            dst_geometry.geometry.triangles.maxVertex                = src_geometry.size.vertex_count - 1;

            auto& range           = blas.ranges.at(k);
            range.firstVertex     = src_geometry.first_vertex;
            range.primitiveCount  = src_geometry.size.index_count / 3;
            range.primitiveOffset = src_geometry.first_index;
            range.transformOffset = 0;
        }

        // update build info
        blas.build.srcAccelerationStructure  = blas.blas;
        blas.build.dstAccelerationStructure  = blas.blas;
        blas.build.scratchData.deviceAddress = scratch_address;
        blas.build.geometryCount             = geometry_count;
        blas.build.pGeometries               = blas.geometries.data();
        blas.build.mode                      = (blas.blas == VK_NULL_HANDLE)
                                                   ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR
                                                   : vkenum(blas.update_mode);

        build_infos.push_back(blas.build);
        build_ranges.push_back(blas.ranges.data());

        scratch_address += (blas.build.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
                               ? blas.sizes.buildScratchSize
                               : blas.sizes.updateScratchSize;
    }

    rhi->vtable.vkCmdBuildAccelerationStructuresKHR(
        cmd.command_buffer,
        static_cast<uint32_t>(build_infos.size()),
        build_infos.data(),
        build_ranges.data());
}
