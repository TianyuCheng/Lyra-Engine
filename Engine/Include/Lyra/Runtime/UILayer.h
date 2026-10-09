#pragma once

#ifndef LYRA_ENGINE_RUNTIME_UI_LAYER_H
#define LYRA_ENGINE_RUNTIME_UI_LAYER_H

#include <Lyra/UISystem/Renderer/GUITypes.h>

// local import
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    /**
     * @brief The UILayer struct manages the UI system for applications.
     */
    struct UILayer
    {
    public:
        /**
         * @brief Construct the UILayer with a GUIDescriptor.
         */
        explicit UILayer(const GUIDescriptor& descriptor);

        /**
         * @brief Register the GUIRenderer to the toolboard and bind its lifecycle events.
         */
        void bind(Application& app);

        /**
         * @brief Main update loop for the GUI.
         */
        void update(AppContext&);

        /**
         * @brief Prepare the GUI system for a new frame.
         */
        void pre_update(AppContext&);

        /**
         * @brief Finalize GUI updates for the current frame.
         */
        void post_update(AppContext&);

        /**
         * @brief Main GUI rendering step.
         */
        void render(AppContext&);

        /**
         * @brief Handle window resize events for the GUI.
         */
        void resize(AppContext&);

        /**
         * @brief Apply a default theme to the GUI.
         */
        void theme(AppContext&);

        /**
         * @brief Retrieve the raw underlying GUI context pointer.
         * @return A pointer to the current GUI context.
         */
        auto context() const -> void*;

        /**
         * @brief Set the GUI context as current for the current thread/library.
         */
        void apply_context() const;

    private:
        GUIDescriptor              descriptor; ///< The GUI initialization descriptor.
        OwnedResource<GUIRenderer> gui;        ///< The managed GUIRenderer resource.
    };

    using EditorLayer = UILayer;

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_UI_LAYER_H
