#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_SLC_ENUMS_H
#define LYRA_LIBRARY_PLUGIN_SLC_ENUMS_H

#include <Lyra/Common/Stdint.h>

namespace lyra
{

    enum struct CompileFlag : uint
    {
        NONE    = 0x0,
        DEBUG   = 0x1,
        REFLECT = 0x2,
    };

    enum struct CompileTarget : uint
    {
        MSL,
        DXIL,
        SPIRV,
    };

} // namespace lyra

#endif // LYRA_LIBRARY_PLUGIN_SLC_ENUMS_H
