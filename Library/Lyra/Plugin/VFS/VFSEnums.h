#pragma once

#ifndef LYRA_LIBRARY_PLUGIN_VFS_ENUMS_H
#define LYRA_LIBRARY_PLUGIN_VFS_ENUMS_H

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

#endif // LYRA_LIBRARY_PLUGIN_VFS_ENUMS_H
