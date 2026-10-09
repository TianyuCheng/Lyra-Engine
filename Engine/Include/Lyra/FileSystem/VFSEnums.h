#pragma once

#ifndef LYRA_ENGINE_FILESYSTEM_VFSENUMS_H
#define LYRA_ENGINE_FILESYSTEM_VFSENUMS_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Stdint.h>

namespace lyra
{
    enum struct FSLoader : uint
    {
        NATIVE,
        PHYSFS,
    };

    enum struct FSPacker : uint
    {
        PAK,
        ZIP,
    };

} // namespace lyra

#endif // LYRA_ENGINE_FILESYSTEM_VFSENUMS_H
