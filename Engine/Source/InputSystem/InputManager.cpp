#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/InputSystem/InputManager.h>

using namespace lyra;

static bool is_button_down(const WindowInput* raw_input, const DeviceButton& button, const InputFilter& filter)
{
    if (!raw_input || !button.valid()) return false;
    if (button.type == DeviceButton::Type::KEY) {
        if (filter.block_keyboard) return false;
        return raw_input->is_key_down(static_cast<KeyButton>(button.code));
    }
    if (button.type == DeviceButton::Type::MOUSE) {
        if (filter.block_mouse) return false;
        return raw_input->is_mouse_down(static_cast<MouseButton>(button.code));
    }
    return false;
}

static bool is_chord_down(const WindowInput* raw_input, const ButtonChord& chord, const InputFilter& filter)
{
    if (!raw_input || !chord.valid()) return false;

    // primary button must be down
    if (!is_button_down(raw_input, chord.primary, filter)) {
        return false;
    }

    // if extra button is specified, it must also be down
    if (chord.extra.valid() && !is_button_down(raw_input, chord.extra, filter)) {
        return false;
    }

    // evaluate modifier keys
    if (chord.modifiers.value != 0) {
        if (filter.block_keyboard) return false;

        if (chord.modifiers.contains(ModifierKey::SHIFT) && !raw_input->is_key_down(KeyButton::SHIFT)) {
            return false;
        }
        if (chord.modifiers.contains(ModifierKey::CTRL) && !raw_input->is_key_down(KeyButton::CTRL)) {
            return false;
        }
        if (chord.modifiers.contains(ModifierKey::ALT) && !raw_input->is_key_down(KeyButton::ALT)) {
            return false;
        }
        if (chord.modifiers.contains(ModifierKey::SUPER) && !raw_input->is_key_down(KeyButton::SUPER)) {
            return false;
        }
    }

    return true;
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
    bind_action(InputAction::MOVE_UP, DeviceButton::key(KeyButton::E));
    bind_action(InputAction::MOVE_DOWN, DeviceButton::key(KeyButton::Q));

    // modifiers
    bind_action(InputAction::SPRINT, DeviceButton::key(KeyButton::SHIFT));

    // common actions (space and ctrl removed to avoid conflicts with shortcuts and camera navigation)
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
    elevation.positive.push_back(DeviceButton::key(KeyButton::E));
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

void InputManager::bind_action(InputAction action, ButtonChord chord)
{
    auto idx = static_cast<size_t>(action);
    if (idx >= ACTION_COUNT) return;
    auto& list = action_bindings[idx];
    for (const auto& existing : list) {
        if (existing == chord) return;
    }
    list.push_back(chord);
}

void InputManager::bind_action(InputAction action, DeviceButton button)
{
    bind_action(action, ButtonChord(button));
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
    action_aliases.clear();
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

bool InputManager::is_custom_action_down(size_t index) const
{
    if (index >= CUSTOM_ACTION_COUNT) return false;
    return is_action_down(custom_action(index));
}

bool InputManager::is_custom_action_pressed(size_t index) const
{
    if (index >= CUSTOM_ACTION_COUNT) return false;
    return is_action_pressed(custom_action(index));
}

bool InputManager::is_custom_action_released(size_t index) const
{
    if (index >= CUSTOM_ACTION_COUNT) return false;
    return is_action_released(custom_action(index));
}

void InputManager::bind_custom_action(size_t index, ButtonChord chord)
{
    if (index >= CUSTOM_ACTION_COUNT) return;
    bind_action(custom_action(index), chord);
}

void InputManager::bind_custom_action(size_t index, DeviceButton button)
{
    bind_custom_action(index, ButtonChord(button));
}

void InputManager::clear_custom_action(size_t index)
{
    if (index >= CUSTOM_ACTION_COUNT) return;
    clear_action(custom_action(index));
}

void InputManager::register_action_alias(StringView name, InputAction action)
{
    action_aliases[String(name)] = action;
}

Optional<InputAction> InputManager::get_action_by_name(StringView name) const
{
    auto it = action_aliases.find(String(name));
    if (it != action_aliases.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool InputManager::is_action_down(StringView name) const
{
    if (auto act = get_action_by_name(name)) {
        return is_action_down(*act);
    }
    return false;
}

bool InputManager::is_action_pressed(StringView name) const
{
    if (auto act = get_action_by_name(name)) {
        return is_action_pressed(*act);
    }
    return false;
}

bool InputManager::is_action_released(StringView name) const
{
    if (auto act = get_action_by_name(name)) {
        return is_action_released(*act);
    }
    return false;
}

void InputManager::bind_action(StringView name, ButtonChord chord)
{
    if (auto act = get_action_by_name(name)) {
        bind_action(*act, chord);
    }
}

void InputManager::bind_action(StringView name, DeviceButton button)
{
    bind_action(name, ButtonChord(button));
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

float InputManager::get_custom_axis(size_t index) const
{
    if (index >= CUSTOM_AXIS_1D_COUNT) return 0.0f;
    return get_axis(custom_axis(index));
}

void InputManager::bind_custom_axis_1d(size_t index, const Axis1DComposite& composite)
{
    if (index >= CUSTOM_AXIS_1D_COUNT) return;
    bind_axis_1d(custom_axis(index), composite);
}

Vector2 InputManager::get_custom_axis_2d(size_t index) const
{
    if (index >= CUSTOM_AXIS_2D_COUNT) return Vector2(0.0f);
    return get_axis_2d(custom_axis_2d(index));
}

void InputManager::bind_custom_axis_2d(size_t index, const Axis2DComposite& composite)
{
    if (index >= CUSTOM_AXIS_2D_COUNT) return;
    bind_axis_2d(custom_axis_2d(index), composite);
}

void InputManager::reset()
{
    for (auto& state : action_states) {
        state.current  = false;
        state.previous = false;
    }
    for (auto& val : axis_1d_values) {
        val = 0.0f;
    }
    for (auto& val : axis_2d_values) {
        val = Vector2(0.0f);
    }
}

void InputManager::set_enabled(bool value)
{
    enabled = value;
    if (!enabled) {
        for (auto& state : action_states) {
            state.current = false;
        }
        for (auto& val : axis_1d_values) {
            val = 0.0f;
        }
        for (auto& val : axis_2d_values) {
            val = Vector2(0.0f);
        }
    }
}

void InputManager::update(const WindowInput* raw_input, float dt, const InputFilter& filter)
{
    // if input is gated off or raw input is null, zero all axes and release active actions
    if (!enabled || !raw_input) {
        for (size_t i = 0; i < ACTION_COUNT; ++i) {
            action_states[i].previous = action_states[i].current;
            action_states[i].current  = false;
        }
        for (auto& val : axis_1d_values) {
            val = 0.0f;
        }
        for (auto& val : axis_2d_values) {
            val = Vector2(0.0f);
        }
        return;
    }

    // update digital actions
    for (size_t i = 0; i < ACTION_COUNT; ++i) {
        action_states[i].previous = action_states[i].current;
        bool is_down              = false;
        for (const auto& chord : action_bindings[i]) {
            if (is_chord_down(raw_input, chord, filter)) {
                is_down = true;
                break;
            }
        }
        action_states[i].current = is_down;
    }

    // evaluate 1d composite axes
    for (size_t i = 0; i < AXIS_1D_COUNT; ++i) {
        const auto& comp = axis_1d_bindings[i];
        float       val  = 0.0f;
        for (const auto& chord : comp.positive) {
            if (is_chord_down(raw_input, chord, filter)) {
                val += 1.0f;
                break;
            }
        }
        for (const auto& chord : comp.negative) {
            if (is_chord_down(raw_input, chord, filter)) {
                val -= 1.0f;
                break;
            }
        }
        val *= comp.sensitivity;

        if (static_cast<InputAxis>(i) == InputAxis::ZOOM) {
            if (!filter.block_mouse) {
                val += raw_input->get_mouse_scroll().y;
            }
        }
        axis_1d_values[i] = val;
    }

    // evaluate 2d composite axes
    for (size_t i = 0; i < AXIS_2D_COUNT; ++i) {
        const auto& comp = axis_2d_bindings[i];
        Vector2     val(0.0f);
        for (const auto& chord : comp.up) {
            if (is_chord_down(raw_input, chord, filter)) {
                val.y += 1.0f;
                break;
            }
        }
        for (const auto& chord : comp.down) {
            if (is_chord_down(raw_input, chord, filter)) {
                val.y -= 1.0f;
                break;
            }
        }
        for (const auto& chord : comp.left) {
            if (is_chord_down(raw_input, chord, filter)) {
                val.x -= 1.0f;
                break;
            }
        }
        for (const auto& chord : comp.right) {
            if (is_chord_down(raw_input, chord, filter)) {
                val.x += 1.0f;
                break;
            }
        }

        if (static_cast<InputAxis2D>(i) == InputAxis2D::LOOK) {
            if (!filter.block_mouse) {
                val += raw_input->get_mouse_delta();
            }
        } else if (comp.normalize && glm::dot(val, val) > 1.0f) {
            val = glm::normalize(val);
        }
        axis_2d_values[i] = val;
    }
}
