#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_API_H
#define LYRA_ENGINE_RENDERING_RENDERER_API_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>

namespace lyra
{
    /**
     * @brief C-style ABI interface for pluggable renderer implementations.
     */
    struct RendererAPI
    {
        // identification & capabilities
        CString (*get_name)();
        RendererCapabilities (*get_capabilities)();

        // lifecycle
        bool (*create_renderer)(RendererHandle& out_handle, const RendererDescriptor& descriptor);
        void (*delete_renderer)(RendererHandle handle);

        // configuration & target dimensions
        void (*resize)(RendererHandle handle, uint width, uint height);
        void (*set_settings)(RendererHandle handle, const RendererSettings& settings);
        void (*get_settings)(RendererHandle handle, RendererSettings& out_settings);
        void (*get_statistics)(RendererHandle handle, RendererStats& out_stats);

        // frame execution
        void (*render)(
            RendererHandle          handle,
            const RenderView&       view,
            const RenderScene&      scene,
            const Backbuffer&       target,
            GPUCommandEncoderHandle command_encoder);

        // optional debug gui hook
        void (*draw_debug_panel)(RendererHandle handle);
    };

    using RendererPlugin        = Plugin<RendererAPI>;
    using BuiltinRendererPlugin = BuiltinPlugin<RendererAPI>;

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_API_H
