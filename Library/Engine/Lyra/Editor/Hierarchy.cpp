#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "Hierarchy.h"

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

    ImGui::Begin(LYRA_HIERARCHY_WINDOW_NAME);
    {
        // more to come
    }
    ImGui::End();
}
