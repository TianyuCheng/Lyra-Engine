#include <Lyra/Engine/Layers/AssetLayer.h>

using namespace lyra;

AssetLayer::AssetLayer(const AMSDescriptor& descriptor) : ams(descriptor)
{
    // do nothing
}

void AssetLayer::bind(Application& app)
{
    // save asset manager into blackboard
    app.get_blackboard().add<AssetServer*>(&ams);

    // bind asset manager events
    app.bind<AppEvent::UPDATE>(&AssetLayer::update, this);
}

void AssetLayer::update()
{
    // do nothing for now,
    // we will do some asset event polling later
}
