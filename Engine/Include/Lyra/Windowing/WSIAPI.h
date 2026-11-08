#pragma once

#ifndef LYRA_ENGINE_WINDOWING_WSIAPI_H
#define LYRA_ENGINE_WINDOWING_WSIAPI_H

#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/Windowing/WSIUtils.h>
#include <Lyra/Windowing/WSIDescs.h>
#include <Lyra/Windowing/WSIState.h>

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
        auto (*get_window_alpha)(WindowHandle window) -> float;

        void (*set_window_title)(WindowHandle window, CString title);
        auto (*get_window_title)(WindowHandle window) -> CString;

        void (*set_clipboard_text)(WindowHandle window, CString text);
        auto (*get_clipboard_text)(WindowHandle window) -> CString;

        void (*set_cursor_mode)(WindowHandle window, CursorMode mode);
        auto (*get_cursor_mode)(WindowHandle window) -> CursorMode;
        void (*set_cursor_pos)(WindowHandle window, float xpos, float ypos);
        void (*get_cursor_pos)(WindowHandle window, float& xpos, float& ypos);

        void (*get_content_scale)(WindowHandle window, float& xscale, float& yscale);
        void (*get_framebuffer_scale)(WindowHandle window, float& xscale, float& yscale);

        bool (*get_window_minimized)(WindowHandle window);

        void (*bind_window_callback)(WindowHandle window, WindowCallback callback);

        void (*query_input_events)(WindowHandle window, WindowInputQuery& query);

        void (*show_window)(WindowHandle window);

        void (*run_in_loop)();
    };

} // namespace lyra

#endif // LYRA_ENGINE_WINDOWING_WSIAPI_H
