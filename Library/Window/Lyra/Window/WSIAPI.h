#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_WSI_API_H
#define LYRA_LIBRARY_PLUGIN_WSI_API_H

#include <Lyra/Window/WSIEnums.h>
#include <Lyra/Window/WSIUtils.h>
#include <Lyra/Window/WSIDescs.h>
#include <Lyra/Window/WSIState.h>

namespace lyra
{
    struct WindowAPI
    {
        // api name
        CString (*get_api_name)();

        void (*list_monitors)(uint& count, MonitorInfo* monitors);

        bool (*create_window)(const WindowDescriptor& desc, WindowHandle& window);
        void (*delete_window)(WindowHandle window);

        void (*set_window_pos)(WindowHandle window, int x, int y);
        void (*get_window_pos)(WindowHandle window, int& x, int& y);

        void (*set_window_size)(WindowHandle window, uint width, uint height);
        void (*get_window_size)(WindowHandle window, uint& width, uint& height);

        void (*set_window_focus)(WindowHandle window);
        bool (*get_window_focus)(WindowHandle window);

        void (*set_window_alpha)(WindowHandle window, float alpha);
        float (*get_window_alpha)(WindowHandle window);

        void (*set_window_title)(WindowHandle window, CString title);
        CString (*get_window_title)(WindowHandle window);

        void (*set_clipboard_text)(WindowHandle window, CString text);
        CString (*get_clipboard_text)(WindowHandle window);

        void (*get_content_scale)(WindowHandle window, float& xscale, float& yscale);
        void (*get_framebuffer_scale)(WindowHandle window, float& xscale, float& yscale);

        bool (*get_window_minimized)(WindowHandle window);

        void (*bind_window_callback)(WindowHandle window, WindowCallback callback);

        void (*query_input_events)(WindowHandle window, WindowInputQuery& query);

        void (*show_window)(WindowHandle window);

        void (*run_in_loop)();
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_WSI_API_H
