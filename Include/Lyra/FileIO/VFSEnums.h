#pragma once

#ifndef LYRA_LYRA_FILEIO_VFSENUMS_H
#define LYRA_LYRA_FILEIO_VFSENUMS_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>

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

#endif // LYRA_LYRA_FILEIO_VFSENUMS_H
