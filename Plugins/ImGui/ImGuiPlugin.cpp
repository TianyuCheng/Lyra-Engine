// global module headers
#include <Lyra/Common/String.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/UICore/GUIAPI.h>

// local plugin header(s)
#include "GUIRenderer.h"

using namespace lyra;
using namespace lyra::imgui;

static auto get_api_name() -> CString { return "ImGui"; }

static bool create_gui(GUIHandle& gui, const GUIDescriptor& descriptor)
{
    auto renderer = std::make_unique<GUIRenderer>(descriptor);
    gui.pointer   = renderer.release();
    return true;
}

static void delete_gui(GUIHandle gui)
{
    auto renderer = gui.as_type<GUIRenderer>();
    renderer->destroy();
    delete renderer;
}

static void new_frame(GUIHandle gui)
{
    gui.as_type<GUIRenderer>()->new_frame();
}

static void end_frame(GUIHandle gui)
{
    gui.as_type<GUIRenderer>()->end_frame();
}

static void update_gui(GUIHandle gui)
{
    gui.as_type<GUIRenderer>()->update();
}

static void resize_gui(GUIHandle gui)
{
    gui.as_type<GUIRenderer>()->resize();
}

static void* get_context(GUIHandle gui)
{
    return gui.as_type<GUIRenderer>()->context();
}

GUITextureHandle create_texture(GUIHandle gui, GPUTextureHandle texture, GPUTextureViewHandle view)
{
    return gui.as_type<GUIRenderer>()->create_texture(texture, view);
}

void delete_texture(GUIHandle gui, GUITextureHandle texid)
{
    return gui.as_type<GUIRenderer>()->delete_texture(texid);
}

static void render_main_viewport(GUIHandle gui, GPUCommandEncoderHandle encoder, GPUTextureViewHandle backbuffer)
{
    auto cmdbuffer = GPUCommandBuffer(encoder);
    auto renderer  = gui.as_type<GUIRenderer>();
    renderer->reset();
    renderer->prepare(cmdbuffer);
    renderer->begin_render_pass(cmdbuffer, backbuffer);
    renderer->render(cmdbuffer, backbuffer);
    renderer->end_render_pass(cmdbuffer);
}

static void render_side_viewports(GUIHandle gui)
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

namespace lyra::imgui
{

    void prepare()
    {
        get_logger()->set_level(parse_log_level_from_env("LYRA_IMGUI_VERBOSITY"));
    }

    void cleanup()
    {
        // do nothing
    }

    GUIAPI create()
    {
        auto api                  = GUIAPI{};
        api.get_api_name          = get_api_name;
        api.create_gui            = create_gui;
        api.delete_gui            = delete_gui;
        api.update_gui            = update_gui;
        api.resize_gui            = resize_gui;
        api.new_frame             = new_frame;
        api.end_frame             = end_frame;
        api.get_context           = get_context;
        api.create_texture        = create_texture;
        api.delete_texture        = delete_texture;
        api.render_main_viewport  = render_main_viewport;
        api.render_side_viewports = render_side_viewports;
        return api;
    }

} // namespace lyra::imgui
