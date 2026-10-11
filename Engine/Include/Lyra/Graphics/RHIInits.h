#pragma once

#ifndef LYRA_ENGINE_GRAPHICS_RHIINITS_H
#define LYRA_ENGINE_GRAPHICS_RHIINITS_H

#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Graphics/RHIDescs.h>

namespace lyra
{
    // =========================================================================
    // texture transitions
    // =========================================================================

    struct TextureTransitionState
    {
        GPUBarrierLayout layout;
        GPUBarrierSync   sync;
        GPUBarrierAccess access;

        bool operator==(const TextureTransitionState& other) const = default;
        bool operator!=(const TextureTransitionState& other) const = default;
    };

    TextureTransitionState undefined_texture_state();
    TextureTransitionState shader_resource_texture_state(GPUBarrierSync sync);
    TextureTransitionState unordered_access_texture_state(GPUBarrierSync sync);
    TextureTransitionState present_src_texture_state();
    TextureTransitionState color_attachment_texture_state();
    TextureTransitionState depth_stencil_attachment_texture_state();
    TextureTransitionState depth_stencil_read_texture_state(GPUBarrierSync sync = GPUBarrierSync::DEPTH_STENCIL);
    TextureTransitionState copy_src_texture_state();
    TextureTransitionState copy_dst_texture_state();
    TextureTransitionState resolve_src_texture_state();
    TextureTransitionState resolve_dst_texture_state();

    GPUTextureBarrier state_transition(
        GPUTextureHandle              texture,
        const TextureTransitionState& src_state,
        const TextureTransitionState& dst_state,
        uint32_t                      base_array_layer = 0,
        uint32_t                      array_layers     = 1,
        uint32_t                      base_mip_level   = 0,
        uint32_t                      mip_level_count  = 1);

    // =========================================================================
    // buffer transitions
    // =========================================================================

    struct BufferTransitionState
    {
        GPUBarrierSync   sync;
        GPUBarrierAccess access;

        bool operator==(const BufferTransitionState& other) const = default;
        bool operator!=(const BufferTransitionState& other) const = default;
    };

    BufferTransitionState undefined_buffer_state();
    BufferTransitionState shader_resource_buffer_state(GPUBarrierSync sync);
    BufferTransitionState unordered_access_buffer_state(GPUBarrierSync sync);
    BufferTransitionState copy_src_buffer_state();
    BufferTransitionState copy_dst_buffer_state();
    BufferTransitionState vertex_buffer_state(GPUBarrierSync sync = GPUBarrierSync::VERTEX_SHADING);
    BufferTransitionState index_buffer_state();
    BufferTransitionState indirect_argument_state();
    BufferTransitionState uniform_buffer_state(GPUBarrierSync sync = GPUBarrierSync::ALL_SHADING);
    BufferTransitionState acceleration_structure_build_state();
    BufferTransitionState acceleration_structure_read_state(GPUBarrierSync sync = GPUBarrierSync::RAYTRACING);

    GPUBufferBarrier state_transition(
        GPUBufferHandle              buffer,
        const BufferTransitionState& src_state,
        const BufferTransitionState& dst_state,
        GPUSize64                    offset = 0,
        GPUSize64                    size   = 0);

} // namespace lyra

#endif // LYRA_ENGINE_GRAPHICS_RHIINITS_H
