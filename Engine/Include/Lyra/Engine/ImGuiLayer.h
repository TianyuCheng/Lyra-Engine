#pragma once

#ifndef LYRA_LYRA_ENGINE_IMGUI_LAYER_H
#define LYRA_LYRA_ENGINE_IMGUI_LAYER_H

#include <imgui.h>
#include <imgui_internal.h>

#include <Lyra/UICore/GUITypes.h>

// local import
#include <Lyra/Engine/Application.h>

namespace lyra
{
    /**
     * @brief The ImGuiLayer struct manages the Dear ImGui GUI system for applications.
     */
    struct ImGuiLayer
    {
    public:
        /**
         * @brief Construct the ImGuiLayer with a GUIDescriptor.
         */
        explicit ImGuiLayer(const GUIDescriptor& descriptor);

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

    using EditorLayer = ImGuiLayer;

} // namespace lyra

#endif // LYRA_LYRA_ENGINE_IMGUI_LAYER_H
