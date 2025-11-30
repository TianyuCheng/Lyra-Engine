#include <Lyra/Plugin/GUI/GUITypes.h>

using namespace lyra;

static Own<GUIRenderPlugin> GUI_PLUGIN = nullptr;

GUIRenderAPI* GUIRenderer::api()
{
    return GUI_PLUGIN->get_api();
}

OwnedResource<GUIRenderer> GUIRenderer::init(const GUIDescriptor& descriptor)
{
    if (!GUI_PLUGIN.get())
        GUI_PLUGIN = std::make_unique<GUIRenderPlugin>("lyra-imgui");

    OwnedResource<GUIRenderer> gui(new GUIRenderer());
    GUIRenderer::api()->create_gui(gui->handle, descriptor);
    auto context = GUIRenderer::api()->get_context(gui->handle);
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(context));
    return gui;
}
