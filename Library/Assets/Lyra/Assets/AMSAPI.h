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
         * @param source_path Path to the raw source file in the OS filesystem.
         * @param target_path Path where the processed asset should be saved.
         * @return JSON The metadata object describing the imported asset.
         */
        JSON (*process)(OSPath source_path, OSPath target_path);

        /**
         * @brief Get the list of file extensions supported by this cooker.
         * @param extensions Output parameter for the array of C-strings.
         * @return uint The number of supported extensions.
         */
        uint (*get_supported_extensions)(CString* extensions);
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ASSETS_AMS_API_H
