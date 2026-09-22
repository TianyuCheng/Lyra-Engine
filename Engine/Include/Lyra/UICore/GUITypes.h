#pragma once

#ifndef LYRA_LYRA_UICORE_GUITYPES_H
#define LYRA_LYRA_UICORE_GUITYPES_H

#include <Lyra/Common/Plugin.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Pointer.h>
#include <Lyra/UICore/GUIAPI.h>

namespace lyra
{
    struct GUITexture
    {
        GUITextureHandle texid;
    };

    struct GUIRenderer
    {
        // implicit conversion
        GUIRenderer() : handle() {}
        GUIRenderer(GUIHandle handle) : handle(handle) {}

        // implicit conversion
        operator GUIHandle() { return handle; }
        operator GUIHandle() const { return handle; }

        static auto api() -> GUIAPI*;

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
            auto texid = GUIRenderer::api()->create_texture(handle, texture, texview);
            return GUITexture{texid};
        }

        // GUIRenderer will be responsible for managing GPUTextureView deletion upon delete_texture
        auto create_texture(GPUTextureViewHandle texview) const -> GUITexture
        {
            auto texid = GUIRenderer::api()->create_texture(handle, GPUTextureHandle(), texview);
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

        // retrieve the raw context pointer
        // NOTE: for Dear ImGui, we will need to manually call ImGui::SetCurrentContext()
        // because our Dear ImGui is initialized in shared libraries. For any call
        // to Dear ImGui outside the shared libraries (e.g. user would like to call
        // some function not wrapped by GUIAPI, then we will need to properly extend
        // the Dear ImGui context to user code.
        template <typename T>
        T* context() const
        {
            return reinterpret_cast<T*>(GUIRenderer::api()->get_context(handle));
        }

    private:
        GUIHandle handle;
    };

} // namespace lyra

#endif // LYRA_LYRA_UICORE_GUITYPES_H
