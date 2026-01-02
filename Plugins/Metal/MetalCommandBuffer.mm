#include "MetalUtils.h"
using namespace lyra;

void MetalCommandBuffer::transition_encoder(EncoderType new_type)
{
    if (active_encoder == new_type) return;
    end_current_encoder();
    active_encoder = new_type;
}

void MetalCommandBuffer::end_current_encoder()
{
    @autoreleasepool {
        switch (active_encoder) {
            case RENDER:
                if (render_encoder) {
                    [render_encoder endEncoding];
                    render_encoder = nil;
                }
                break;
            case COMPUTE:
                if (compute_encoder) {
                    [compute_encoder endEncoding];
                    compute_encoder = nil;
                }
                break;
            case BLIT:
                if (blit_encoder) {
                    [blit_encoder endEncoding];
                    blit_encoder = nil;
                }
                break;
            case ACCEL:
                if (accel_encoder) {
                    [accel_encoder endEncoding];
                    accel_encoder = nil;
                }
                break;
            case NONE:
                break;
        }
        active_encoder = NONE;
    }
}

void MetalCommandBuffer::reset()
{
    end_current_encoder();
    @autoreleasepool {
        command_buffer            = nil;
        bound_render_pso          = nil;
        bound_compute_pso         = nil;
        bound_depth_stencil_state = nil;
        bound_index_buffer        = nil;
        wait_events.clear();
        wait_values.clear();
        signal_events.clear();
        signal_values.clear();
    }
}

void MetalCommandBuffer::submit()
{
    end_current_encoder();

    // encode wait events
    for (size_t i = 0; i < wait_events.size(); ++i) {
        [command_buffer encodeWaitForEvent:wait_events[i] value:wait_values[i]];
    }

    // encode signal events
    for (size_t i = 0; i < signal_events.size(); ++i) {
        [command_buffer encodeSignalEvent:signal_events[i] value:signal_values[i]];
    }

    [command_buffer commit];
}

void MetalCommandBuffer::begin()
{
    // command buffer should already be allocated by frame
}

void MetalCommandBuffer::end()
{
    end_current_encoder();
}

bool api::create_command_buffer(GPUCommandEncoderHandle& handle, const GPUCommandBufferDescriptor& desc)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    handle    = frm.allocate(desc.queue, true);
    frm.command(handle).begin();
    return true;
}

bool api::create_command_bundle(GPUCommandEncoderHandle& handle, const GPUCommandBundleDescriptor& desc)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    handle    = frm.allocate(desc.queue, false);
    frm.command(handle).begin();
    return true;
}

bool api::submit_command_buffer(GPUCommandEncoderHandle handle)
{
    auto  rhi = get_rhi();
    auto& frm = rhi->current_frame();
    auto& cmd = frm.command(handle);
    cmd.end();
    cmd.submit();
    return true;
}

void cmd::insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label)
{
    @autoreleasepool {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        NSString* label = [NSString stringWithUTF8String:marker_label];
        if (cmd.render_encoder) {
            [cmd.render_encoder insertDebugSignpost:label];
        } else if (cmd.compute_encoder) {
            [cmd.compute_encoder insertDebugSignpost:label];
        } else if (cmd.blit_encoder) {
            [cmd.blit_encoder insertDebugSignpost:label];
        }
    }
}

void cmd::push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label)
{
    @autoreleasepool {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        NSString* label = [NSString stringWithUTF8String:group_label];
        if (cmd.render_encoder) {
            [cmd.render_encoder pushDebugGroup:label];
        } else if (cmd.compute_encoder) {
            [cmd.compute_encoder pushDebugGroup:label];
        } else if (cmd.blit_encoder) {
            [cmd.blit_encoder pushDebugGroup:label];
        }
    }
}

void cmd::pop_debug_group(GPUCommandEncoderHandle cmdbuffer)
{
    @autoreleasepool {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        if (cmd.render_encoder) {
            [cmd.render_encoder popDebugGroup];
        } else if (cmd.compute_encoder) {
            [cmd.compute_encoder popDebugGroup];
        } else if (cmd.blit_encoder) {
            [cmd.blit_encoder popDebugGroup];
        }
    }
}

void cmd::wait_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence_handle, GPUBarrierSyncFlags)
{
    auto  rhi   = get_rhi();
    auto& cmd   = rhi->current_frame().command(cmdbuffer);
    auto& fence = fetch_resource(rhi->fences, fence_handle);

    cmd.wait_events.push_back(fence.event);
    cmd.wait_values.push_back(fence.target);
}

void cmd::signal_fence(GPUCommandEncoderHandle cmdbuffer, GPUFenceHandle fence_handle, GPUBarrierSyncFlags)
{
    auto  rhi   = get_rhi();
    auto& cmd   = rhi->current_frame().command(cmdbuffer);
    auto& fence = fetch_resource(rhi->fences, fence_handle);

    fence.target++;
    cmd.signal_events.push_back(fence.event);
    cmd.signal_values.push_back(fence.target);
}

void cmd::begin_render_pass(GPUCommandEncoderHandle cmdbuffer, const GPURenderPassDescriptor& desc)
{
    @autoreleasepool {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        // end any existing encoder
        cmd.end_current_encoder();

        // build MTLRenderPassDescriptor
        MTLRenderPassDescriptor* mtl_pass = [MTLRenderPassDescriptor new];

        // color Attachments
        uint color_index = 0;
        for (auto& attachment : desc.color_attachments) {
            if (!attachment.view.valid()) continue;

            auto& view                                         = fetch_resource(rhi->views, attachment.view);
            mtl_pass.colorAttachments[color_index].texture     = view.texture;
            mtl_pass.colorAttachments[color_index].loadAction  = mtlenum(attachment.load_op);
            mtl_pass.colorAttachments[color_index].storeAction = mtlenum(attachment.store_op);
            mtl_pass.colorAttachments[color_index].clearColor  = MTLClearColorMake(
                attachment.clear_value.r, attachment.clear_value.g,
                attachment.clear_value.b, attachment.clear_value.a);

            // resolve target for MSAA
            if (attachment.resolve_target.valid()) {
                auto& resolve_view                                    = fetch_resource(rhi->views, attachment.resolve_target);
                mtl_pass.colorAttachments[color_index].resolveTexture = resolve_view.texture;
                mtl_pass.colorAttachments[color_index].storeAction    = MTLStoreActionMultisampleResolve;
            }

            color_index++;
        }

        // depth/stencil attachment
        if (desc.depth_stencil_attachment.view.valid()) {
            auto& view                           = fetch_resource(rhi->views, desc.depth_stencil_attachment.view);
            mtl_pass.depthAttachment.texture     = view.texture;
            mtl_pass.depthAttachment.loadAction  = mtlenum(desc.depth_stencil_attachment.depth_load_op);
            mtl_pass.depthAttachment.storeAction = mtlenum(desc.depth_stencil_attachment.depth_store_op);
            mtl_pass.depthAttachment.clearDepth  = desc.depth_stencil_attachment.depth_clear_value;

            // stencil (if format supports it)
            mtl_pass.stencilAttachment.texture      = view.texture;
            mtl_pass.stencilAttachment.loadAction   = mtlenum(desc.depth_stencil_attachment.stencil_load_op);
            mtl_pass.stencilAttachment.storeAction  = mtlenum(desc.depth_stencil_attachment.stencil_store_op);
            mtl_pass.stencilAttachment.clearStencil = desc.depth_stencil_attachment.stencil_clear_value;
        }

        cmd.render_encoder = [cmd.command_buffer renderCommandEncoderWithDescriptor:mtl_pass];
        cmd.active_encoder = MetalCommandBuffer::RENDER;
    }
}

void cmd::end_render_pass(GPUCommandEncoderHandle cmdbuffer)
{
    @autoreleasepool {
        auto  rhi = get_rhi();
        auto& cmd = rhi->current_frame().command(cmdbuffer);

        if (cmd.render_encoder) {
            [cmd.render_encoder endEncoding];
            cmd.render_encoder = nil;
        }
        cmd.active_encoder = MetalCommandBuffer::NONE;
    }
}

void cmd::set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline_handle)
{
    auto  rhi      = get_rhi();
    auto& cmd      = rhi->current_frame().command(cmdbuffer);
    auto& pipeline = fetch_resource(rhi->pipelines, pipeline_handle);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder setRenderPipelineState:pipeline.render_pso];
    cmd.bound_render_pso = pipeline.render_pso;
    cmd.bound_layout     = pipeline.layout;
    cmd.primitive_type   = pipeline.primitive_type;

    // set depth stencil state if available
    if (pipeline.depth_stencil_state) {
        [cmd.render_encoder setDepthStencilState:pipeline.depth_stencil_state];
        cmd.bound_depth_stencil_state = pipeline.depth_stencil_state;
    }

    // set cull mode and front face
    [cmd.render_encoder setCullMode:pipeline.cull_mode];
    [cmd.render_encoder setFrontFacingWinding:pipeline.front_face];

    // set depth bias if needed
    if (pipeline.depth_bias != 0.0f || pipeline.depth_bias_slope != 0.0f) {
        [cmd.render_encoder setDepthBias:pipeline.depth_bias
                              slopeScale:pipeline.depth_bias_slope
                                   clamp:pipeline.depth_bias_clamp];
    }
}

void cmd::set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline_handle)
{
    auto  rhi      = get_rhi();
    auto& cmd      = rhi->current_frame().command(cmdbuffer);
    auto& pipeline = fetch_resource(rhi->pipelines, pipeline_handle);

    // transition to compute encoder if needed
    cmd.transition_encoder(MetalCommandBuffer::COMPUTE);
    if (!cmd.compute_encoder) {
        cmd.compute_encoder = [cmd.command_buffer computeCommandEncoder];
    }

    [cmd.compute_encoder setComputePipelineState:pipeline.compute_pso];
    cmd.bound_compute_pso = pipeline.compute_pso;
    cmd.bound_layout      = pipeline.layout;
}

void cmd::set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline_handle)
{
    auto rhi = get_rhi();

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        throw GPUValidationError("Metal ray tracing is not supported on this device");
    }

    auto& cmd      = rhi->current_frame().command(cmdbuffer);
    auto& pipeline = fetch_resource(rhi->pipelines, pipeline_handle);

    // NOTE: Metal ray tracing works through compute pipelines with visible/intersection function tables.
    // The ray tracing pipeline in this engine is a placeholder that stores configuration.
    //
    // To perform ray tracing, users should:
    // 1. Create a compute pipeline with ray tracing functions
    // 2. Use cmd_set_compute_pipeline to bind it
    // 3. Set acceleration structure resources via bind groups
    // 4. Dispatch compute workgroups
    //
    // the visible and intersection function tables would be bound here if the RHI
    // supported full ray tracing shader specification.

    // transition to compute encoder for ray tracing operations
    cmd.transition_encoder(MetalCommandBuffer::COMPUTE);
    if (!cmd.compute_encoder) {
        cmd.compute_encoder = [cmd.command_buffer computeCommandEncoder];
    }

    // store the layout for bind group operations
    cmd.bound_layout = pipeline.layout;

    // if we have function tables, they would be set on the compute encoder here:
    // if (pipeline.visible_function_table) {
    //     [cmd.compute_encoder setVisibleFunctionTable:pipeline.visible_function_table atBufferIndex:...];
    // }
    // if (pipeline.intersection_function_table) {
    //     [cmd.compute_encoder setIntersectionFunctionTable:pipeline.intersection_function_table atBufferIndex:...];
    // }

    get_logger()->debug("Ray tracing pipeline bound (max_recursion_depth={})", pipeline.max_recursion_depth);
}

void cmd::set_bind_group(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 index, GPUBindGroupHandle bind_group_handle, GPUBufferDynamicOffsets)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!bind_group_handle.valid()) return;

    if (!cmd.bound_layout.valid()) {
        get_logger()->error("No valid pipeline layout bound when setting bind group");
        throw GPUValidationError("No valid pipeline layout bound when setting bind group");
    }

    // cast handle value directly to pointer
    auto* bind_group = reinterpret_cast<MetalBindGroup*>(bind_group_handle.value);
    if (!bind_group) {
        get_logger()->error("bind group is invalid!");
        throw GPUValidationError("Invalid bind group handle");
    }

    auto& layout = fetch_resource(rhi->bind_group_layouts, bind_group->layout_handle);
    auto& pipeline_layout = fetch_resource(rhi->pipeline_layouts, cmd.bound_layout);
    
    if (layout.is_argument_buffer) {
        uint32_t key = (index << 16) | 0;
        if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
            uint32_t slot = pipeline_layout.buffer_indices.at(key);
            if (cmd.render_encoder) {
                [cmd.render_encoder setVertexBuffer:bind_group->argument_buffer offset:0 atIndex:slot];
                [cmd.render_encoder setFragmentBuffer:bind_group->argument_buffer offset:0 atIndex:slot];
                for(const auto& res : bind_group->used_resources) {
                    [cmd.render_encoder useResource:res.first usage:res.second];
                }
            } else if (cmd.compute_encoder) {
                [cmd.compute_encoder setBuffer:bind_group->argument_buffer offset:0 atIndex:slot];
                for(const auto& res : bind_group->used_resources) {
                    [cmd.compute_encoder useResource:res.first usage:res.second];
                }
            }
        }
    } else {
        // retrieve pipeline layout for remapped resource slots
        for (uint32_t i = 0; i < bind_group->entry_count; ++i) {
            const auto& entry = bind_group->entries[i];
            uint32_t    key   = (index << 16) | entry.binding;

            switch (entry.type) {
                case GPUResourceType::BUFFER:
                {
                    if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
                        uint32_t slot = pipeline_layout.buffer_indices.at(key);
                        if (cmd.render_encoder) {
                            [cmd.render_encoder setVertexBuffer:entry.buffer.buffer offset:entry.buffer.offset atIndex:slot];
                            [cmd.render_encoder setFragmentBuffer:entry.buffer.buffer offset:entry.buffer.offset atIndex:slot];
                        } else if (cmd.compute_encoder) {
                            [cmd.compute_encoder setBuffer:entry.buffer.buffer offset:entry.buffer.offset atIndex:slot];
                        }
                    }
                    break;
                }
                case GPUResourceType::TEXTURE:
                case GPUResourceType::STORAGE_TEXTURE:
                {
                    if (pipeline_layout.texture_indices.find(key) != pipeline_layout.texture_indices.end()) {
                        uint32_t slot = pipeline_layout.texture_indices.at(key);
                        if (cmd.render_encoder) {
                            [cmd.render_encoder setVertexTexture:entry.texture.texture atIndex:slot];
                            [cmd.render_encoder setFragmentTexture:entry.texture.texture atIndex:slot];
                        } else if (cmd.compute_encoder) {
                            [cmd.compute_encoder setTexture:entry.texture.texture atIndex:slot];
                        }
                    }
                    break;
                }
                case GPUResourceType::SAMPLER:
                {
                    if (pipeline_layout.sampler_indices.find(key) != pipeline_layout.sampler_indices.end()) {
                        uint32_t slot = pipeline_layout.sampler_indices.at(key);
                        if (cmd.render_encoder) {
                            [cmd.render_encoder setVertexSamplerState:entry.sampler.sampler atIndex:slot];
                            [cmd.render_encoder setFragmentSamplerState:entry.sampler.sampler atIndex:slot];
                        } else if (cmd.compute_encoder) {
                            [cmd.compute_encoder setSamplerState:entry.sampler.sampler atIndex:slot];
                        }
                    }
                    break;
                }
                case GPUResourceType::ACCELERATION_STRUCTURE:
                {
                    if (@available(macOS 13.0, iOS 16.0, *)) {
                        if (pipeline_layout.buffer_indices.find(key) != pipeline_layout.buffer_indices.end()) {
                            uint32_t slot = pipeline_layout.buffer_indices.at(key);
                            if (cmd.render_encoder) {
                                [cmd.render_encoder setVertexAccelerationStructure:entry.tlas.tlas atBufferIndex:slot];
                                [cmd.render_encoder setFragmentAccelerationStructure:entry.tlas.tlas atBufferIndex:slot];
                            } else if (cmd.compute_encoder) {
                                [cmd.compute_encoder setAccelerationStructure:entry.tlas.tlas atBufferIndex:slot];
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

void cmd::set_push_constants(GPUCommandEncoderHandle cmdbuffer, GPUShaderStageFlags visibility, uint offset, uint size, void* data)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    // Metal uses setBytes for small data (< 4KB)
    // push constant slot is typically at index 0 or a reserved buffer index
    uint buffer_index = METAL_PushConstantBufferIndex; // use high index for push constants

    if (cmd.render_encoder && (visibility.contains(GPUShaderStage::VERTEX) || visibility.contains(GPUShaderStage::FRAGMENT))) {
        if (visibility.contains(GPUShaderStage::VERTEX)) {
            [cmd.render_encoder setVertexBytes:data length:size atIndex:buffer_index];
        }
        if (visibility.contains(GPUShaderStage::FRAGMENT)) {
            [cmd.render_encoder setFragmentBytes:data length:size atIndex:buffer_index];
        }
    } else if (cmd.compute_encoder && visibility.contains(GPUShaderStage::COMPUTE)) {
        [cmd.compute_encoder setBytes:data length:size atIndex:buffer_index];
    }
}

void cmd::set_index_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer_handle, GPUIndexFormat format, GPUSize64 offset, GPUSize64)
{
    auto  rhi    = get_rhi();
    auto& cmd    = rhi->current_frame().command(cmdbuffer);
    auto& buffer = fetch_resource(rhi->buffers, buffer_handle);

    cmd.bound_index_buffer  = buffer.buffer;
    cmd.index_format        = format;
    cmd.index_type          = mtlenum(format);
    cmd.index_buffer_offset = offset;
}

void cmd::set_vertex_buffer(GPUCommandEncoderHandle cmdbuffer, GPUIndex32 slot, GPUBufferHandle buffer_handle, GPUSize64 offset, GPUSize64)
{
    auto  rhi    = get_rhi();
    auto& cmd    = rhi->current_frame().command(cmdbuffer);
    auto& buffer = fetch_resource(rhi->buffers, buffer_handle);

    if (cmd.render_encoder) {
        uint32_t metal_slot = METAL_VertexBufferSlotIndex - slot;
        [cmd.render_encoder setVertexBuffer:buffer.buffer offset:offset atIndex:metal_slot];
    }
}

void cmd::draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder drawPrimitives:cmd.primitive_type
                           vertexStart:first_vertex
                           vertexCount:vertex_count
                         instanceCount:instance_count
                          baseInstance:first_instance];
}

void cmd::draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder || !cmd.bound_index_buffer) return;

    size_t index_size   = (cmd.index_type == MTLIndexTypeUInt16) ? 2 : 4;
    size_t index_offset = cmd.index_buffer_offset + first_index * index_size;

    [cmd.render_encoder drawIndexedPrimitives:cmd.primitive_type
                                   indexCount:index_count
                                    indexType:cmd.index_type
                                  indexBuffer:cmd.bound_index_buffer
                            indexBufferOffset:index_offset
                                instanceCount:instance_count
                                   baseVertex:base_vertex
                                 baseInstance:first_instance];
}

void cmd::draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer_handle, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi             = get_rhi();
    auto& cmd             = rhi->current_frame().command(cmdbuffer);
    auto& indirect_buffer = fetch_resource(rhi->buffers, indirect_buffer_handle);

    if (!cmd.render_encoder) return;

    for (GPUSize32 i = 0; i < draw_count; ++i) {
        [cmd.render_encoder drawPrimitives:cmd.primitive_type
                            indirectBuffer:indirect_buffer.buffer
                      indirectBufferOffset:indirect_offset + i * sizeof(MTLDrawPrimitivesIndirectArguments)];
    }
}

void cmd::draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer_handle, GPUSize64 indirect_offset, GPUSize32 draw_count)
{
    auto  rhi             = get_rhi();
    auto& cmd             = rhi->current_frame().command(cmdbuffer);
    auto& indirect_buffer = fetch_resource(rhi->buffers, indirect_buffer_handle);

    if (!cmd.render_encoder || !cmd.bound_index_buffer) return;

    for (GPUSize32 i = 0; i < draw_count; ++i) {
        [cmd.render_encoder drawIndexedPrimitives:cmd.primitive_type
                                        indexType:cmd.index_type
                                      indexBuffer:cmd.bound_index_buffer
                                indexBufferOffset:cmd.index_buffer_offset
                                   indirectBuffer:indirect_buffer.buffer
                             indirectBufferOffset:indirect_offset + i * sizeof(MTLDrawIndexedPrimitivesIndirectArguments)];
    }
}

void cmd::dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.compute_encoder || !cmd.bound_compute_pso) return;

    MTLSize threadgroups = MTLSizeMake(x, y, z);

    // Get optimal threadgroup size from pipeline
    NSUInteger width             = [cmd.bound_compute_pso threadExecutionWidth];
    NSUInteger height            = [cmd.bound_compute_pso maxTotalThreadsPerThreadgroup] / width;
    MTLSize    threads_per_group = MTLSizeMake(width, height > 0 ? height : 1, 1);

    [cmd.compute_encoder dispatchThreadgroups:threadgroups
                        threadsPerThreadgroup:threads_per_group];
}

void cmd::dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer_handle, GPUSize64 indirect_offset)
{
    auto  rhi             = get_rhi();
    auto& cmd             = rhi->current_frame().command(cmdbuffer);
    auto& indirect_buffer = fetch_resource(rhi->buffers, indirect_buffer_handle);

    if (!cmd.compute_encoder || !cmd.bound_compute_pso) return;

    NSUInteger width             = [cmd.bound_compute_pso threadExecutionWidth];
    NSUInteger height            = [cmd.bound_compute_pso maxTotalThreadsPerThreadgroup] / width;
    MTLSize    threads_per_group = MTLSizeMake(width, height > 0 ? height : 1, 1);

    [cmd.compute_encoder dispatchThreadgroupsWithIndirectBuffer:indirect_buffer.buffer
                                           indirectBufferOffset:indirect_offset
                                          threadsPerThreadgroup:threads_per_group];
}

void cmd::copy_buffer_to_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle source_handle, GPUSize64 source_offset, GPUBufferHandle dest_handle, GPUSize64 dest_offset, GPUSize64 size)
{
    auto  rhi    = get_rhi();
    auto& cmd    = rhi->current_frame().command(cmdbuffer);
    auto& source = fetch_resource(rhi->buffers, source_handle);
    auto& dest   = fetch_resource(rhi->buffers, dest_handle);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    [cmd.blit_encoder copyFromBuffer:source.buffer
                        sourceOffset:source_offset
                            toBuffer:dest.buffer
                   destinationOffset:dest_offset
                                size:size];
}

void cmd::copy_buffer_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyBufferInfo& source, const GPUTexelCopyTextureInfo& dest, GPUExtent3D copy_size)
{
    auto  rhi     = get_rhi();
    auto& cmd     = rhi->current_frame().command(cmdbuffer);
    auto& buffer  = fetch_resource(rhi->buffers, source.buffer);
    auto& texture = fetch_resource(rhi->textures, dest.texture);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    MTLOrigin mtl_origin = MTLOriginMake(dest.origin.x, dest.origin.y, dest.origin.z);
    MTLSize   size       = MTLSizeMake(copy_size.width, copy_size.height, copy_size.depth);

    uint32_t bytes_per_row  = source.bytes_per_row;
    uint32_t rows_per_image = source.rows_per_image;

    // tightly packed texture (infer bytes per row)
    if (bytes_per_row == 0 || rows_per_image == 0) {
        uint32_t bw = block_width(texture.format);
        uint32_t bh = block_height(texture.format);
        uint32_t bs = size_of(texture.format);
        if (bytes_per_row == 0) {
            uint32_t pitch = ((copy_size.width + bw - 1) / bw) * bs;
            bytes_per_row  = (pitch + rhi->texture_row_pitch_alignment - 1) & ~(rhi->texture_row_pitch_alignment - 1);
        }
        if (rows_per_image == 0) {
            rows_per_image = (copy_size.height + bh - 1) / bh;
        }
    }

    NSUInteger destination_slice = 0;
    if (texture.type == MTLTextureType2DArray || texture.type == MTLTextureTypeCube || texture.type == MTLTextureTypeCubeArray) {
        destination_slice = mtl_origin.z;
        mtl_origin.z      = 0;
    }

    NSUInteger source_bytes_per_image = 0;
    if (texture.type == MTLTextureType3D || texture.type == MTLTextureType2DArray || texture.type == MTLTextureTypeCube || texture.type == MTLTextureTypeCubeArray) {
        source_bytes_per_image = (NSUInteger)bytes_per_row * rows_per_image;
    }

    [cmd.blit_encoder copyFromBuffer:buffer.buffer
                        sourceOffset:source.offset
                   sourceBytesPerRow:bytes_per_row
                 sourceBytesPerImage:source_bytes_per_image
                          sourceSize:size
                           toTexture:texture.texture
                    destinationSlice:destination_slice
                    destinationLevel:dest.mip_level
                   destinationOrigin:mtl_origin];
}

void cmd::copy_texture_to_buffer(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyBufferInfo& dest, const GPUExtent3D& copy_size)
{
    auto  rhi     = get_rhi();
    auto& cmd     = rhi->current_frame().command(cmdbuffer);
    auto& texture = fetch_resource(rhi->textures, source.texture);
    auto& buffer  = fetch_resource(rhi->buffers, dest.buffer);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    MTLOrigin mtl_origin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
    MTLSize   size       = MTLSizeMake(copy_size.width, copy_size.height, copy_size.depth);

    uint32_t bytes_per_row  = dest.bytes_per_row;
    uint32_t rows_per_image = dest.rows_per_image;

    // tightly packed texture (infer bytes per row)
    if (bytes_per_row == 0 || rows_per_image == 0) {
        uint32_t bw = block_width(texture.format);
        uint32_t bh = block_height(texture.format);
        uint32_t bs = size_of(texture.format);
        if (bytes_per_row == 0) {
            uint32_t pitch = ((copy_size.width + bw - 1) / bw) * bs;
            bytes_per_row  = (pitch + rhi->texture_row_pitch_alignment - 1) & ~(rhi->texture_row_pitch_alignment - 1);
        }
        if (rows_per_image == 0) {
            rows_per_image = (copy_size.height + bh - 1) / bh;
        }
    }

    NSUInteger source_slice = 0;
    if (texture.type == MTLTextureType2DArray || texture.type == MTLTextureTypeCube || texture.type == MTLTextureTypeCubeArray) {
        source_slice = mtl_origin.z;
        mtl_origin.z = 0;
    }

    NSUInteger dest_bytes_per_image = 0;
    if (texture.type == MTLTextureType3D || texture.type == MTLTextureType2DArray || texture.type == MTLTextureTypeCube || texture.type == MTLTextureTypeCubeArray) {
        dest_bytes_per_image = (NSUInteger)bytes_per_row * rows_per_image;
    }

    [cmd.blit_encoder copyFromTexture:texture.texture
                          sourceSlice:source_slice
                          sourceLevel:source.mip_level
                         sourceOrigin:mtl_origin
                           sourceSize:size
                             toBuffer:buffer.buffer
                    destinationOffset:dest.offset
               destinationBytesPerRow:bytes_per_row
             destinationBytesPerImage:dest_bytes_per_image];
}

void cmd::copy_texture_to_texture(GPUCommandEncoderHandle cmdbuffer, const GPUTexelCopyTextureInfo& source, const GPUTexelCopyTextureInfo& dest, const GPUExtent3D& copy_size)
{
    auto  rhi         = get_rhi();
    auto& cmd         = rhi->current_frame().command(cmdbuffer);
    auto& src_texture = fetch_resource(rhi->textures, source.texture);
    auto& dst_texture = fetch_resource(rhi->textures, dest.texture);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    MTLOrigin src_mtl_origin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
    MTLOrigin dst_mtl_origin = MTLOriginMake(dest.origin.x, dest.origin.y, dest.origin.z);
    MTLSize   size           = MTLSizeMake(copy_size.width, copy_size.height, copy_size.depth);

    NSUInteger source_slice = 0;
    if (src_texture.type == MTLTextureType2DArray || src_texture.type == MTLTextureTypeCube || src_texture.type == MTLTextureTypeCubeArray) {
        source_slice       = src_mtl_origin.z;
        src_mtl_origin.z = 0;
    }

    NSUInteger destination_slice = 0;
    if (dst_texture.type == MTLTextureType2DArray || dst_texture.type == MTLTextureTypeCube || dst_texture.type == MTLTextureTypeCubeArray) {
        destination_slice = dst_mtl_origin.z;
        dst_mtl_origin.z  = 0;
    }

    [cmd.blit_encoder copyFromTexture:src_texture.texture
                          sourceSlice:source_slice
                          sourceLevel:source.mip_level
                         sourceOrigin:src_mtl_origin
                           sourceSize:size
                            toTexture:dst_texture.texture
                     destinationSlice:destination_slice
                     destinationLevel:dest.mip_level
                    destinationOrigin:dst_mtl_origin];
}

void cmd::clear_buffer(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle buffer_handle, GPUSize64 offset, GPUSize64 size)
{
    auto  rhi    = get_rhi();
    auto& cmd    = rhi->current_frame().command(cmdbuffer);
    auto& buffer = fetch_resource(rhi->buffers, buffer_handle);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    GPUSize64 clear_size = (size == 0) ? [buffer.buffer length] - offset : size;
    [cmd.blit_encoder fillBuffer:buffer.buffer range:NSMakeRange(offset, clear_size) value:0];
}

void cmd::clear_texture(GPUCommandEncoderHandle, GPUTextureHandle, const GPUTextureSubresourceRange&)
{
    // Metal doesn't have a direct clear texture command
    // Would need to use a render pass with clear load action
}

void cmd::set_viewport(GPUCommandEncoderHandle cmdbuffer, float x, float y, float w, float h, float min_depth, float max_depth)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    MTLViewport viewport = {x, y, w, h, min_depth, max_depth};
    [cmd.render_encoder setViewport:viewport];
}

void cmd::set_scissor_rect(GPUCommandEncoderHandle cmdbuffer, GPUIntegerCoordinate x, GPUIntegerCoordinate y, GPUIntegerCoordinate w, GPUIntegerCoordinate h)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    MTLScissorRect scissor = {x, y, w, h};
    [cmd.render_encoder setScissorRect:scissor];
}

void cmd::set_blend_constant(GPUCommandEncoderHandle cmdbuffer, GPUColor color)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder setBlendColorRed:color.r green:color.g blue:color.b alpha:color.a];
}

void cmd::set_stencil_reference(GPUCommandEncoderHandle cmdbuffer, GPUStencilValue reference)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder setStencilReferenceValue:reference];
}

void cmd::begin_occlusion_query(GPUCommandEncoderHandle cmdbuffer, GPUSize32 query_index)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder setVisibilityResultMode:MTLVisibilityResultModeBoolean offset:query_index * sizeof(uint64_t)];
}

void cmd::end_occlusion_query(GPUCommandEncoderHandle cmdbuffer)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (!cmd.render_encoder) return;

    [cmd.render_encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
}

void cmd::write_timestamp(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set_handle, GPUSize32 query_index)
{
    auto  rhi       = get_rhi();
    auto& cmd       = rhi->current_frame().command(cmdbuffer);
    auto& query_set = fetch_resource(rhi->query_sets, query_set_handle);

    if (query_set.type != GPUQueryType::TIMESTAMP || !query_set.sample_buffer) {
        get_logger()->error("Invalid query set for timestamp write");
        throw GPUValidationError("Invalid query set for timestamp write");
    }

    // Metal requires sampling timestamps at specific points using MTLCounterSampleBuffer
    // timestamps are sampled via sampleCounters API on blit or compute encoders
    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    [cmd.blit_encoder sampleCountersInBuffer:query_set.sample_buffer
                               atSampleIndex:query_index
                                 withBarrier:YES];
}

void cmd::write_blas_properties(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set_handle, GPUSize32 query_index, GPUBlasHandle blas_handle)
{
    auto  rhi       = get_rhi();
    auto& cmd       = rhi->current_frame().command(cmdbuffer);
    auto& query_set = fetch_resource(rhi->query_sets, query_set_handle);
    auto& blas      = fetch_resource(rhi->blases, blas_handle);

    if (query_set.type != GPUQueryType::BLAS_PROPERTIES || !query_set.visibility_buffer) {
        get_logger()->error("Invalid query set for BLAS properties write");
        throw GPUValidationError("Invalid query set for BLAS properties write");
    }

    // for BLAS properties, we write the acceleration structure sizes to the buffer
    // this is done on the CPU side since Metal doesn't have a direct GPU query for this
    MTLAccelerationStructureSizes* sizes_ptr =
        (MTLAccelerationStructureSizes*)([query_set.visibility_buffer contents]) + query_index;
    *sizes_ptr = blas.sizes;
}

void cmd::resolve_query_set(GPUCommandEncoderHandle cmdbuffer, GPUQuerySetHandle query_set_handle, GPUSize32 first_query, GPUSize32 query_count, GPUBufferHandle destination_handle, GPUSize64 destination_offset)
{
    auto  rhi         = get_rhi();
    auto& cmd         = rhi->current_frame().command(cmdbuffer);
    auto& query_set   = fetch_resource(rhi->query_sets, query_set_handle);
    auto& destination = fetch_resource(rhi->buffers, destination_handle);

    cmd.transition_encoder(MetalCommandBuffer::BLIT);
    if (!cmd.blit_encoder) {
        cmd.blit_encoder = [cmd.command_buffer blitCommandEncoder];
    }

    switch (query_set.type) {
        case GPUQueryType::TIMESTAMP:
        {
            if (query_set.sample_buffer) {
                // resolve counter sample buffer to destination buffer
                [cmd.blit_encoder resolveCounters:query_set.sample_buffer
                                          inRange:NSMakeRange(first_query, query_count)
                                destinationBuffer:destination.buffer
                                destinationOffset:destination_offset];
            }
            break;
        }
        case GPUQueryType::OCCLUSION:
        case GPUQueryType::BLAS_PROPERTIES:
        {
            if (query_set.visibility_buffer) {
                // copy from visibility buffer to destination
                size_t element_size  = (query_set.type == GPUQueryType::OCCLUSION)
                                           ? sizeof(uint64_t)
                                           : sizeof(MTLAccelerationStructureSizes);
                size_t copy_size     = query_count * element_size;
                size_t source_offset = first_query * element_size;

                [cmd.blit_encoder copyFromBuffer:query_set.visibility_buffer
                                    sourceOffset:source_offset
                                        toBuffer:destination.buffer
                               destinationOffset:destination_offset
                                            size:copy_size];
            }
            break;
        }
        case GPUQueryType::PIPELINE_STATISTICS:
            // Pipeline statistics have limited support on Metal
            get_logger()->warn("Pipeline statistics query resolution not fully supported");
            break;
    }
}

void cmd::memory_barrier(GPUCommandEncoderHandle cmdbuffer, GPUMemoryBarriers barriers)
{
    auto  rhi = get_rhi();
    auto& cmd = rhi->current_frame().command(cmdbuffer);

    if (cmd.render_encoder) {
        MTLBarrierScope scope  = MTLBarrierScopeBuffers | MTLBarrierScopeTextures;
        MTLRenderStages after  = MTLRenderStageVertex | MTLRenderStageFragment;
        MTLRenderStages before = MTLRenderStageVertex | MTLRenderStageFragment;
        [cmd.render_encoder memoryBarrierWithScope:scope afterStages:after beforeStages:before];
    } else if (cmd.compute_encoder) {
        [cmd.compute_encoder memoryBarrierWithScope:MTLBarrierScopeBuffers | MTLBarrierScopeTextures];
    }
}

void cmd::buffer_barrier(GPUCommandEncoderHandle cmdbuffer, GPUBufferBarriers barriers)
{
    // buffer barriers handled through memory barrier
    memory_barrier(cmdbuffer, {});
}

void cmd::texture_barrier(GPUCommandEncoderHandle cmdbuffer, GPUTextureBarriers barriers)
{
    // texture barriers handled through memory barrier
    memory_barrier(cmdbuffer, {});
}

void cmd::build_tlases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer_handle, GPUTlasBuildEntries entries)
{
    auto  rhi            = get_rhi();
    auto& cmd            = rhi->current_frame().command(cmdbuffer);
    auto& scratch_buffer = fetch_resource(rhi->buffers, scratch_buffer_handle);

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        throw GPUValidationError("Metal ray tracing is not supported on this device");
    }

    // transition to acceleration structure encoder
    cmd.transition_encoder(MetalCommandBuffer::ACCEL);
    if (!cmd.accel_encoder) {
        cmd.accel_encoder = [cmd.command_buffer accelerationStructureCommandEncoder];
    }

    for (auto& entry : entries) {
        auto& tlas = fetch_resource(rhi->tlases, entry.tlas);

        if (!tlas.descriptor || !tlas.tlas) {
            get_logger()->error("Invalid TLAS for building");
            throw GPUValidationError("Invalid TLAS for building");
        }

        // get instance descriptor from entry
        MTLInstanceAccelerationStructureDescriptor* as_desc =
            (MTLInstanceAccelerationStructureDescriptor*)tlas.descriptor;

        // create instance buffer from GPUTlasInstances
        if (!entry.instances.empty()) {
            NSUInteger instance_count = entry.instances.size();
            NSUInteger buffer_size    = instance_count * sizeof(MTLAccelerationStructureUserIDInstanceDescriptor);

            // allocate temporary buffer for instance descriptors
            id<MTLBuffer> instance_buffer = [rhi->device newBufferWithLength:buffer_size
                                                                     options:MTLResourceStorageModeShared];

            MTLAccelerationStructureUserIDInstanceDescriptor* descriptors =
                (MTLAccelerationStructureUserIDInstanceDescriptor*)[instance_buffer contents];

            for (NSUInteger i = 0; i < instance_count; ++i) {
                auto& src_instance   = entry.instances.at(i);
                auto& dst_descriptor = descriptors[i];

                // copy transform (3x4 matrix in row-major order)
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 4; ++col) {
                        dst_descriptor.transformationMatrix.columns[col][row] = src_instance.transform[col][row];
                    }
                }

                dst_descriptor.mask    = src_instance.mask;
                dst_descriptor.userID  = src_instance.custom_data & 0xFFFFFF; // 24-bit custom index
                dst_descriptor.options = MTLAccelerationStructureInstanceOptionNone;

                // get BLAS reference
                if (src_instance.blas.valid()) {
                    auto& blas                                = fetch_resource(rhi->blases, src_instance.blas);
                    dst_descriptor.accelerationStructureIndex = 0; // Index in acceleration structure array
                }
            }

            as_desc.instanceDescriptorBuffer       = instance_buffer;
            as_desc.instanceDescriptorBufferOffset = 0;
            as_desc.instanceCount                  = instance_count;

            // collect all referenced BLAS acceleration structures
            NSMutableArray<id<MTLAccelerationStructure>>* blas_array = [NSMutableArray new];
            for (auto& src_instance : entry.instances) {
                if (src_instance.blas.valid()) {
                    auto& blas = fetch_resource(rhi->blases, src_instance.blas);
                    [blas_array addObject:blas.blas];
                }
            }
            as_desc.instancedAccelerationStructures = blas_array;
        }

        // build the acceleration structure
        [cmd.accel_encoder buildAccelerationStructure:tlas.tlas
                                           descriptor:as_desc
                                        scratchBuffer:scratch_buffer.buffer
                                  scratchBufferOffset:0];
    }
}

void cmd::build_blases(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle scratch_buffer_handle, GPUBlasBuildEntries entries)
{
    auto  rhi            = get_rhi();
    auto& cmd            = rhi->current_frame().command(cmdbuffer);
    auto& scratch_buffer = fetch_resource(rhi->buffers, scratch_buffer_handle);

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        throw GPUValidationError("Metal ray tracing is not supported on this device");
    }

    // transition to acceleration structure encoder
    cmd.transition_encoder(MetalCommandBuffer::ACCEL);
    if (!cmd.accel_encoder) {
        cmd.accel_encoder = [cmd.command_buffer accelerationStructureCommandEncoder];
    }

    for (auto& entry : entries) {
        auto& blas = fetch_resource(rhi->blases, entry.blas);

        if (!blas.descriptor || !blas.blas) {
            get_logger()->error("Invalid BLAS for building");
            throw GPUValidationError("Invalid BLAS for building");
        }

        MTLPrimitiveAccelerationStructureDescriptor* as_desc =
            (MTLPrimitiveAccelerationStructureDescriptor*)blas.descriptor;

        // update geometry descriptors with actual buffer references
        NSArray<MTLAccelerationStructureTriangleGeometryDescriptor*>* geom_descs =
            (NSArray<MTLAccelerationStructureTriangleGeometryDescriptor*>*)as_desc.geometryDescriptors;

        // access the triangle geometries from the union based on type
        if (entry.geometries.type == GPUBlasType::TRIANGLE) {
            auto& triangles = entry.geometries.triangles;
            for (NSUInteger i = 0; i < triangles.size() && i < geom_descs.count; ++i) {
                auto&                                               triangle_geom = triangles.at(i);
                MTLAccelerationStructureTriangleGeometryDescriptor* geom          = geom_descs[i];

                auto& vertex_buffer     = fetch_resource(rhi->buffers, triangle_geom.vertex_buffer);
                geom.vertexBuffer       = vertex_buffer.buffer;
                geom.vertexBufferOffset = triangle_geom.first_vertex * triangle_geom.vertex_stride;
                geom.vertexStride       = triangle_geom.vertex_stride;

                if (triangle_geom.index_buffer.valid()) {
                    auto& index_buffer = fetch_resource(rhi->buffers, triangle_geom.index_buffer);
                    geom.indexBuffer   = index_buffer.buffer;
                    // Calculate index buffer offset based on index format
                    NSUInteger index_size  = (triangle_geom.size.index_format == GPUIndexFormat::UINT16) ? 2 : 4;
                    geom.indexBufferOffset = triangle_geom.first_index * index_size;
                }

                if (triangle_geom.transform_buffer.valid()) {
                    auto& transform_buffer                = fetch_resource(rhi->buffers, triangle_geom.transform_buffer);
                    geom.transformationMatrixBuffer       = transform_buffer.buffer;
                    geom.transformationMatrixBufferOffset = triangle_geom.transform_buffer_offset;
                }
            }
        }

        // build the acceleration structure
        [cmd.accel_encoder buildAccelerationStructure:blas.blas
                                           descriptor:as_desc
                                        scratchBuffer:scratch_buffer.buffer
                                  scratchBufferOffset:0];
    }
}

void cmd::copy_blas(GPUCommandEncoderHandle cmdbuffer, GPUBlasHandle src_blas_handle, GPUBlasHandle dst_blas_handle)
{
    auto  rhi      = get_rhi();
    auto& cmd      = rhi->current_frame().command(cmdbuffer);
    auto& src_blas = fetch_resource(rhi->blases, src_blas_handle);
    auto& dst_blas = fetch_resource(rhi->blases, dst_blas_handle);

    // check device support
    if (![rhi->device supportsRaytracing]) {
        get_logger()->error("Metal ray tracing is not supported on this device");
        throw GPUValidationError("Metal ray tracing is not supported on this device");
    }

    // transition to acceleration structure encoder
    cmd.transition_encoder(MetalCommandBuffer::ACCEL);
    if (!cmd.accel_encoder) {
        cmd.accel_encoder = [cmd.command_buffer accelerationStructureCommandEncoder];
    }

    [cmd.accel_encoder copyAccelerationStructure:src_blas.blas
                         toAccelerationStructure:dst_blas.blas];
}
