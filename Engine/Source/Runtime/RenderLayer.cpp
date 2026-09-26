#include <Lyra/Runtime/RenderLayer.h>

using namespace lyra;

RenderLayer::RenderLayer()
{
    // deletion queues are initialized in bind() once we know the surface frame count
}

void RenderLayer::bind(Application& app)
{
    // query frame count from the surface already registered on the toolboard
    auto surface          = app.get_toolboard().get<GPUSurface*>();
    uint frames_in_flight = surface->get_image_count();

    // initialize all per-type deletion queues
    buffers.init(frames_in_flight);
    textures.init(frames_in_flight);
    texture_views.init(frames_in_flight);
    samplers.init(frames_in_flight);
    shaders.init(frames_in_flight);
    query_sets.init(frames_in_flight);
    fences.init(frames_in_flight);
    tlases.init(frames_in_flight);
    blases.init(frames_in_flight);
    bind_group_heaps.init(frames_in_flight);
    bind_group_layouts.init(frames_in_flight);
    pipeline_layouts.init(frames_in_flight);
    render_pipelines.init(frames_in_flight);
    compute_pipelines.init(frames_in_flight);
    raytracing_pipelines.init(frames_in_flight);

    // register on toolboard so render passes can enqueue deletions via RenderLayer*
    app.get_toolboard().add<RenderLayer*>(this);

    // camera projection: runs before rendering
    app.bind<AppEvent::UPDATE_PRE, &RenderLayer::update>(*this);

    // deletion queue tick: advance all queues once per frame before rendering
    app.bind<AppEvent::RENDER_PRE, &RenderLayer::pre_render>(*this);

    // on shutdown: device idle + flush all remaining deletions
    app.bind<AppEvent::DESTROY, &RenderLayer::destroy>(*this);
}

void RenderLayer::update(AppContext&)
{
}

void RenderLayer::pre_render(AppContext&)
{
    // tick all per-type queues — each flushes resources that are now N frames old
    buffers.tick();
    textures.tick();
    texture_views.tick();
    samplers.tick();
    shaders.tick();
    query_sets.tick();
    fences.tick();
    tlases.tick();
    blases.tick();
    bind_group_heaps.tick();
    bind_group_layouts.tick();
    pipeline_layouts.tick();
    render_pipelines.tick();
    compute_pipelines.tick();
    raytracing_pipelines.tick();
}

void RenderLayer::drain()
{
    buffers.drain();
    textures.drain();
    texture_views.drain();
    samplers.drain();
    shaders.drain();
    query_sets.drain();
    fences.drain();
    tlases.drain();
    blases.drain();
    bind_group_heaps.drain();
    bind_group_layouts.drain();
    pipeline_layouts.drain();
    render_pipelines.drain();
    compute_pipelines.drain();
    raytracing_pipelines.drain();
}

void RenderLayer::destroy(AppContext& context)
{
    auto device = context.toolboard.get<GPUDevice*>();
    device->wait();
    drain();
}
