#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/InputLayer.h>
#include <Lyra/Runtime/TimingLayer.h>

using namespace lyra;

static bool is_button_down(const WindowInput* raw_input, const DeviceButton& button)
{
    if (!raw_input) return false;
    if (button.type == DeviceButton::Type::KEY) {
        return raw_input->is_key_down(static_cast<KeyButton>(button.code));
    }
    if (button.type == DeviceButton::Type::MOUSE) {
        return raw_input->is_mouse_down(static_cast<MouseButton>(button.code));
    }
    return false;
}

InputManager::InputManager()
{
    for (auto& state : action_states) {
        state = {false, false};
    }
    for (auto& val : axis_1d_values) {
        val = 0.0f;
    }
    for (auto& val : axis_2d_values) {
        val = Vector2(0.0f);
    }
    set_default_bindings();
}

void InputManager::set_default_bindings()
{
    for (auto& bindings : action_bindings) {
        bindings.clear();
    }

    // directional movement
    bind_action(InputAction::MOVE_FORWARD, DeviceButton::key(KeyButton::W));
    bind_action(InputAction::MOVE_FORWARD, DeviceButton::key(KeyButton::UP));
    bind_action(InputAction::MOVE_BACKWARD, DeviceButton::key(KeyButton::S));
    bind_action(InputAction::MOVE_BACKWARD, DeviceButton::key(KeyButton::DOWN));
    bind_action(InputAction::MOVE_LEFT, DeviceButton::key(KeyButton::A));
    bind_action(InputAction::MOVE_LEFT, DeviceButton::key(KeyButton::LEFT));
    bind_action(InputAction::MOVE_RIGHT, DeviceButton::key(KeyButton::D));
    bind_action(InputAction::MOVE_RIGHT, DeviceButton::key(KeyButton::RIGHT));
    bind_action(InputAction::MOVE_UP, DeviceButton::key(KeyButton::SPACE));
    bind_action(InputAction::MOVE_UP, DeviceButton::key(KeyButton::E));
    bind_action(InputAction::MOVE_DOWN, DeviceButton::key(KeyButton::CTRL));
    bind_action(InputAction::MOVE_DOWN, DeviceButton::key(KeyButton::Q));

    // modifiers
    bind_action(InputAction::SPRINT, DeviceButton::key(KeyButton::SHIFT));

    // common actions
    bind_action(InputAction::JUMP, DeviceButton::key(KeyButton::SPACE));
    bind_action(InputAction::CROUCH, DeviceButton::key(KeyButton::CTRL));
    bind_action(InputAction::CROUCH, DeviceButton::key(KeyButton::C));
    bind_action(InputAction::ATTACK, DeviceButton::mouse(MouseButton::LEFT));
    bind_action(InputAction::ATTACK_ALT, DeviceButton::mouse(MouseButton::RIGHT));
    bind_action(InputAction::INTERACT, DeviceButton::key(KeyButton::F));
    bind_action(InputAction::INTERACT, DeviceButton::key(KeyButton::E));
    bind_action(InputAction::USE, DeviceButton::key(KeyButton::E));
    bind_action(InputAction::RELOAD, DeviceButton::key(KeyButton::R));

    // camera & navigation
    bind_action(InputAction::LOOK_ACTIVATE, DeviceButton::mouse(MouseButton::RIGHT));
    bind_action(InputAction::LOOK_ACTIVATE, DeviceButton::mouse(MouseButton::LEFT));
    bind_action(InputAction::LOOK_ACTIVATE, DeviceButton::mouse(MouseButton::MIDDLE));
    bind_action(InputAction::CANCEL, DeviceButton::key(KeyButton::ESC));
    bind_action(InputAction::CONFIRM, DeviceButton::key(KeyButton::ENTER));
    bind_action(InputAction::CONFIRM, DeviceButton::key(KeyButton::SPACE));

    // 1d axes
    Axis1DComposite horizontal{};
    horizontal.positive.push_back(DeviceButton::key(KeyButton::D));
    horizontal.positive.push_back(DeviceButton::key(KeyButton::RIGHT));
    horizontal.negative.push_back(DeviceButton::key(KeyButton::A));
    horizontal.negative.push_back(DeviceButton::key(KeyButton::LEFT));
    horizontal.sensitivity = 1.0f;
    bind_axis_1d(InputAxis::HORIZONTAL, horizontal);

    Axis1DComposite vertical{};
    vertical.positive.push_back(DeviceButton::key(KeyButton::W));
    vertical.positive.push_back(DeviceButton::key(KeyButton::UP));
    vertical.negative.push_back(DeviceButton::key(KeyButton::S));
    vertical.negative.push_back(DeviceButton::key(KeyButton::DOWN));
    vertical.sensitivity = 1.0f;
    bind_axis_1d(InputAxis::VERTICAL, vertical);

    Axis1DComposite elevation{};
    elevation.positive.push_back(DeviceButton::key(KeyButton::SPACE));
    elevation.positive.push_back(DeviceButton::key(KeyButton::E));
    elevation.negative.push_back(DeviceButton::key(KeyButton::CTRL));
    elevation.negative.push_back(DeviceButton::key(KeyButton::Q));
    elevation.sensitivity = 1.0f;
    bind_axis_1d(InputAxis::ELEVATION, elevation);

    Axis1DComposite zoom{};
    zoom.sensitivity = 1.0f;
    bind_axis_1d(InputAxis::ZOOM, zoom);

    // 2d axes
    Axis2DComposite move{};
    move.up.push_back(DeviceButton::key(KeyButton::W));
    move.up.push_back(DeviceButton::key(KeyButton::UP));
    move.down.push_back(DeviceButton::key(KeyButton::S));
    move.down.push_back(DeviceButton::key(KeyButton::DOWN));
    move.left.push_back(DeviceButton::key(KeyButton::A));
    move.left.push_back(DeviceButton::key(KeyButton::LEFT));
    move.right.push_back(DeviceButton::key(KeyButton::D));
    move.right.push_back(DeviceButton::key(KeyButton::RIGHT));
    move.normalize = true;
    bind_axis_2d(InputAxis2D::MOVE, move);

    Axis2DComposite look{};
    look.normalize = false;
    bind_axis_2d(InputAxis2D::LOOK, look);
}

void InputManager::bind_action(InputAction action, DeviceButton button)
{
    auto idx = static_cast<size_t>(action);
    if (idx >= ACTION_COUNT) return;
    auto& list = action_bindings[idx];
    for (const auto& existing : list) {
        if (existing == button) return;
    }
    list.push_back(button);
}

void InputManager::clear_action(InputAction action)
{
    auto idx = static_cast<size_t>(action);
    if (idx < ACTION_COUNT) {
        action_bindings[idx].clear();
    }
}

void InputManager::clear_actions()
{
    for (auto& bindings : action_bindings) {
        bindings.clear();
    }
}

void InputManager::bind_axis_1d(InputAxis axis, const Axis1DComposite& composite)
{
    auto idx = static_cast<size_t>(axis);
    if (idx < AXIS_1D_COUNT) {
        axis_1d_bindings[idx] = composite;
    }
}

void InputManager::clear_axis_1d(InputAxis axis)
{
    auto idx = static_cast<size_t>(axis);
    if (idx < AXIS_1D_COUNT) {
        axis_1d_bindings[idx] = Axis1DComposite{};
    }
}

void InputManager::clear_axes_1d()
{
    for (auto& binding : axis_1d_bindings) {
        binding = Axis1DComposite{};
    }
}

void InputManager::bind_axis_2d(InputAxis2D axis, const Axis2DComposite& composite)
{
    auto idx = static_cast<size_t>(axis);
    if (idx < AXIS_2D_COUNT) {
        axis_2d_bindings[idx] = composite;
    }
}

void InputManager::clear_axis_2d(InputAxis2D axis)
{
    auto idx = static_cast<size_t>(axis);
    if (idx < AXIS_2D_COUNT) {
        axis_2d_bindings[idx] = Axis2DComposite{};
    }
}

void InputManager::clear_axes_2d()
{
    for (auto& binding : axis_2d_bindings) {
        binding = Axis2DComposite{};
    }
}

void InputManager::clear_all_bindings()
{
    clear_actions();
    clear_axes_1d();
    clear_axes_2d();
}

bool InputManager::is_action_down(InputAction action) const
{
    auto idx = static_cast<size_t>(action);
    if (idx >= ACTION_COUNT) return false;
    return action_states[idx].current;
}

bool InputManager::is_action_pressed(InputAction action) const
{
    auto idx = static_cast<size_t>(action);
    if (idx >= ACTION_COUNT) return false;
    return action_states[idx].current && !action_states[idx].previous;
}

bool InputManager::is_action_released(InputAction action) const
{
    auto idx = static_cast<size_t>(action);
    if (idx >= ACTION_COUNT) return false;
    return !action_states[idx].current && action_states[idx].previous;
}

float InputManager::get_axis(InputAxis axis) const
{
    auto idx = static_cast<size_t>(axis);
    if (idx >= AXIS_1D_COUNT) return 0.0f;
    return axis_1d_values[idx];
}

Vector2 InputManager::get_axis_2d(InputAxis2D axis) const
{
    auto idx = static_cast<size_t>(axis);
    if (idx >= AXIS_2D_COUNT) return Vector2(0.0f);
    return axis_2d_values[idx];
}

void InputManager::update(const WindowInput* raw_input, float dt)
{
    // update digital actions
    for (size_t i = 0; i < ACTION_COUNT; ++i) {
        action_states[i].previous = action_states[i].current;
        bool is_down              = false;
        if (raw_input) {
            for (const auto& btn : action_bindings[i]) {
                if (is_button_down(raw_input, btn)) {
                    is_down = true;
                    break;
                }
            }
        }
        action_states[i].current = is_down;
    }

    // evaluate 1d composite axes
    for (size_t i = 0; i < AXIS_1D_COUNT; ++i) {
        const auto& comp = axis_1d_bindings[i];
        float       val  = 0.0f;
        if (raw_input) {
            for (const auto& btn : comp.positive) {
                if (is_button_down(raw_input, btn)) {
                    val += 1.0f;
                    break;
                }
            }
            for (const auto& btn : comp.negative) {
                if (is_button_down(raw_input, btn)) {
                    val -= 1.0f;
                    break;
                }
            }
        }
        val *= comp.sensitivity;

        if (static_cast<InputAxis>(i) == InputAxis::ZOOM && raw_input) {
            val += raw_input->get_mouse_scroll().y;
        }
        axis_1d_values[i] = val;
    }

    // evaluate 2d composite axes
    for (size_t i = 0; i < AXIS_2D_COUNT; ++i) {
        const auto& comp = axis_2d_bindings[i];
        Vector2     val(0.0f);
        if (raw_input) {
            for (const auto& btn : comp.up) {
                if (is_button_down(raw_input, btn)) {
                    val.y += 1.0f;
                    break;
                }
            }
            for (const auto& btn : comp.down) {
                if (is_button_down(raw_input, btn)) {
                    val.y -= 1.0f;
                    break;
                }
            }
            for (const auto& btn : comp.left) {
                if (is_button_down(raw_input, btn)) {
                    val.x -= 1.0f;
                    break;
                }
            }
            for (const auto& btn : comp.right) {
                if (is_button_down(raw_input, btn)) {
                    val.x += 1.0f;
                    break;
                }
            }
        }

        if (static_cast<InputAxis2D>(i) == InputAxis2D::LOOK) {
            if (raw_input) {
                val += raw_input->get_mouse_delta();
            }
        } else if (comp.normalize && glm::dot(val, val) > 1.0f) {
            val = glm::normalize(val);
        }
        axis_2d_values[i] = val;
    }
}

InputLayer::InputLayer()
{
    // do nothing
}

void InputLayer::bind(Application& app)
{
    app.get_toolboard().add<InputManager*>(&manager);
    app.get_toolboard().add<InputLayer*>(this);
    app.bind<AppEvent::UPDATE_PRE, &InputLayer::update>(*this);
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

    manager.update(raw_input, dt);
}
