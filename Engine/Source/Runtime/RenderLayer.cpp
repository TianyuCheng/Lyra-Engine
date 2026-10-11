#include <Lyra/Runtime/RenderLayer.h>

using namespace lyra;

RenderLayer::RenderLayer()
{
}

void RenderLayer::bind(Application& app)
{
    // query frame count from the surface already registered on the toolboard
    auto surface          = app.get_toolboard().get<GPUSurface*>();
    uint frames_in_flight = surface->get_image_count();

    // initialize deferred resource retirement system
    resource_retire.init(frames_in_flight);

    // initialize geometry cache with ResourceRetire
    geometry_cache.set_resource_retire(&resource_retire);

    // register on toolboard so passes and systems can resolve them
    app.get_toolboard().add<RenderLayer*>(this);
    app.get_toolboard().add<ResourceRetire*>(&resource_retire);
    app.get_toolboard().add<GeometryCache*>(&geometry_cache);

    // camera projection: runs before rendering
    app.bind<AppEvent::UPDATE_PRE, &RenderLayer::update>(*this);

    // retirement tick: advance retirement queues once per frame before rendering
    app.bind<AppEvent::RENDER_PRE, &RenderLayer::pre_render>(*this);

    // on shutdown: device idle + flush all remaining retirements
    app.bind<AppEvent::DESTROY, &RenderLayer::destroy>(*this);
}

void RenderLayer::update(AppContext&)
{
}

void RenderLayer::pre_render(AppContext&)
{
    resource_retire.tick();
}

void RenderLayer::drain()
{
    resource_retire.drain();
}

void RenderLayer::destroy(AppContext& context)
{
    geometry_cache.clear();
    auto device = context.toolboard.get<GPUDevice*>();
    device->wait();
    drain();
}
