#pragma once

#ifndef LYRA_LIBRARY_WINDOW_WSI_DESCS_H
#define LYRA_LIBRARY_WINDOW_WSI_DESCS_H

#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Window/WSIEnums.h>
#include <Lyra/Window/WSIUtils.h>

namespace lyra
{
    struct WindowDescriptor
    {
        CString     title  = "Lyra Engine";
        uint        width  = 1920;
        uint        height = 1080;
        WindowFlags flags  = WindowFlag::DECORATED;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_WINDOW_WSI_DESCS_H
