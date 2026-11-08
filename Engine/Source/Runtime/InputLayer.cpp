#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Runtime/InputLayer.h>
#include <Lyra/UISystem/Widgets/UI.h>

using namespace lyra;

InputLayer::InputLayer()
{
    // do nothing
}

void InputLayer::bind(Application& app)
{
    app.get_toolboard().add<InputManager*>(&manager);
    app.get_toolboard().add<InputLayer*>(this);
    app.bind<AppEvent::INIT, &InputLayer::init>(*this);
    app.bind<AppEvent::UPDATE_PRE, &InputLayer::update>(*this);
}

void InputLayer::init(AppContext& context)
{
    // layer 1: application chrome & global modals/text inputs
    filter_pipeline.set_filter(InputPriority::EDITOR, []() -> InputFilter {
        InputFilter filter{};
        if (ui::is_modal_active() || ui::is_text_input_active()) {
            filter.block_mouse    = true;
            filter.block_keyboard = true;
        }
        return filter;
    });

    // layer 2: in-scene viewport overlays & gizmos
    filter_pipeline.set_filter(InputPriority::OVERLAY, [&context](const InputFilter& current) -> InputFilter {
        InputFilter filter{};
        if (current.block_mouse) return filter; // already blocked by editor layer
        if (auto viewport = context.try_tool<ViewportInputProvider>()) {
            if (viewport->is_canvas_hovered() && viewport->is_overlay_interacting()) {
                filter.block_mouse = true;
            }
        }
        return filter;
    });

    // layer 3: viewport canvas navigation & hotkeys
    filter_pipeline.set_filter(InputPriority::VIEWPORT, [&context](const InputFilter&) -> InputFilter {
        InputFilter filter{};
        if (auto viewport = context.try_tool<ViewportInputProvider>()) {
            filter.block_mouse    = !viewport->is_mouse_nav_active();
            filter.block_keyboard = !viewport->is_keyboard_nav_active();
        }
        return filter;
    });
}

void InputLayer::update(AppContext& context)
{
    const WindowInput* raw_input = nullptr;
    if (Window* window = context.try_tool<Window>()) {
        raw_input = &window->get_input_state();
    }

    float dt = 0.0f;
    if (Clock* clock = context.try_tool<Clock>()) {
        dt = clock->delta_time;
    }

    InputFilter filter = filter_pipeline.evaluate();
    manager.update(raw_input, dt, filter);
}
