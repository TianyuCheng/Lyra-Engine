#pragma once

#ifndef LYRA_LIBRARY_ENGINE_LAYERS_ASSET_LAYER_H
#define LYRA_LIBRARY_ENGINE_LAYERS_ASSET_LAYER_H

#include <Lyra/Plugin/AMS/AMSTypes.h>
#include <Lyra/Engine/Applet/Application.h>

namespace lyra
{
    // AssetLayer is only a wrapper around AssetServer.
    // It is created to handle application events.
    struct AssetLayer
    {
    public:
        explicit AssetLayer(const AMSDescriptor& descriptor);

        void bind(Application& app);

        void update(Blackboard&);

    private:
        AssetServer ams;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_LAYERS_ASSET_LAYER_H
