#pragma once

#ifndef LYRA_EDITOR_COMMON_EDITOR_LAYOUT_H
#define LYRA_EDITOR_COMMON_EDITOR_LAYOUT_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/UISystem/Widgets/UIDock.h>
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    struct EditorLayoutDescriptor
    {
        ui::workspace::WorkspaceLayout layout = {};
        ui::workspace::DockSpaceFlags  flags  = ui::DockSpaceFlag::PassthruCentralNode;
    };

    struct EditorLayout
    {
    public:
        explicit EditorLayout(const EditorLayoutDescriptor& descriptor = {});

        void bind(Application& app);
        void update(AppContext& context);

    private:
        EditorLayoutDescriptor descriptor = {};
    };

} // namespace lyra

#endif // LYRA_EDITOR_COMMON_EDITOR_LAYOUT_H
