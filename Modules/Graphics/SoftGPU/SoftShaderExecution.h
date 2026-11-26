#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSHADEREXECUTION_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSHADEREXECUTION_H

#include <cstring>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>

namespace lyra::softgpu
{
    struct ComputeVaryingInput
    {
        struct GroupID
        {
            uint x = 0;
            uint y = 0;
            uint z = 0;
        };

        GroupID start_group_id{0, 0, 0};
        GroupID end_group_id{1, 1, 1};
    };

    struct VertexShaderParams
    {
        const void* in_vertex;   // pointer to vertex attributes struct or vertex ID
        void*       out_varying; // pointer to VS output struct
    };

    struct FragmentShaderParams
    {
        const void* in_varying;   // pointer to FS interpolated inputs struct
        void*       out_color;    // pointer to FS output struct / array
        uint*       is_discarded; // pointer to uint (1 if discarded, 0 otherwise)
    };

    struct ShadedVertexOutput
    {
        Vector4 position;
        float   varyings[16];
    };

    struct FragmentShaderInput
    {
        Vector4 position;
        float   varyings[16];
    };

    using SlangComputeCallable = void (*)(ComputeVaryingInput* varying_input, void* entry_point_params, void* uniform_state);

    inline void invoke_vertex_shader(void* func_ptr, const void* in_vertex, ShadedVertexOutput& out_vertex, void* uniform_state)
    {
        if (!func_ptr) return;
        ComputeVaryingInput varying;
        VertexShaderParams  params{in_vertex, &out_vertex};
        auto                func = reinterpret_cast<SlangComputeCallable>(func_ptr);
        func(&varying, &params, uniform_state);
    }

    inline bool invoke_fragment_shader(void* func_ptr, const FragmentShaderInput& in_frag, Vector4 out_colors[8], void* uniform_state)
    {
        if (!func_ptr) return false;
        ComputeVaryingInput  varying;
        uint                 is_discarded = 0;
        FragmentShaderParams params{&in_frag, out_colors, &is_discarded};
        auto                 func = reinterpret_cast<SlangComputeCallable>(func_ptr);
        func(&varying, &params, uniform_state);
        return is_discarded != 0;
    }
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTSHADEREXECUTION_H
