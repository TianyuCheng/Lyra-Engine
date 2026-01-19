#include <Lyra/Common/GUI.h>
#include <Lyra/Editor/Icons.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Editor/Colors.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/Inspector.h>

#define LYRA_INSPECTOR_WINDOW_NAME (LYRA_ICON_INSPECTOR "Inspector")

using namespace lyra;

Inspector::Inspector()
{
}

void Inspector::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &Inspector::update>(*this);
}

void Inspector::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_INSPECTOR_WINDOW_NAME, layout.right);
    });

    imgui::disable_window_menu_button();
    ImGui::Begin(LYRA_INSPECTOR_WINDOW_NAME);
    {
        // more to come
    }
    ImGui::End();
}
