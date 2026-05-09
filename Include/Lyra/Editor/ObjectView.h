#pragma once

#ifndef LYRA_LYRA_EDITOR_OBJECT_VIEW_H
#define LYRA_LYRA_EDITOR_OBJECT_VIEW_H

// local imports
#include <Lyra/Player/Application.h>

namespace lyra
{
    struct ObjectView
    {
    public:
        explicit ObjectView();

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        void draw_inspector(World& world, SceneNode node);
    };
} // namespace lyra

#endif // LYRA_LYRA_EDITOR_OBJECT_VIEW_H
