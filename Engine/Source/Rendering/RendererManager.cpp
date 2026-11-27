#include <Lyra/Rendering/RendererManager.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/ShadingBuffers.h>
#include <Lyra/Rendering/SceneLighting.h>

using namespace lyra;

RendererManager::RendererManager() = default;

RendererManager::~RendererManager()
{
    if (has_active_renderer()) {
        current_plugin.get_api()->delete_renderer(active_handle);
        active_handle = {};
        current_plugin.unload();
    }
}

void RendererManager::init(AppContext& context, const RendererDescriptor& desc)
{
    descriptor  = desc;
    initialized = true;
}

void RendererManager::shutdown(AppContext& context)
{
    if (auto dev = context.services.try_get<GPUDevice*>()) {
        dev->wait();
    }

    if (auto retire = context.services.try_get<ResourceRetire*>()) {
        retire->drain();
    }

    if (has_active_renderer()) {
        current_plugin.get_api()->delete_renderer(active_handle);
        active_handle = {};
        current_plugin.unload();
        active_name.clear();
        active_backend.reset();
    }

    allocator.clear();
    arena.destroy();
    initialized = false;
}

Result<void> RendererManager::load_renderer(AppContext& context, StringView plugin_name)
{
    if (has_active_renderer()) {
        current_plugin.get_api()->delete_renderer(active_handle);
        active_handle = {};
        current_plugin.unload();
        active_name.clear();
        active_backend.reset();
    }

    String name_str(plugin_name);
    auto loaded = current_plugin.load(name_str.c_str());
    if (!loaded) {
        spdlog::error("failed to load renderer plugin '{}': {}", name_str, loaded.error());
        return make_unexpected("failed to load renderer plugin '" + name_str + "': " + loaded.error());
    }

    auto api = current_plugin.get_api();
    if (!api || !api->create_renderer) {
        spdlog::error("renderer plugin '{}' missing create_renderer entry point", name_str);
        current_plugin.unload();
        return make_unexpected("renderer plugin '" + name_str + "' missing create_renderer entry point");
    }

    if (!api->create_renderer(active_handle, descriptor)) {
        spdlog::error("failed to create renderer instance from plugin '{}'", name_str);
        current_plugin.unload();
        return make_unexpected("failed to create renderer instance from plugin '" + name_str + "'");
    }

    active_name    = name_str;
    active_backend = to_renderer_backend(plugin_name);

    if (api->set_settings) {
        api->set_settings(active_handle, settings);
    }

    if (api->resize) {
        api->resize(active_handle, descriptor.initial_extent.width, descriptor.initial_extent.height);
    }

    spdlog::info("successfully loaded renderer '{}' ({})", api->get_name ? api->get_name() : name_str.c_str(), name_str.c_str());
    return {};
}

Result<void> RendererManager::load_renderer(AppContext& context, RendererBackend backend)
{
    auto plugin_name = get_renderer_plugin_name(backend);
    auto res         = load_renderer(context, plugin_name);
    if (res) {
        active_backend = backend;
    }
    return res;
}

Result<void> RendererManager::switch_renderer(AppContext& context, StringView plugin_name)
{
    // synchronize gpu device before dismantling active pipelines
    if (auto dev = context.services.try_get<GPUDevice*>()) {
        dev->wait();
    }

    // flush pending deferred retirements
    if (auto retire = context.services.try_get<ResourceRetire*>()) {
        retire->drain();
    }

    allocator.clear();
    return load_renderer(context, plugin_name);
}

Result<void> RendererManager::switch_renderer(AppContext& context, RendererBackend backend)
{
    auto plugin_name = get_renderer_plugin_name(backend);
    return switch_renderer(context, plugin_name);
}

void RendererManager::resize(uint width, uint height)
{
    descriptor.initial_extent = {width, height};

    if (has_active_renderer() && current_plugin.get_api()->resize) {
        current_plugin.get_api()->resize(active_handle, width, height);
    }
}

FrameGraph::TextureHandle RendererManager::render(const RenderFrameContext& ctx)
{
    if (!has_active_renderer()) {
        return ctx.color_target;
    }

    auto api = current_plugin.get_api();
    if (api && api->render) {
        auto out_target = api->render(active_handle, ctx);
        if (api->get_statistics) {
            api->get_statistics(active_handle, stats);
        }
        return out_target;
    }
    return ctx.color_target;
}

void RendererManager::render(
    AppContext&             context,
    const RenderView&       view,
    const RenderScene&      scene,
    const Backbuffer&       target,
    GPUCommandEncoderHandle command_encoder)
{
    if (!has_active_renderer() || !command_encoder.valid() || target.extent.width == 0 || target.extent.height == 0) {
        return;
    }

    FrameGraphBuilder builder;

    // import target render target
    FrameGraphTexture imported_target{};
    imported_target.texture = target.texture;
    imported_target.view    = target.texview;
    imported_target.format  = target.format;
    auto target_handle      = builder.import(imported_target);

    ViewUniforms view_uniforms{};
    view_uniforms.init(view, arena);

    SceneLighting scene_lighting{};
    scene_lighting.init(scene, arena, context.services.get<GPUDevice>());

    RenderFrameContext ctx{
        .builder        = builder,
        .view           = view,
        .scene          = scene,
        .view_uniforms  = &view_uniforms,
        .scene_lighting = &scene_lighting,
        .color_target   = target_handle,
        .depth_target   = {},
        .color_format   = target.format,
        .depth_format   = descriptor.depth_format,
        .render_extent  = target.extent,
        .display_extent = target.extent,
        .services       = &context.services,
    };

    render(ctx);

    auto graph = builder.build();
    if (graph) {
        FrameGraphContext fg_context{};
        fg_context.device         = context.services.get<GPUDevice>();
        fg_context.cmdlist        = GPUCommandBuffer(command_encoder);
        fg_context.arena          = &arena;
        fg_context.view_uniforms  = ctx.view_uniforms;
        fg_context.scene_lighting = ctx.scene_lighting;
        graph->execute(&fg_context, &allocator);
    }

    allocator.tick();
    arena.tick();
}

void RendererManager::draw_debug_panel()
{
    if (has_active_renderer() && current_plugin.get_api()->draw_debug_panel) {
        current_plugin.get_api()->draw_debug_panel(active_handle);
    }
}

void RendererManager::set_settings(const RendererSettings& s)
{
    settings = s;
    if (has_active_renderer() && current_plugin.get_api()->set_settings) {
        current_plugin.get_api()->set_settings(active_handle, settings);
    }
}
