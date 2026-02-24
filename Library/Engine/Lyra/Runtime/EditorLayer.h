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
    /**
     * @brief The EditorLayer struct manages the Dear ImGui GUI system for the engine editor.
     */
    struct EditorLayer
    {
    public:
        /**
         * @brief Construct the EditorLayer with a GUIDescriptor.
         */
        explicit EditorLayer(const GUIDescriptor& descriptor);

        /**
         * @brief Register the GUIRenderer to the blackboard and bind its lifecycle events.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for the GUI.
         */
        void update(Blackboard&);

        /**
         * @brief Prepare the GUI system for a new frame.
         */
        void pre_update(Blackboard&);

        /**
         * @brief Finalize GUI updates for the current frame.
         */
        void post_update(Blackboard&);

        /**
         * @brief Main GUI rendering step.
         */
        void render(Blackboard&);

        /**
         * @brief Handle window resize events for the GUI.
         */
        void resize(Blackboard&);

        /**
         * @brief Apply a default theme to the GUI.
         */
        void theme(Blackboard&);

        /**
         * @brief Retrieve the raw ImGuiContext.
         * @return A pointer to the current ImGuiContext.
         */
        FORCE_INLINE auto context() -> ImGuiContext*
        {
            return reinterpret_cast<ImGuiContext*>(gui->context<ImGuiContext>());
        }

        /**
         * @brief Set the ImGui context as current for the current thread/library.
         * This must be called in any library or executable that uses ImGui.
         */
        FORCE_INLINE void apply_context()
        {
            ImGui::SetCurrentContext(context());
        }

    private:
        GUIDescriptor              descriptor; ///< The GUI initialization descriptor.
        OwnedResource<GUIRenderer> gui;        ///< The managed GUIRenderer resource.
    };

} // namespace lyra

#endif // LYRA_LIBRARY_RUNTIME_EDITOR_LAYER_H
