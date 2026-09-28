#include <Lyra/Runtime/SceneLayer.h>

using namespace lyra;

SceneLayer::SceneLayer() : world(), hierarchy(world), scene_manager(nullptr)
{
    // do nothing
}

void SceneLayer::bind(Application& app)
{
    // retrieve asset server from toolboard
    auto* ams     = app.get_toolboard().get<AssetServer*>();
    scene_manager = std::make_unique<SceneManager>(world, hierarchy, *ams);

    // save scene objects into toolboard
    app.get_toolboard().add<World*>(&world);
    app.get_toolboard().add<SceneTree*>(&hierarchy);
    app.get_toolboard().add<SceneManager*>(scene_manager.get());
    app.bind<AppEvent::UPDATE, &SceneLayer::update>(*this);

    // create default scene with Main Camera
    scene_manager->create("Untitled");
}

void SceneLayer::update(AppContext&)
{
    if (scene_manager) {
        scene_manager->update();
    } else {
        hierarchy.update();
    }
}
