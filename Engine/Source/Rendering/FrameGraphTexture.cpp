#include <Lyra/Utilities/Macros.h>
#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

void FrameGraphTexture::pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    MAYBE_UNUSED(pass);

    TextureTransitionState dst_state{};
    switch (op) {
        case FrameGraphReadOp::NOP:
            return;
        case FrameGraphReadOp::READ:
            dst_state = is_depth_stencil_format(format)
                            ? depth_stencil_read_texture_state()
                            : unordered_access_texture_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::SAMPLE:
            dst_state = shader_resource_texture_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::PRESENT:
            dst_state = present_src_texture_state();
            break;
    }

    if (src_state == dst_state) {
        return;
    }

    if (barriers) {
        barriers->add_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    } else if (context && context->cmdlist.handle.valid()) {
        context->cmdlist.resource_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    }
    state = dst_state;
}

void FrameGraphTexture::pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    MAYBE_UNUSED(pass);

    TextureTransitionState dst_state{};
    switch (op) {
        case FrameGraphWriteOp::NOP:
            return;
        case FrameGraphWriteOp::WRITE:
            dst_state = unordered_access_texture_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphWriteOp::RENDER:
            dst_state = is_depth_stencil_format(format)
                            ? depth_stencil_attachment_texture_state()
                            : color_attachment_texture_state();
            break;
    }
    if (barriers) {
        barriers->add_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    } else if (context && context->cmdlist.handle.valid()) {
        context->cmdlist.resource_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    }
    state = dst_state;
}

void FrameGraphBuffer::pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    MAYBE_UNUSED(pass);

    BufferTransitionState dst_state{};
    switch (op) {
        case FrameGraphReadOp::NOP:
            return;
        case FrameGraphReadOp::READ:
            dst_state = unordered_access_buffer_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::SAMPLE:
            dst_state = shader_resource_buffer_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::PRESENT:
            return;
    }

    if (src_state == dst_state) {
        return;
    }

    if (barriers) {
        barriers->add_barrier(state_transition(buffer, src_state, dst_state, 0, size));
    } else if (context && context->cmdlist.handle.valid()) {
        context->cmdlist.resource_barrier(state_transition(buffer, src_state, dst_state, 0, size));
    }
    state = dst_state;
}

void FrameGraphBuffer::pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    MAYBE_UNUSED(pass);

    BufferTransitionState dst_state{};
    switch (op) {
        case FrameGraphWriteOp::NOP:
            return;
        case FrameGraphWriteOp::WRITE:
        case FrameGraphWriteOp::RENDER:
            dst_state = unordered_access_buffer_state(GPUBarrierSync::ALL);
            break;
    }

    if (barriers) {
        barriers->add_barrier(state_transition(buffer, src_state, dst_state, 0, size));
    } else if (context && context->cmdlist.handle.valid()) {
        context->cmdlist.resource_barrier(state_transition(buffer, src_state, dst_state, 0, size));
    }
    state = dst_state;
}
