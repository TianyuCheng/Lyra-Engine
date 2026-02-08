#include <Lyra/Window/WSIAPI.h>
#include <Lyra/Window/WSIEvent.h>
#include <Lyra/Window/WSIState.h>
#include <Lyra/Window/WSITypes.h>

using namespace lyra;

void InputState::update(const WindowHandle& handle)
{
    WindowInputQuery query{};
    Window::api()->query_input_events(handle, query);

    // loop over events and update input state
    for (uint i = 0; i < query.num_events; i++) {
        const auto& event = query.input_events.at(i);
        switch (event.type) {
            case WindowInputEvent::Type::MOUSE_BUTTON:
                mouse.status.at(static_cast<uint>(event.mouse_button.button)) = event.mouse_button.state;
                break;
            case WindowInputEvent::Type::KEY_BUTTON:
                keyboard.status.at(static_cast<uint>(event.key_button.button)) = event.key_button.state;
                break;
            default:
                // TODO: ignore other events for now
                break;
        }
    }
}

WindowInput::WindowInput()
{
    elapsed_time = std::chrono::steady_clock::now();
}

void WindowInput::update(const WindowHandle& handle)
{
    states[state_index].update(handle);
    state_index = (state_index + 1) % 2;

    auto timestamp = std::chrono::steady_clock::now();
    auto duration  = timestamp - elapsed_time;
    auto ms        = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    delta_time     = static_cast<float>(ms.count()) / 1000.0f;
    elapsed_time   = timestamp;
}

bool WindowInput::is_mouse_moved(MouseButton button) const
{
    auto& curr_mouse = current_state().mouse;
    auto& prev_mouse = previous_state().mouse;
    float delta_x    = std::abs(curr_mouse.position.xpos - prev_mouse.position.xpos);
    float delta_y    = std::abs(curr_mouse.position.ypos - prev_mouse.position.ypos);
    return (prev_mouse.status[(int)button] == ButtonState::OFF) &&
           (curr_mouse.status[(int)button] == ButtonState::OFF) &&
           ((delta_x > 1) || (delta_y > 1));
}

bool WindowInput::is_mouse_dragged(MouseButton button) const
{
    auto& curr_mouse = current_state().mouse;
    auto& prev_mouse = previous_state().mouse;
    float delta_x    = std::abs(curr_mouse.position.xpos - prev_mouse.position.xpos);
    float delta_y    = std::abs(curr_mouse.position.ypos - prev_mouse.position.ypos);
    return (prev_mouse.status[(int)button] == ButtonState::ON) &&
           (curr_mouse.status[(int)button] == ButtonState::ON) &&
           ((delta_x > 1) || (delta_y > 1));
}

bool WindowInput::is_mouse_pressed(MouseButton button) const
{
    auto& curr_mouse = current_state().mouse;
    auto& prev_mouse = previous_state().mouse;
    return (prev_mouse.status[(int)button] == ButtonState::OFF) &&
           (curr_mouse.status[(int)button] == ButtonState::ON);
}

bool WindowInput::is_mouse_released(MouseButton button) const
{
    auto& curr_mouse = current_state().mouse;
    auto& prev_mouse = previous_state().mouse;
    return (prev_mouse.status[(int)button] == ButtonState::ON) &&
           (curr_mouse.status[(int)button] == ButtonState::OFF);
}

bool WindowInput::is_key_down(KeyButton key) const
{
    auto& curr_keyboard = current_state().keyboard;
    return (curr_keyboard.status[(int)key] == ButtonState::ON);
}

bool WindowInput::is_key_pressed(KeyButton key) const
{
    auto& curr_keyboard = current_state().keyboard;
    auto& prev_keyboard = previous_state().keyboard;
    return (prev_keyboard.status[(int)key] == ButtonState::OFF) &&
           (curr_keyboard.status[(int)key] == ButtonState::ON);
}

bool WindowInput::is_key_released(KeyButton key) const
{
    auto& curr_keyboard = current_state().keyboard;
    auto& prev_keyboard = previous_state().keyboard;
    return (prev_keyboard.status[(int)key] == ButtonState::ON) &&
           (curr_keyboard.status[(int)key] == ButtonState::OFF);
}
