#include <Lyra/Runtime/SceneLayer.h>

using namespace lyra;

SceneLayer::SceneLayer() : world(), hierarchy(world), scene_manager(nullptr)
{
    // do nothing
}

void SceneLayer::bind(Application& app)
{
    // retrieve asset server from services
    auto* ams = app.get_services().get<AssetServer*>();
    assert(ams != nullptr && "SceneLayer requires a valid AssetServer registered on the services");
    scene_manager = std::make_unique<SceneManager>(world, hierarchy, *ams);

    // save scene objects into services
    app.get_services().add<World*>(&world);
    app.get_services().add<SceneTree*>(&hierarchy);
    app.get_services().add<SceneManager*>(scene_manager.get());
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
