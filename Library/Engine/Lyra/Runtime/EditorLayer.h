#pragma once

#ifndef LYRA_LIBRARY_RUNTIME_EDITOR_LAYER_H
#define LYRA_LIBRARY_RUNTIME_EDITOR_LAYER_H

#include <imgui.h>
#include <imgui_internal.h>

#include <Lyra/UICore/GUITypes.h>

// local import
#include "Application.h"

namespace lyra
{
    // EditorLayer is only a wrapper around GUIRenderer.
    // It is created to handle application events.
    struct EditorLayer
    {
    public:
        explicit EditorLayer(const GUIDescriptor& descriptor);

        void bind(Application& app);

        void update(Blackboard&);

        void pre_update(Blackboard&);

        void post_update(Blackboard&);

        void render(Blackboard&);

        void resize(Blackboard&);

        void theme(Blackboard&);

        // provide a way to retrieve the raw ImGuiContext
        FORCE_INLINE auto context() -> ImGuiContext*
        {
            return reinterpret_cast<ImGuiContext*>(gui->context<ImGuiContext>());
        }

        // apply_context() is required to be called from user application
        // because currently we created GUIRenderer in Lyra-Engine.dll.
        // User application will need ImGuiContext* if it intends to call
        // any ImGui functions.
        FORCE_INLINE void apply_context()
        {
            ImGui::SetCurrentContext(context());
        }

    private:
        GUIDescriptor              descriptor;
        OwnedResource<GUIRenderer> gui;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RUNTIME_EDITOR_LAYER_H
