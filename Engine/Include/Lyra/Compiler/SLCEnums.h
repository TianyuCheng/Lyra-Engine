#pragma once

#ifndef LYRA_ENGINE_COMPILER_SLCENUMS_H
#define LYRA_ENGINE_COMPILER_SLCENUMS_H

#include <Lyra/Utilities/Stdint.h>

#undef DEBUG
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

#endif // LYRA_ENGINE_COMPILER_SLCENUMS_H
