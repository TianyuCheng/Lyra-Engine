#include <Lyra/Runtime/SceneLayer.h>

using namespace lyra;

SceneLayer::SceneLayer() : world(), hierarchy(world)
{
    // do nothing
}

void SceneLayer::bind(Application& app)
{
    // save scene objects into toolboard
    app.get_toolboard().add<World*>(&world);
    app.get_toolboard().add<SceneTree*>(&hierarchy);
}

void SceneLayer::update(AppContext&)
{
}
