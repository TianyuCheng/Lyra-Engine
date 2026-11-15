#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_API_H
#define LYRA_ENGINE_RENDERING_RENDERER_API_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RendererTypes.h>

namespace lyra
{
    /**
     * @brief common auxiliary render targets published by renderers for engine passes and editor tools.
     */
    struct RenderFrameOutputs
    {
        FrameGraph::TextureHandle color;
        FrameGraph::TextureHandle depth;
        FrameGraph::TextureHandle normal;
        FrameGraph::TextureHandle velocity;
        FrameGraph::TextureHandle picking;
    };

    /**
     * @brief frame context provided to a renderer plugin during FrameGraph pass recording.
     */
    struct RenderFrameContext
    {
        // FrameGraph pass recorder
        FrameGraphBuilder& builder;

        // scene & primary camera view
        const RenderView&  view;
        const RenderScene& scene;

        // primary targets
        FrameGraph::TextureHandle color_target;
        FrameGraph::TextureHandle depth_target;

        // primary formats
        GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT;

        // resolutions (decouples internal render resolution from presentation target for DRS/FSR)
        GPUExtent2D render_extent  = {0, 0};
        GPUExtent2D display_extent = {0, 0};

        // temporal & reconstruction state
        RenderFrameTime time          = {};
        Vector2         camera_jitter = {0.0f, 0.0f};

        // optional output channel bundle (replaces fragile one-off pointers)
        RenderFrameOutputs* outputs = nullptr;

        // engine service locator (borrow persistent infrastructure)
        Services* services = nullptr;
    };

    /**
     * @brief C-style ABI interface for pluggable renderer implementations.
     */
    struct RendererAPI
    {
        // identification & capabilities
        auto (*get_name)() -> CString;
        auto (*get_capabilities)() -> RendererCapabilities;

        // lifecycle
        bool (*create_renderer)(RendererHandle& out_handle, const RendererDescriptor& descriptor);
        void (*delete_renderer)(RendererHandle handle);

        // configuration
        void (*set_settings)(RendererHandle handle, const RendererSettings& settings);
        void (*get_settings)(RendererHandle handle, RendererSettings& out_settings);
        void (*get_statistics)(RendererHandle handle, RendererStats& out_stats);

        // frame dimension
        void (*resize)(RendererHandle handle, uint width, uint height);

        // frame execution
        auto (*render)(RendererHandle handle, const RenderFrameContext& ctx) -> FrameGraph::TextureHandle;

        // optional debug gui hook
        void (*draw_debug_panel)(RendererHandle handle);
    };

    using RendererPlugin = Plugin<RendererAPI>;

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_API_H
