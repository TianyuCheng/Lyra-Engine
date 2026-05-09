#pragma once

#ifndef LYRA_LYRA_ASSETS_AMSAPI_H
#define LYRA_LYRA_ASSETS_AMSAPI_H

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
     * @brief The AssetLoaderAPI struct defines the interface for loading engine-ready assets.
     */
    struct AssetLoaderAPI
    {
        /**
         * @brief Configure the loader and provide access to the AssetServer.
         */
        void (*configure)(AssetServer* manager, const JSON& options);

        /**
         * @brief Load processed asset data into memory.
         * @param loader Pointer to the virtual FileLoader.
         * @param path The virtual path to the asset file.
         * @return void* A pointer to the loaded asset data (untyped).
         */
        void* (*load)(FileLoader* loader, FSPath path);

        /**
         * @brief Properly unload and clean up the asset data.
         * @param data Pointer to the asset data to be unloaded.
         */
        void (*unload)(void* data);

        /**
         * @brief Get the list of file extensions supported by this loader.
         * @param extensions Output parameter for the array of C-strings.
         * @return uint The number of supported extensions.
         */
        uint (*get_supported_extensions)(CString* extensions);
    };

    /**
     * @brief The AssetCookerAPI struct defines the interface for preprocessing source assets.
     */
    struct AssetCookerAPI
    {
        /**
         * @brief Configure the cooker and provide access to the AssetServer.
         */
        void (*configure)(AssetServer* manager, const JSON& options);

        /**
         * @brief Process a raw source asset and save it to a target path (cooking).
         * @param metadata JSON object containing input info (like guid) and to be populated with output metadata (like path).
         * @param source_path Path to the raw source file in the OS filesystem.
         * @param caches_root Root path where processed assets should be saved.
         * @return True if processing was successful, false otherwise.
         */
        bool (*process)(JSON& metadata, OSPath source_path, OSPath caches_root);

        /**
         * @brief Get the list of file extensions supported by this cooker.
         * @param extensions Output parameter for the array of C-strings.
         * @return uint The number of supported extensions.
         */
        uint (*get_supported_extensions)(CString* extensions);
    };

} // namespace lyra

#endif // LYRA_LYRA_ASSETS_AMSAPI_H
