#pragma once

#ifndef LYRA_LYRA_PLAYER_ASSET_LAYER_H
#define LYRA_LYRA_PLAYER_ASSET_LAYER_H

#include <Lyra/Assets/AMSServer.h>

// local includes
#include <Lyra/Player/Application.h>

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
         * @brief Register the AssetServer to the blackboard and bind its lifecycle events.
         */
        void bind(Application& app);

        /**
         * @brief Update function for processing asset management related events.
         */
        void update(Blackboard&);

    private:
        AssetServer ams; ///< The underlying asset server.
    };

} // namespace lyra

#endif // LYRA_LYRA_PLAYER_ASSET_LAYER_H
