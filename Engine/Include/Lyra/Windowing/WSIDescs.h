#pragma once

#ifndef LYRA_ENGINE_WINDOWING_WSIDESCS_H
#define LYRA_ENGINE_WINDOWING_WSIDESCS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Windowing/WSIEnums.h>
#include <Lyra/Windowing/WSIUtils.h>

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

#endif // LYRA_ENGINE_WINDOWING_WSIDESCS_H
