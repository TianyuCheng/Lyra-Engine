#include "EditorLayout.h"
#include <Lyra/UICore/UIDock.h>

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
    ui::workspace::LayoutSplit split;
    split.left   = descriptor.left;
    split.right  = descriptor.right;
    split.top    = descriptor.top;
    split.bottom = descriptor.bottom;
    ui::workspace::setup(split);

    // running dock builder exactly once
    lyra::execute_once([&]() {
        blackboard.add<EditorLayoutInfo>(init());
    });
}

EditorLayoutInfo EditorLayout::init() const
{
    auto nodes = ui::workspace::get_nodes();
    EditorLayoutInfo layout{};
    layout.main   = nodes.main;
    layout.top    = nodes.top;
    layout.left   = nodes.left;
    layout.bottom = nodes.bottom;
    layout.right  = nodes.right;
    return layout;
}
