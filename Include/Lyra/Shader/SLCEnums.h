#pragma once

#ifndef LYRA_LYRA_SHADER_SLCENUMS_H
#define LYRA_LYRA_SHADER_SLCENUMS_H

#include <Lyra/Common/Stdint.h>

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

#endif // LYRA_LYRA_SHADER_SLCENUMS_H
