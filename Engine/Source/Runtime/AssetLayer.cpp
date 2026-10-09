#include <Lyra/Runtime/AssetLayer.h>

using namespace lyra;

AssetLayer::AssetLayer(const AMSDescriptor& descriptor) : ams(descriptor)
{
    // do nothing
}

void AssetLayer::bind(Application& app)
{
    // save asset manager into toolboard
    app.get_toolboard().add<AssetServer*>(&ams);

    // bind asset manager events
    app.bind<AppEvent::UPDATE, &AssetLayer::update>(*this);
}

void AssetLayer::update(AppContext&)
{
    ams.poll_events();
    ams.purge();
    ams.flush();
}
