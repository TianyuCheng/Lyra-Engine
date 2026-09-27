#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/InputLayer.h>
#include <Lyra/Runtime/ScriptLayer.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Scripting/ScriptContext.h>

using namespace lyra;

ScriptContext::ScriptContext(AppContext& context, ScriptLayer& layer)
{
    world         = context.try_tool<World>();
    cmd_queue     = &layer.get_command_queue();
    scratch_arena = &layer.get_scratch_arena();

    if (Clock* clock = context.try_tool<Clock>()) {
        delta_time = clock->delta_time;
        total_time = clock->total_time;
    }

    if (Window* window = context.try_tool<Window>()) {
        input_state = &window->get_input_state();
    } else {
        input_state = context.try_tool<WindowInput>();
    }

    input_manager = context.try_tool<InputManager>();

    assert(cmd_queue != nullptr && "ScriptContext requires non-null ScriptCommandQueue");
    assert(scratch_arena != nullptr && "ScriptContext requires non-null MemoryArena");
    assert(input_state != nullptr && "ScriptContext requires Window or WindowInput in AppContext");
    assert(input_manager != nullptr && "ScriptContext requires InputManager in AppContext");
}

ScriptContext::ScriptContext(World* world, ScriptCommandQueue* queue, MemoryArena* scratch, const WindowInput* input, const InputManager* input_mgr, float dt, float time)
    : world(world), cmd_queue(queue), scratch_arena(scratch), input_state(input), input_manager(input_mgr), delta_time(dt), total_time(time)
{
    assert(cmd_queue != nullptr && "ScriptContext requires non-null ScriptCommandQueue");
    assert(scratch_arena != nullptr && "ScriptContext requires non-null MemoryArena");
    assert(input_state != nullptr && "ScriptContext requires non-null WindowInput");
    assert(input_manager != nullptr && "ScriptContext requires non-null InputManager");
}

bool ScriptContext::is_key_down(KeyButton key) const
{
    return input_state->is_key_down(key);
}

bool ScriptContext::is_key_pressed(KeyButton key) const
{
    return input_state->is_key_pressed(key);
}

bool ScriptContext::is_key_released(KeyButton key) const
{
    return input_state->is_key_released(key);
}

bool ScriptContext::is_mouse_down(MouseButton button) const
{
    return input_state->is_mouse_down(button);
}

bool ScriptContext::is_mouse_pressed(MouseButton button) const
{
    return input_state->is_mouse_pressed(button);
}

bool ScriptContext::is_mouse_released(MouseButton button) const
{
    return input_state->is_mouse_released(button);
}

Vector2 ScriptContext::mouse_position() const
{
    return input_state->get_mouse_position();
}

Vector2 ScriptContext::mouse_delta() const
{
    return input_state->get_mouse_delta();
}

Vector2 ScriptContext::mouse_scroll() const
{
    return input_state->get_mouse_scroll();
}

bool ScriptContext::is_action_down(InputAction action) const
{
    return input_manager->is_action_down(action);
}

bool ScriptContext::is_action_pressed(InputAction action) const
{
    return input_manager->is_action_pressed(action);
}

bool ScriptContext::is_action_released(InputAction action) const
{
    return input_manager->is_action_released(action);
}

float ScriptContext::get_axis(InputAxis axis) const
{
    return input_manager->get_axis(axis);
}

Vector2 ScriptContext::get_axis_2d(InputAxis2D axis) const
{
    return input_manager->get_axis_2d(axis);
}

void ScriptContext::rotate(TransformLocal& transform, const Vector3& axis, float angle_deg)
{
    rotate(transform, glm::angleAxis(glm::radians(angle_deg), axis));
}

void ScriptContext::rotate(TransformLocal& transform, const Quaternion& rot)
{
    transform.rotation *= rot;
    transform.flags |= TransformFlag::LOCAL_DIRTY;
}

void ScriptContext::translate(TransformLocal& transform, const Vector3& translation)
{
    transform.position += translation;
    transform.flags |= TransformFlag::LOCAL_DIRTY;
}

void ScriptContext::scale(TransformLocal& transform, const Vector3& scale)
{
    transform.scale *= scale;
    transform.flags |= TransformFlag::LOCAL_DIRTY;
}

void ScriptContext::rotate(SceneNode node, const Vector3& axis, float angle_deg)
{
    if (!world) return;
    world->rotate(node, axis, angle_deg);
}

void ScriptContext::rotate(SceneNode node, const Quaternion& rot)
{
    if (!world) return;
    world->rotate(node, rot);
}

void ScriptContext::translate(SceneNode node, const Vector3& translation)
{
    if (!world) return;
    world->translate(node, translation);
}

void ScriptContext::scale(SceneNode node, const Vector3& scale)
{
    if (!world) return;
    world->scale(node, scale);
}
