#pragma once

#ifndef LYRA_LIBRARY_ASSETS_AMS_API_H
#define LYRA_LIBRARY_ASSETS_AMS_API_H

#include <Lyra/Common/Config.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/Handle.h>
#include <Lyra/FileIO/VFSAPI.h>
#include <Lyra/Assets/AMSUtils.h>

namespace lyra
{
    struct AssetServer;

    /**
     * @brief The AssetHandlerAPI struct defines the interface for asset-type specific logic.
     *
     * This API is typically implemented by plugins or built-in asset handlers to allow the
     * AssetServer to process, load, and unload different types of assets.
     */
    struct AssetHandlerAPI
    {
        /**
         * @brief (Optional) Configure asset specific options via a JSON object.
         * @param options The configuration options.
         */
        void (*configure)(const JSON& options);

        /**
         * @brief Process a raw source asset and save it to a target path (cooking).
         *
         * @param manager Pointer to the active AssetServer.
         * @param source_path Path to the raw source file in the OS filesystem.
         * @param target_path Path where the processed asset should be saved.
         * @return JSON The metadata object describing the imported asset.
         */
        JSON (*process)(AssetServer* manager, OSPath source_path, OSPath target_path);

        /**
         * @brief Load the processed asset data into memory.
         *
         * @param loader Pointer to the virtual FileLoader.
         * @param metadata The metadata JSON associated with the asset.
         * @return void* A pointer to the loaded asset data (untyped).
         */
        void* (*load)(AssetServer* manager, FileLoader* loader, const JSON& metadata);

        /**
         * @brief Properly unload and clean up the asset data.
         * @param data Pointer to the asset data to be unloaded.
         */
        void (*unload)(AssetServer* manager, void* data);
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_API_H
