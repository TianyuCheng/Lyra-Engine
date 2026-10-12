#include <Lyra/Rendering/ResourceRetire.h>

using namespace lyra;

void ResourceRetire::init(uint frames_in_flight)
{
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
}

void ResourceRetire::tick()
{
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

void ResourceRetire::drain()
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

bool ResourceRetire::empty() const
{
    return buffers.empty() &&
           textures.empty() &&
           texture_views.empty() &&
           samplers.empty() &&
           shaders.empty() &&
           query_sets.empty() &&
           fences.empty() &&
           tlases.empty() &&
           blases.empty() &&
           bind_group_heaps.empty() &&
           bind_group_layouts.empty() &&
           pipeline_layouts.empty() &&
           render_pipelines.empty() &&
           compute_pipelines.empty() &&
           raytracing_pipelines.empty();
}

usize ResourceRetire::size() const
{
    return buffers.size() +
           textures.size() +
           texture_views.size() +
           samplers.size() +
           shaders.size() +
           query_sets.size() +
           fences.size() +
           tlases.size() +
           blases.size() +
           bind_group_heaps.size() +
           bind_group_layouts.size() +
           pipeline_layouts.size() +
           render_pipelines.size() +
           compute_pipelines.size() +
           raytracing_pipelines.size();
}
