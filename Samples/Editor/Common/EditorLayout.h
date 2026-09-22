#pragma once

#ifndef LYRA_EDITOR_COMMON_EDITOR_LAYOUT_H
#define LYRA_EDITOR_COMMON_EDITOR_LAYOUT_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/UISystem/UIDock.h>

// local imports
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    // uint32_t == 0 means this panel does not exist
    struct EditorLayoutInfo
    {
        uint32_t main   = 0;
        uint32_t left   = 0;
        uint32_t right  = 0;
        uint32_t top    = 0;
        uint32_t bottom = 0;
    };

    // editor layout descriptor is used for configuring docking splits.
    struct EditorLayoutDescriptor
    {
        float left   = 0.25f;
        float right  = 0.25f;
        float top    = 0.25f;
        float bottom = 0.25f;
    };

    // editor layout configures the workspace layout splits.
    struct EditorLayout
    {
    public:
        explicit EditorLayout(const EditorLayoutDescriptor& descriptor);

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        EditorLayoutInfo init() const;

    private:
        EditorLayoutDescriptor descriptor = {};
    };

} // namespace lyra

#endif // LYRA_LYRA_EDITOR_LAYOUT_H
