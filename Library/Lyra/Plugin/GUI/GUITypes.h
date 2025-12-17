#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_GUI_TYPES_H
#define LYRA_LIBRARY_PLUGIN_GUI_TYPES_H

#include <imgui.h>
#include <imgui_internal.h>

#include <Lyra/Common/Plugin.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/Plugin/GUI/GUIAPI.h>

namespace lyra
{
    using GUIRenderPlugin = Plugin<GUIRenderAPI>;

    struct GUITexture
    {
        uint texid = 0;
    };

    struct GUIRenderer
    {
        // implicit conversion
        GUIRenderer() : handle() {}
        GUIRenderer(GUIHandle handle) : handle(handle) {}

        // implicit conversion
        operator GUIHandle() { return handle; }
        operator GUIHandle() const { return handle; }

        static auto api() -> GUIRenderAPI*;

        static auto init(const GUIDescriptor& descriptor) -> OwnedResource<GUIRenderer>;

        void destroy() const { GUIRenderer::api()->delete_gui(handle); }

        void update() const { GUIRenderer::api()->update_gui(handle); }

        void resize() const { GUIRenderer::api()->resize_gui(handle); }

        void new_frame() const { GUIRenderer::api()->new_frame(handle); }

        void end_frame() const { GUIRenderer::api()->end_frame(handle); }

        // GUIRenderer will be responsible for managing GPUTexture / GPUTextureView deletion upon delete_image
        // ownership of GPUTextureHandle and GPUTextureViewHandle will be taken over
        auto create_texture(GPUTextureHandle texture, GPUTextureViewHandle texview) const -> GUITexture
        {
            uint texid = GUIRenderer::api()->create_texture(handle, texture, texview);
            return GUITexture{texid};
        }

        // GUIRenderer will be responsible for managing GPUTextureView deletion upon delete_texture
        auto create_texture(GPUTextureViewHandle texview) const -> GUITexture
        {
            uint texid = GUIRenderer::api()->create_texture(handle, GPUTextureHandle(), texview);
            return GUITexture{texid};
        }

        void delete_texture(GUITexture texture) const
        {
            return GUIRenderer::api()->delete_texture(handle, texture.texid);
        }

        void render_main_viewport(GPUCommandBuffer cmdbuffer, GPUTextureViewHandle backbuffer) const
        {
            GUIRenderer::api()->render_main_viewport(handle, cmdbuffer, backbuffer);
        }

        void render_side_viewports() const
        {
            GUIRenderer::api()->render_side_viewports(handle);
        }

        // retrieve the created ImGuiContext*
        ImGuiContext* context() const
        {
            return reinterpret_cast<ImGuiContext*>(GUIRenderer::api()->get_context(handle));
        }

        // apply_context() is required to be called from user application
        // because currently we created ImGuiContext* in Lyra-Engine.dll.
        // User application will need ImGuiContext* if it intends to call
        // any ImGui functions.
        FORCE_INLINE void apply_context()
        {
            ImGui::SetCurrentContext(context());
        }

    private:
        GUIHandle handle;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_GUI_TYPES_H
