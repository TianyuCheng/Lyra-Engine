#include "EditorLayout.h"
#include <Lyra/UISystem/Widgets/UIDock.h>

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

void EditorLayout::update(AppContext&)
{
    ui::workspace::dockspace(descriptor.flags, descriptor.layout);
}
