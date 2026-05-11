#include <Lyra/Common/Plugin.h>
#include <Lyra/UICore/GUITypes.h>

using namespace lyra;

// forward declarations for inlined plugins
namespace lyra::imgui
{
    extern GUIAPI create();
    extern void   prepare();
    extern void   cleanup();
} // namespace lyra::imgui

using GUIPluginBuiltin = BuiltinPlugin<GUIAPI>;

static Own<GUIPluginBuiltin> GUI_PLUGIN = nullptr;

GUIAPI* GUIRenderer::api()
{
    return GUI_PLUGIN->get_api();
}

OwnedResource<GUIRenderer> GUIRenderer::init(const GUIDescriptor& descriptor)
{
    if (!GUI_PLUGIN.get())
        GUI_PLUGIN = std::make_unique<GUIPluginBuiltin>(
            lyra::imgui::create,
            lyra::imgui::prepare,
            lyra::imgui::cleanup);

    OwnedResource<GUIRenderer> gui(new GUIRenderer());
    GUIRenderer::api()->create_gui(gui->handle, descriptor);
    return gui;
}
