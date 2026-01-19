#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Editor/Icons.h>
#include <Lyra/Editor/Colors.h>
#include <Lyra/Editor/Layout.h>
#include <Lyra/Editor/Hierarchy.h>

#define LYRA_HIERARCHY_WINDOW_NAME (LYRA_ICON_HIERARCHY "Hierarchy")

using namespace lyra;

Hierarchy::Hierarchy()
{
}

void Hierarchy::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &Hierarchy::update>(*this);
}

void Hierarchy::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_HIERARCHY_WINDOW_NAME, layout.left);
    });

    imgui::disable_window_menu_button();
    ImGui::Begin(LYRA_HIERARCHY_WINDOW_NAME);
    {
        // more to come
    }
    ImGui::End();
}
