#include <Lyra/Render/RHIInits.h>

using namespace lyra;

TransitionState lyra::undefined_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::UNDEFINED;
    state.sync   = GPUBarrierSync::NONE;
    state.access = GPUBarrierAccess::NO_ACCESS;
    return state;
}

TransitionState lyra::shader_resource_state(GPUBarrierSync sync)
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::SHADER_RESOURCE;
    state.sync   = sync;
    state.access = GPUBarrierAccess::SHADER_RESOURCE;
    return state;
}

TransitionState lyra::unordered_access_state(GPUBarrierSync sync)
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::UNORDERED_ACCESS;
    state.sync   = sync;
    state.access = GPUBarrierAccess::UNORDERED_ACCESS;
    return state;
}

TransitionState lyra::present_src_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::PRESENT;
    state.sync   = GPUBarrierSync::NONE;
    state.access = GPUBarrierAccess::NO_ACCESS;
    return state;
}

TransitionState lyra::color_attachment_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::RENDER_TARGET;
    state.sync   = GPUBarrierSync::RENDER_TARGET;
    state.access = GPUBarrierAccess::RENDER_TARGET;
    return state;
}

TransitionState lyra::depth_stencil_attachment_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::DEPTH_STENCIL_WRITE;
    state.sync   = GPUBarrierSync::DEPTH_STENCIL;
    state.access = GPUBarrierAccess::DEPTH_STENCIL_WRITE;
    return state;
}

TransitionState lyra::copy_src_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::COPY_SOURCE;
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_SOURCE;
    return state;
}

TransitionState lyra::copy_dst_state()
{
    TransitionState state{};
    state.layout = GPUBarrierLayout::COPY_DEST;
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_DEST;
    return state;
}

GPUTextureBarrier lyra::state_transition(
    GPUTextureHandle       texture,
    const TransitionState& src_state,
    const TransitionState& dst_state,
    uint32_t               base_array_layer,
    uint32_t               array_layers,
    uint32_t               base_mip_level,
    uint32_t               mip_level_count)
{
    auto barrier                          = GPUTextureBarrier{};
    barrier.texture                       = texture;
    barrier.subresources.base_array_layer = base_array_layer;
    barrier.subresources.base_mip_level   = base_mip_level;
    barrier.subresources.array_layers     = array_layers;
    barrier.subresources.mip_level_count  = mip_level_count;
    barrier.src_layout                    = src_state.layout;
    barrier.dst_layout                    = dst_state.layout;
    barrier.src_access                    = src_state.access;
    barrier.dst_access                    = dst_state.access;
    barrier.src_sync                      = src_state.sync;
    barrier.dst_sync                      = dst_state.sync;
    return barrier;
}
