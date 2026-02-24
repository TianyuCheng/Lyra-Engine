#include "CameraLayer.h"

using namespace lyra;

CameraLayer::CameraLayer()
{
    // do nothing
}

void CameraLayer::bind(Application& app)
{
    // add camera layer into blackboard
    app.get_blackboard().add<CameraLayer*>(this);

    // update cameras before rendering
    app.bind<AppEvent::UPDATE_PRE, &CameraLayer::update>(*this);
}

void CameraLayer::update(Blackboard& blackboard)
{
    auto world = blackboard.get<World*>();

    update_perspective(*world);
    update_orthographic(*world);
}

void CameraLayer::update_perspective(World& world)
{
    auto view = world.view<PerspectiveCamera, CameraProjection>();
    for (auto entity : view) {
        auto& camera     = view.get<PerspectiveCamera>(entity);
        auto& projection = view.get<CameraProjection>(entity);

        projection.projection = glm::perspective(glm::radians(camera.fov), camera.aspect, camera.near_plane, camera.far_plane);
    }
}

void CameraLayer::update_orthographic(World& world)
{
    auto view = world.view<OrthographicCamera, CameraProjection>();
    for (auto entity : view) {
        auto& camera     = view.get<OrthographicCamera>(entity);
        auto& projection = view.get<CameraProjection>(entity);

        float half_size       = camera.size * 0.5f;
        projection.projection = glm::ortho(-half_size * camera.aspect, half_size * camera.aspect, -half_size, half_size, camera.near_plane, camera.far_plane);
    }
}
