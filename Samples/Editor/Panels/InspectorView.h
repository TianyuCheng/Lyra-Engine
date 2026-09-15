#pragma once

#ifndef LYRA_EDITOR_PANELS_INSPECTOR_VIEW_H
#define LYRA_EDITOR_PANELS_INSPECTOR_VIEW_H

// local imports
#include <Lyra/Engine/Application.h>

namespace lyra
{
    struct InspectorView
    {
    public:
        explicit InspectorView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        void draw_inspector(World& world, SceneNode node);
    };

    using ObjectView = InspectorView;
} // namespace lyra

#endif // LYRA_EDITOR_PANELS_INSPECTOR_VIEW_H
