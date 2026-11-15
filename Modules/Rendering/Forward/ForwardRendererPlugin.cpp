#include "ForwardRenderer.h"
#include <Lyra/Rendering/RendererAPI.h>

using namespace lyra;

static auto get_name() -> CString
{
    return "Forward/ForwardPlus Renderer";
}

static auto get_capabilities() -> RendererCapabilities
{
    RendererCapabilities caps{};
    caps.features               = RendererFeature::RASTERIZATION | RendererFeature::POST_PROCESSING | RendererFeature::DEBUG_VISUALIZATION;
    caps.supported_primitives   = PrimitiveSupport::MESH;
    caps.max_directional_lights = 4;
    caps.max_local_lights       = 4096;
    caps.max_samples_per_pixel  = 1;
    caps.supports_hdr           = true;
    return caps;
}

static bool create_renderer(RendererHandle& out_handle, const RendererDescriptor& descriptor)
{
    auto renderer = std::make_unique<ForwardRenderer>();
    if (RHI::api() && descriptor.compiler.handle.valid()) {
        if (!renderer->init(descriptor.device, descriptor.compiler)) {
            return false;
        }
    }

    out_handle = RendererHandle(renderer.release());
    return true;
}

static void delete_renderer(RendererHandle handle)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->shutdown();
        delete renderer;
    }
}

static void resize(RendererHandle handle, uint width, uint height)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->resize(width, height);
    }
}

static void set_settings(RendererHandle handle, const RendererSettings& settings)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->set_settings(settings);
    }
}

static void get_settings(RendererHandle handle, RendererSettings& out_settings)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->get_settings(out_settings);
    }
}

static void get_statistics(RendererHandle handle, RendererStats& out_stats)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->get_statistics(out_stats);
    }
}

static FrameGraph::TextureHandle render(RendererHandle handle, const RenderFrameContext& ctx)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        return renderer->render(ctx);
    }
    return ctx.color_target;
}

static void draw_debug_panel(RendererHandle handle)
{
    if (auto* renderer = handle.as_type<ForwardRenderer>()) {
        renderer->draw_debug_panel();
    }
}

LYRA_EXPORT auto prepare() -> void
{
}

LYRA_EXPORT auto cleanup() -> void
{
}

LYRA_EXPORT auto create() -> RendererAPI
{
    RendererAPI api{};
    api.get_name         = get_name;
    api.get_capabilities = get_capabilities;
    api.create_renderer  = create_renderer;
    api.delete_renderer  = delete_renderer;
    api.resize           = resize;
    api.set_settings     = set_settings;
    api.get_settings     = get_settings;
    api.get_statistics   = get_statistics;
    api.render           = render;
    api.draw_debug_panel = draw_debug_panel;
    return api;
}
