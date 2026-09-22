#pragma once

#ifndef LYRA_LYRA_WINDOW_WSIDESCS_H
#define LYRA_LYRA_WINDOW_WSIDESCS_H

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

#endif // LYRA_LYRA_WINDOW_WSIDESCS_H
