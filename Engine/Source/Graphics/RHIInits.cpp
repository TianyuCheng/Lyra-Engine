#include <Lyra/Graphics/RHIInits.h>

using namespace lyra;

TextureTransitionState lyra::undefined_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::UNDEFINED;
    state.sync   = GPUBarrierSync::NONE;
    state.access = GPUBarrierAccess::NO_ACCESS;
    return state;
}

TextureTransitionState lyra::shader_resource_texture_state(GPUBarrierSync sync)
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::SHADER_RESOURCE;
    state.sync   = sync;
    state.access = GPUBarrierAccess::SHADER_RESOURCE;
    return state;
}

TextureTransitionState lyra::unordered_access_texture_state(GPUBarrierSync sync)
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::UNORDERED_ACCESS;
    state.sync   = sync;
    state.access = GPUBarrierAccess::UNORDERED_ACCESS;
    return state;
}

TextureTransitionState lyra::present_src_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::PRESENT;
    state.sync   = GPUBarrierSync::NONE;
    state.access = GPUBarrierAccess::NO_ACCESS;
    return state;
}

TextureTransitionState lyra::color_attachment_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::RENDER_TARGET;
    state.sync   = GPUBarrierSync::RENDER_TARGET;
    state.access = GPUBarrierAccess::RENDER_TARGET;
    return state;
}

TextureTransitionState lyra::depth_stencil_attachment_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::DEPTH_STENCIL_WRITE;
    state.sync   = GPUBarrierSync::DEPTH_STENCIL;
    state.access = GPUBarrierAccess::DEPTH_STENCIL_WRITE;
    return state;
}

TextureTransitionState lyra::copy_src_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::COPY_SOURCE;
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_SOURCE;
    return state;
}

TextureTransitionState lyra::copy_dst_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::COPY_DEST;
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_DEST;
    return state;
}

TextureTransitionState lyra::depth_stencil_read_texture_state(GPUBarrierSync sync)
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::DEPTH_STENCIL_READ;
    state.sync   = sync;
    state.access = GPUBarrierAccess::DEPTH_STENCIL_READ;
    return state;
}

TextureTransitionState lyra::resolve_src_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::RESOLVE_SOURCE;
    state.sync   = GPUBarrierSync::RESOLVE;
    state.access = GPUBarrierAccess::RESOLVE_SOURCE;
    return state;
}

TextureTransitionState lyra::resolve_dst_texture_state()
{
    TextureTransitionState state{};
    state.layout = GPUBarrierLayout::RESOLVE_DEST;
    state.sync   = GPUBarrierSync::RESOLVE;
    state.access = GPUBarrierAccess::RESOLVE_DEST;
    return state;
}

GPUTextureBarrier lyra::state_transition(
    GPUTextureHandle              texture,
    const TextureTransitionState& src_state,
    const TextureTransitionState& dst_state,
    uint32_t                      base_array_layer,
    uint32_t                      array_layers,
    uint32_t                      base_mip_level,
    uint32_t                      mip_level_count)
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

BufferTransitionState lyra::undefined_buffer_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::NONE;
    state.access = GPUBarrierAccess::NO_ACCESS;
    return state;
}

BufferTransitionState lyra::shader_resource_buffer_state(GPUBarrierSync sync)
{
    BufferTransitionState state{};
    state.sync   = sync;
    state.access = GPUBarrierAccess::SHADER_RESOURCE;
    return state;
}

BufferTransitionState lyra::unordered_access_buffer_state(GPUBarrierSync sync)
{
    BufferTransitionState state{};
    state.sync   = sync;
    state.access = GPUBarrierAccess::UNORDERED_ACCESS;
    return state;
}

BufferTransitionState lyra::copy_src_buffer_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_SOURCE;
    return state;
}

BufferTransitionState lyra::copy_dst_buffer_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::COPY;
    state.access = GPUBarrierAccess::COPY_DEST;
    return state;
}

BufferTransitionState lyra::vertex_buffer_state(GPUBarrierSync sync)
{
    BufferTransitionState state{};
    state.sync   = sync;
    state.access = GPUBarrierAccess::VERTEX_BUFFER;
    return state;
}

BufferTransitionState lyra::index_buffer_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::INDEX_INPUT;
    state.access = GPUBarrierAccess::INDEX_BUFFER;
    return state;
}

BufferTransitionState lyra::indirect_argument_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::EXECUTE_INDIRECT;
    state.access = GPUBarrierAccess::INDIRECT_ARGUMENT;
    return state;
}

BufferTransitionState lyra::uniform_buffer_state(GPUBarrierSync sync)
{
    BufferTransitionState state{};
    state.sync   = sync;
    state.access = GPUBarrierAccess::UNIFORM_BUFFER;
    return state;
}

BufferTransitionState lyra::acceleration_structure_build_state()
{
    BufferTransitionState state{};
    state.sync   = GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD;
    state.access = GPUBarrierAccess::ACCELERATION_STRUCTURE_WRITE;
    return state;
}

BufferTransitionState lyra::acceleration_structure_read_state(GPUBarrierSync sync)
{
    BufferTransitionState state{};
    state.sync   = sync;
    state.access = GPUBarrierAccess::ACCELERATION_STRUCTURE_READ;
    return state;
}

GPUBufferBarrier lyra::state_transition(
    GPUBufferHandle              buffer,
    const BufferTransitionState& src_state,
    const BufferTransitionState& dst_state,
    GPUSize64                    offset,
    GPUSize64                    size)
{
    auto barrier       = GPUBufferBarrier{};
    barrier.buffer     = buffer;
    barrier.offset     = offset;
    barrier.size       = size;
    barrier.src_access = src_state.access;
    barrier.dst_access = dst_state.access;
    barrier.src_sync   = src_state.sync;
    barrier.dst_sync   = dst_state.sync;
    return barrier;
}
