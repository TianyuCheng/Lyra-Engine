#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H
#define LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Runtime/AppTypes.h>
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
         * @brief load a renderer plugin by module name (e.g. "lyra-renderer-forward").
         */
        bool load_renderer(StringView plugin_name, AppContext& context);

        /**
         * @brief dynamically swap the active renderer at runtime.
         *        Drains in-flight GPU frames, shuts down previous renderer,
         *        and instantiates the new renderer with the current viewport size.
         */
        bool switch_renderer(StringView plugin_name, AppContext& context);

        /**
         * @brief notify the active renderer of viewport resize.
         */
        void resize(uint width, uint height);

        /**
         * @brief execute the active renderer for the current frame.
         */
        void render(
            AppContext&             context,
            const RenderView&       view,
            const RenderScene&      scene,
            const Backbuffer&       target,
            GPUCommandEncoderHandle command_encoder);

        /**
         * @brief render imgui debug controls if supported by active renderer.
         */
        void draw_debug_panel();

        [[nodiscard]] bool has_active_renderer() const { return active_handle.valid() && current_plugin.is_loaded(); }
        [[nodiscard]] StringView get_active_name() const { return active_name; }

        RendererAPI*        get_active_api() { return current_plugin.get_api(); }
        const RendererAPI*  get_active_api() const { return current_plugin.get_api(); }
        RendererHandle      get_active_handle() const { return active_handle; }

        RendererSettings&       get_settings() { return settings; }
        const RendererSettings& get_settings() const { return settings; }
        void set_settings(const RendererSettings& s);

        const RendererStats& get_stats() const { return stats; }

    private:
        RendererPlugin     current_plugin;
        RendererHandle     active_handle = {};
        String             active_name;
        RendererDescriptor descriptor    = {};
        RendererSettings   settings      = {};
        RendererStats      stats         = {};
        bool               initialized   = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDERER_MANAGER_H
