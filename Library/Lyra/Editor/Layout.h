#pragma once

#ifndef LYRA_LIBRARY_ENGINE_SYSTEM_LAYOUT_MANAGER_H
#define LYRA_LIBRARY_ENGINE_SYSTEM_LAYOUT_MANAGER_H

#include <Lyra/Common/GUI.h>
#include <Lyra/Engine/Applet/Application.h>

namespace lyra
{
    // ImGuiID == 0 means this panel does not exist
    struct EditorLayoutInfo
    {
        ImGuiID main   = 0;
        ImGuiID left   = 0;
        ImGuiID right  = 0;
        ImGuiID top    = 0;
        ImGuiID bottom = 0;
    };

    // EditorLayoutDescriptor is used for configuring docking splits.
    struct EditorLayoutDescriptor
    {
        float left   = 0.25f;
        float right  = 0.25f;
        float top    = 0.25f;
        float bottom = 0.25f;
    };

    // EditorLayout is only a wrapper for ImGui's docking builder.
    // It is created to fit AppKit's style of data and event hanlding.
    struct EditorLayout
    {
    public:
        explicit EditorLayout(const EditorLayoutDescriptor& descriptor);

        void bind(Application& app);

        void update(Blackboard& blackboard);

    private:
        EditorLayoutInfo init() const;

        // editor mode has support for docking on every direction
        EditorLayoutInfo create_editor_layout(const EditorLayoutDescriptor& desc) const;

    private:
        EditorLayoutDescriptor descriptor = {};
    };

} // namespace lyra

#endif // LYRA_LIBRARY_ENGINE_SYSTEM_LAYOUT_MANAGER_H
