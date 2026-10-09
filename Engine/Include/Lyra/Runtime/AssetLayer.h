#pragma once

#ifndef LYRA_ENGINE_RUNTIME_ASSET_LAYER_H
#define LYRA_ENGINE_RUNTIME_ASSET_LAYER_H

#include <Lyra/Assets/AMSServer.h>

// local includes
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    /**
     * @brief The AssetLayer struct wraps the AssetServer and handles its application events.
     */
    struct AssetLayer
    {
    public:
        /**
         * @brief Construct the AssetLayer with an AMSDescriptor.
         */
        explicit AssetLayer(const AMSDescriptor& descriptor);

        /**
         * @brief Register the AssetServer to the toolboard and bind its lifecycle events.
         */
        void bind(Application& app);

        /**
         * @brief Update function for processing asset management related events.
         */
        void update(AppContext&);

    private:
        AssetServer ams; ///< The underlying asset server.
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_ASSET_LAYER_H
