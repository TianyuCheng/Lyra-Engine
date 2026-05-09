#include <Lyra/Editor/Layout.h>

using namespace lyra;

EditorLayout::EditorLayout(const EditorLayoutDescriptor& descriptor) : descriptor(descriptor)
{
    // do nothing
}

void EditorLayout::bind(Application& app)
{
    // bind layout manager events
    app.bind<AppEvent::UPDATE, &EditorLayout::update>(*this);
}

void EditorLayout::update(Blackboard& blackboard)
{
    ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

    // running dock builder exactly once
    lyra::execute_once([&]() {
        blackboard.add<EditorLayoutInfo>(init());
    });
}

EditorLayoutInfo EditorLayout::init() const
{
    ImGuiID dockspace_id = ImGui::GetMainViewport()->ID;

    // clear old layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // split the dockspace into regions
    EditorLayoutInfo layout{};
    layout.main   = dockspace_id;
    layout.top    = ImGui::DockBuilderSplitNode(layout.main, ImGuiDir_Up, descriptor.top, nullptr, &layout.main);
    layout.left   = ImGui::DockBuilderSplitNode(layout.main, ImGuiDir_Left, descriptor.left, nullptr, &layout.main);
    layout.bottom = ImGui::DockBuilderSplitNode(layout.main, ImGuiDir_Down, descriptor.bottom, nullptr, &layout.main);
    layout.right  = ImGui::DockBuilderSplitNode(layout.main, ImGuiDir_Right, descriptor.right, nullptr, &layout.main);

    // finish
    ImGui::DockBuilderFinish(dockspace_id);
    return layout;
}
