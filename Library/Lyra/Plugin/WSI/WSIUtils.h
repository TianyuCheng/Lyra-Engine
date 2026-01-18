#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_WSI_UTILS_H
#define LYRA_LIBRARY_PLUGIN_WSI_UTILS_H

#include <Lyra/Common/Function.h>
#include <Lyra/Common/BitFlags.h>
#include <Lyra/Plugin/WSI/WSIEnums.h>

namespace lyra
{
    using WindowFlags = BitFlags<WindowFlag>;

    using WindowCallback = Delegate<void(WindowEvent)>;

    struct MonitorInfo
    {
        int   monitor_pos_x;
        int   monitor_pos_y;
        uint  monitor_width;
        uint  monitor_height;
        int   workarea_pos_x;
        int   workarea_pos_y;
        uint  workarea_width;
        uint  workarea_height;
        float dpi_scale_x;
        float dpi_scale_y;
    };

    struct WindowHandle
    {
        void* window = nullptr;
        void* native = nullptr;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_WSI_UTILS_H
