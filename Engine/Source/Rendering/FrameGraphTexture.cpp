#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

void FrameGraphTexture::pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    (void)pass;

    TransitionState dst_state{};
    switch (op) {
        case FrameGraphReadOp::NOP:
            return;
        case FrameGraphReadOp::READ:
            dst_state = unordered_access_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::SAMPLE:
            dst_state = shader_resource_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphReadOp::PRESENT:
            dst_state = present_src_state();
            break;
    }
    if (barriers) {
        barriers->add_texture_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    } else {
        context->cmdlist.resource_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    }
    state = dst_state;
}

void FrameGraphTexture::pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers)
{
    auto& src_state = state;

    TransitionState dst_state{};
    switch (op) {
        case FrameGraphWriteOp::NOP:
            return;
        case FrameGraphWriteOp::WRITE:
            dst_state = unordered_access_state(GPUBarrierSync::ALL);
            break;
        case FrameGraphWriteOp::RENDER:
            dst_state = is_depth_stencil_format(format)
                            ? depth_stencil_attachment_state()
                            : color_attachment_state();
            break;
    }
    if (barriers) {
        barriers->add_texture_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    } else {
        context->cmdlist.resource_barrier(state_transition(texture, src_state, dst_state, 0, layers, 0, levels));
    }
    state = dst_state;
}

void FrameGraphBuffer::pre_read(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphReadOp op, FrameGraphBarrierBatch* barriers)
{
    (void)context;
    (void)pass;
    (void)op;
    (void)barriers;
}

void FrameGraphBuffer::pre_write(FrameGraphContext* context, FrameGraphPass* pass, FrameGraphWriteOp op, FrameGraphBarrierBatch* barriers)
{
    (void)context;
    (void)pass;
    (void)op;
    (void)barriers;
}
