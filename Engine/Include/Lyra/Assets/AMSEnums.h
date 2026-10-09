#pragma once

#ifndef LYRA_ENGINE_ASSETS_AMSENUMS_H
#define LYRA_ENGINE_ASSETS_AMSENUMS_H

#include <Lyra/Utilities/Enums.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/UUID.h>

/**
 * @file AMSEnums.h
 * @brief Enums for the Asset Management System.
 */

namespace lyra
{

    using AssetTypeID = lyra::UUID;

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

#endif // LYRA_ENGINE_ASSETS_AMSENUMS_H
