#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H
#define LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/RendererAPI.h>

namespace lyra
{
    struct AppContext;

    /**
     * @brief manages the lifecycle, dynamic swapping, and execution of renderer plugins.
     */
    class RendererManager
    {
    public:
        explicit RendererManager();
        virtual ~RendererManager();

        RendererManager(const RendererManager&)            = delete;
        RendererManager& operator=(const RendererManager&) = delete;

        RendererManager(RendererManager&&) noexcept            = default;
        RendererManager& operator=(RendererManager&&) noexcept = default;

        /**
         * @brief initialize renderer manager with graphics device and compiler.
         */
        bool init(AppContext& context, const RendererDescriptor& desc);

        /**
         * @brief safely shut down the active renderer and unload plugins.
         */
        void shutdown(AppContext& context);

        /**
         * @brief load a renderer plugin by module name (e.g. "lyra-forward").
         */
        bool load_renderer(AppContext& context, StringView plugin_name);

        /**
         * @brief load a renderer plugin by backend enum.
         */
        bool load_renderer(AppContext& context, RendererBackend backend);

        /**
         * @brief dynamically swap the active renderer at runtime.
         *        Drains in-flight GPU frames, shuts down previous renderer,
         *        and instantiates the new renderer with the current viewport size.
         */
        bool switch_renderer(AppContext& context, StringView plugin_name);

        /**
         * @brief dynamically swap the active renderer at runtime by backend enum.
         */
        bool switch_renderer(AppContext& context, RendererBackend backend);

        /**
         * @brief notify the active renderer of viewport resize.
         */
        void resize(uint width, uint height);

        /**
         * @brief record rendering passes into a provided FrameGraphBuilder with logical targets.
         */
        auto render(const RenderFrameContext& ctx) -> FrameGraph::TextureHandle;

        /**
         * @brief execute the active renderer for the current frame using its managed FrameGraph.
         */
        void render(
            AppContext&             context,
            const RenderView&       view,
            const RenderScene&      scene,
            const Backbuffer&       target,
            GPUCommandEncoderHandle command_encoder);

        /**
         * @brief access the shared FrameGraph allocator.
         */
        auto get_allocator() -> FrameGraphAllocator& { return allocator; }
        auto get_allocator() const -> const FrameGraphAllocator& { return allocator; }

        /**
         * @brief render imgui debug controls if supported by active renderer.
         */
        void draw_debug_panel();

        [[nodiscard]] bool has_active_renderer() const { return active_handle.valid() && current_plugin.is_loaded(); }
        [[nodiscard]] auto get_active_name() const -> StringView { return active_name; }
        [[nodiscard]] auto get_active_backend() const -> Optional<RendererBackend> { return active_backend; }

        auto get_active_api() -> RendererAPI* { return current_plugin.get_api(); }
        auto get_active_api() const -> const RendererAPI* { return current_plugin.get_api(); }
        auto get_active_handle() const -> RendererHandle { return active_handle; }

        auto get_settings() -> RendererSettings& { return settings; }
        auto get_settings() const -> const RendererSettings& { return settings; }
        void set_settings(const RendererSettings& s);

        const RendererStats& get_stats() const { return stats; }

    private:
        RendererPlugin            current_plugin;
        RendererHandle            active_handle = {};
        String                    active_name;
        Optional<RendererBackend> active_backend;
        RendererDescriptor        descriptor = {};
        RendererSettings          settings   = {};
        RendererStats             stats      = {};
        FrameGraphAllocator       allocator;
        bool                      initialized = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H
