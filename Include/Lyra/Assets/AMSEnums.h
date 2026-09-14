#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSENUMS_H
#define LYRA_LYRA_ASSETS_AMSENUMS_H

#include <Lyra/Common/Enums.h>
#include <Lyra/Common/Stdint.h>

/**
 * @file AMSEnums.h
 * @brief Enums for the Asset Management System.
 */

namespace lyra
{

    using AssetTypeID = uint;

    enum struct AssetWatchAction : uint8_t
    {
        Added,
        Modified,
        Removed,
        Renamed
    };

    enum struct AssetCookStatus : uint8_t
    {
        Idle,
        Queued,
        Cooking,
        Succeeded,
        Failed,
        Skipped
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSENUMS_H
