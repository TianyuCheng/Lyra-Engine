#include <Lyra/Rendering/RendererManager.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/ResourceRetire.h>

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

bool RendererManager::init(AppContext& context, const RendererDescriptor& desc)
{
    descriptor  = desc;
    initialized = true;
    return true;
}

void RendererManager::shutdown(AppContext& context)
{
    if (!initialized) {
        return;
    }

    if (auto dev = context.toolboard.try_get<GPUDevice*>()) {
        dev->wait();
    }

    if (auto retire = context.toolboard.try_get<ResourceRetire*>()) {
        retire->drain();
    }

    if (has_active_renderer()) {
        current_plugin.get_api()->delete_renderer(active_handle);
        active_handle = {};
        current_plugin.unload();
        active_name.clear();
    }

    initialized = false;
}

bool RendererManager::load_renderer(StringView plugin_name, AppContext& context)
{
    if (has_active_renderer()) {
        current_plugin.get_api()->delete_renderer(active_handle);
        active_handle = {};
        current_plugin.unload();
        active_name.clear();
    }

    String name_str(plugin_name);
    if (!current_plugin.load(name_str.c_str())) {
        spdlog::error("failed to load renderer plugin '{}'", name_str);
        return false;
    }

    auto api = current_plugin.get_api();
    if (!api || !api->create_renderer) {
        spdlog::error("renderer plugin '{}' missing create_renderer entry point", name_str);
        current_plugin.unload();
        return false;
    }

    if (!api->create_renderer(active_handle, descriptor)) {
        spdlog::error("failed to create renderer instance from plugin '{}'", name_str);
        current_plugin.unload();
        return false;
    }

    active_name = name_str;

    if (api->set_settings) {
        api->set_settings(active_handle, settings);
    }

    if (api->resize) {
        api->resize(active_handle, descriptor.initial_extent.width, descriptor.initial_extent.height);
    }

    spdlog::info("successfully loaded renderer '{}' ({})", api->get_name ? api->get_name() : name_str.c_str(), name_str);
    return true;
}

bool RendererManager::switch_renderer(StringView plugin_name, AppContext& context)
{
    // synchronize gpu device before dismantling active pipelines
    if (auto dev = context.toolboard.try_get<GPUDevice*>()) {
        dev->wait();
    }

    // flush pending deferred retirements
    if (auto retire = context.toolboard.try_get<ResourceRetire*>()) {
        retire->drain();
    }

    return load_renderer(plugin_name, context);
}

void RendererManager::resize(uint width, uint height)
{
    descriptor.initial_extent = {width, height};

    if (has_active_renderer() && current_plugin.get_api()->resize) {
        current_plugin.get_api()->resize(active_handle, width, height);
    }
}

void RendererManager::render(
    AppContext&             context,
    const RenderView&       view,
    const RenderScene&      scene,
    const Backbuffer&       target,
    GPUCommandEncoderHandle command_encoder)
{
    if (!has_active_renderer()) {
        return;
    }

    auto api = current_plugin.get_api();
    if (api && api->render) {
        api->render(active_handle, view, scene, target, command_encoder);

        if (api->get_statistics) {
            api->get_statistics(active_handle, stats);
        }
    }
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
