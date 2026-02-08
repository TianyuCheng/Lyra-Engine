#pragma once

#ifndef LYRA_LIBRARY_RENDER_RHI_INITS_H
#define LYRA_LIBRARY_RENDER_RHI_INITS_H

#include <Lyra/Render/RHIEnums.h>
#include <Lyra/Render/RHIUtils.h>
#include <Lyra/Render/RHIDescs.h>

namespace lyra
{
    struct TransitionState
    {
        GPUBarrierLayout layout;
        GPUBarrierSync   sync;
        GPUBarrierAccess access;
    };

    TransitionState undefined_state();
    TransitionState shader_resource_state(GPUBarrierSync sync);
    TransitionState unordered_access_state(GPUBarrierSync sync);
    TransitionState present_src_state();
    TransitionState color_attachment_state();
    TransitionState depth_stencil_attachment_state();
    TransitionState copy_src_state();
    TransitionState copy_dst_state();

    GPUTextureBarrier state_transition(
        GPUTextureHandle       texture,
        const TransitionState& src_state,
        const TransitionState& dst_state,
        uint32_t               base_array_layer = 0,
        uint32_t               array_layers     = 1,
        uint32_t               base_mip_level   = 0,
        uint32_t               mip_level_count  = 1);

} // namespace lyra

#endif // LYRA_LIBRARY_RENDER_RHI_INITS_H
