#pragma once

#ifndef LYRA_MODULES_RENDERING_FORWARD_RENDERER_H
#define LYRA_MODULES_RENDERING_FORWARD_RENDERER_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/RendererAPI.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/Passes/Pass.h>

namespace lyra
{
    /**
     * @brief Forward and Forward+ scene renderer composing modular engine passes.
     */
    class ForwardRenderer
    {
    public:
        ForwardRenderer()  = default;
        ~ForwardRenderer() = default;

        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat color_format = GPUTextureFormat::RGBA8UNORM,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);
        void shutdown();

        auto render(const RenderFrameContext& ctx) -> FrameGraphHandle<FrameGraphTexture>;
        void resize(uint width, uint height);

        void set_settings(const RendererSettings& s) { settings = s; }
        void get_settings(RendererSettings& out) const { out = settings; }
        void get_statistics(RendererStats& out) const { out = stats; }

        [[nodiscard]] bool             is_initialized() const { return initialized; }
        [[nodiscard]] GPUTextureFormat get_color_format() const { return cached_color_format; }
        [[nodiscard]] GPUTextureFormat get_depth_format() const { return cached_depth_format; }

        void draw_debug_panel();

    private:
        GPUDevice        device;
        Compiler         compiler;
        RendererSettings settings;
        RendererStats    stats;
        GPUTextureFormat cached_color_format = GPUTextureFormat::RGBA8UNORM;
        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;

        // composed engine passes
        passes::ForwardPass         forward_pass;
        passes::DepthPrepass        depth_prepass;
        passes::ShadowMapPass       shadow_pass;
        passes::LightCullingPass    light_culling_pass;
        passes::SkyboxPass          skybox_pass;
        passes::TonemappingPass     tonemap_pass;
        Ref<GeometryBindGroupCache> geometry_bind_group_cache;

        bool initialized = false;
    };

} // namespace lyra

#endif // LYRA_MODULES_RENDERING_FORWARD_RENDERER_H
