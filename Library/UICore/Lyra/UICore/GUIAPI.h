#pragma once

#ifndef LYRA_LIBRARY_UICORE_GUI_API_H
#define LYRA_LIBRARY_UICORE_GUI_API_H

#include <Lyra/Shader/SLCTypes.h>
#include <Lyra/Render/RHITypes.h>
#include <Lyra/UICore/GUIEnums.h>

namespace lyra
{
    struct GUI;

    struct GUIDescriptor
    {
        WindowHandle     window;  // primary window
        GPUSurfaceHandle surface; // primary surface/swapchain
        CompilerHandle   compiler;

        // options
        bool docking   = false;
        bool viewports = false;
    };

    using GUIHandle = TypedPointerHandle<GUI>;

    using GUITextureHandle = TypedEnumHandle<GUIObject, GUIObject::TEXTURE, uint64_t>;

    struct GUIAPI
    {
        // api name
        CString (*get_api_name)();

        bool (*create_gui)(GUIHandle& gui, const GUIDescriptor& descriptor);
        void (*delete_gui)(GUIHandle gui);
        void (*update_gui)(GUIHandle gui);
        void (*resize_gui)(GUIHandle gui);

        void (*new_frame)(GUIHandle gui);
        void (*end_frame)(GUIHandle gui);

        void* (*get_context)(GUIHandle gui);

        auto (*create_texture)(GUIHandle gui, GPUTextureHandle texture, GPUTextureViewHandle view) -> GUITextureHandle;
        void (*delete_texture)(GUIHandle gui, GUITextureHandle texid);

        void (*render_main_viewport)(GUIHandle gui, GPUCommandEncoderHandle cmdbuffer, GPUTextureViewHandle backbuffer);
        void (*render_side_viewports)(GUIHandle gui);
    };

} // namespace lyra

#endif // LYRA_LIBRARY_UICORE_GUI_API_H
