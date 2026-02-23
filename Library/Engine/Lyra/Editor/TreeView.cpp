#include <Lyra/Common/GUI.h>
#include <Lyra/Common/Logger.h>

// local imports
#include "Icons.h"
#include "Layout.h"
#include "TreeView.h"

#define LYRA_TREE_VIEW_WINDOW_NAME (LYRA_ICON_TREE "Hierarchy")

using namespace lyra;

TreeView::TreeView()
{
}

void TreeView::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &TreeView::update>(*this);
}

void TreeView::update(Blackboard& blackboard)
{
    lyra::execute_once([&]() {
        auto& layout = blackboard.get<EditorLayoutInfo>();
        ImGui::DockBuilderDockWindow(LYRA_TREE_VIEW_WINDOW_NAME, layout.left);
    });

    ImGui::Begin(LYRA_TREE_VIEW_WINDOW_NAME);
    {
        // more to come
    }
    ImGui::End();
}
