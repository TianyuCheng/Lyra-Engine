#pragma once

#ifndef LYRA_ENGINE_INPUT_SYSTEM_INPUT_FILTER_H
#define LYRA_ENGINE_INPUT_SYSTEM_INPUT_FILTER_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>

namespace lyra
{
    /**
     * @brief priority levels for input consumption and filtering.
     * higher priority levels evaluate first and have right-of-first-refusal over inputs.
     */
    enum struct InputPriority : uint
    {
        EDITOR   = 0, ///< layer 1: application chrome, menus, modals, text fields.
        OVERLAY  = 1, ///< layer 2: in-scene tools, gizmos (ImGuizmo), viewport HUD.
        VIEWPORT = 2  ///< layer 3: viewport canvas navigation, picking, camera control.
    };

    /**
     * @brief binary input gating filter passed to InputManager.
     */
    struct InputFilter
    {
        bool block_mouse    = false;
        bool block_keyboard = false;

        constexpr bool is_all_blocked() const
        {
            return block_mouse && block_keyboard;
        }

        constexpr auto combine(const InputFilter& other) -> InputFilter&
        {
            block_mouse    = block_mouse || other.block_mouse;
            block_keyboard = block_keyboard || other.block_keyboard;
            return *this;
        }
    };

    /**
     * @brief priority pipeline evaluating input filters in top-down order.
     */
    struct FilterPipeline
    {
    public:
        using Callback = Function<InputFilter(const InputFilter& current) const>;

        static constexpr usize PRIORITY_COUNT = 3;

        void set_filter(InputPriority priority, Callback callback)
        {
            auto index = static_cast<usize>(priority);
            if (index < PRIORITY_COUNT) {
                filters.at(index) = std::move(callback);
            }
        }

        void set_filter(InputPriority priority, Function<InputFilter() const> provider)
        {
            set_filter(priority, [p = std::move(provider)](const InputFilter&) {
                return p ? p() : InputFilter{};
            });
        }

        void clear_filter(InputPriority priority)
        {
            auto index = static_cast<usize>(priority);
            if (index < PRIORITY_COUNT) {
                filters.at(index) = nullptr;
            }
        }

        void clear_all()
        {
            for (auto& filter : filters) {
                filter = nullptr;
            }
        }

        auto evaluate() const -> InputFilter
        {
            InputFilter accumulated{};
            for (usize i = 0; i < PRIORITY_COUNT; ++i) {
                if (const auto& filter_fn = filters.at(i)) {
                    accumulated.combine(filter_fn(accumulated));
                    if (accumulated.is_all_blocked()) {
                        break;
                    }
                }
            }
            return accumulated;
        }

    private:
        Array<Callback, PRIORITY_COUNT> filters = {};
    };

    /**
     * @brief interface for viewports providing input interaction queries to InputLayer.
     */
    struct ViewportInputProvider
    {
        virtual ~ViewportInputProvider()       = default;
        virtual bool is_canvas_hovered() const = 0;
        virtual bool is_overlay_interacting() const { return false; }
        virtual bool is_mouse_nav_active() const    = 0;
        virtual bool is_keyboard_nav_active() const = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_INPUT_SYSTEM_INPUT_FILTER_H
