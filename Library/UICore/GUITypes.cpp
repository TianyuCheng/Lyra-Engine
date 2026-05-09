#include <Lyra/UICore/GUITypes.h>

using namespace lyra;

static Own<GUIPlugin> GUI_PLUGIN = nullptr;

GUIAPI* GUIRenderer::api()
{
    return GUI_PLUGIN->get_api();
}

OwnedResource<GUIRenderer> GUIRenderer::init(const GUIDescriptor& descriptor)
{
    if (!GUI_PLUGIN.get())
        GUI_PLUGIN = std::make_unique<GUIPlugin>("lyra-imgui");

    OwnedResource<GUIRenderer> gui(new GUIRenderer());
    GUIRenderer::api()->create_gui(gui->handle, descriptor);
    return gui;
}
