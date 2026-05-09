#include <Lyra/Player/SceneLayer.h>

using namespace lyra;

SceneLayer::SceneLayer() : world(), hierarchy(world)
{
    // do nothing
}

void SceneLayer::bind(Application& app)
{
    // save scene objects into blackboard
    app.get_blackboard().add<World*>(&world);
    app.get_blackboard().add<SceneTree*>(&hierarchy);
}

void SceneLayer::update(Blackboard&)
{
}
