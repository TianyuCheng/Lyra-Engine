#include <Lyra/Scripting/ScriptContext.h>
#include <Lyra/Windowing/WSIState.h>
#include <Lyra/Windowing/WSITypes.h>
#include <Lyra/Runtime/AppTypes.h>
#include <Lyra/Runtime/ScriptLayer.h>
#include <Lyra/Runtime/TimingLayer.h>

using namespace lyra;

ScriptContext::ScriptContext(AppContext& context, ScriptLayer& layer)
{
    world         = context.try_tool<World>();
    cmd_queue     = &layer.get_command_queue();
    scratch_arena = &layer.get_scratch_arena();

    Clock* clock = context.try_tool<Clock>();
    if (clock) {
        delta_time = clock->delta_time;
        total_time = clock->total_time;
    }

    Window* window = context.try_tool<Window>();
    if (window) {
        input_state = &window->get_input_state();
    }
}

ScriptContext::ScriptContext(World* world, ScriptCommandQueue* queue, MemoryArena* scratch, float dt, float time, const WindowInput* input)
    : world(world), cmd_queue(queue), scratch_arena(scratch), input_state(input), delta_time(dt), total_time(time)
{
    // do nothing else
}

bool ScriptContext::is_key_down(KeyButton key) const
{
    if (!input_state) return false;
    return input_state->is_key_down(key);
}

bool ScriptContext::is_key_pressed(KeyButton key) const
{
    if (!input_state) return false;
    return input_state->is_key_pressed(key);
}

bool ScriptContext::is_key_released(KeyButton key) const
{
    if (!input_state) return false;
    return input_state->is_key_released(key);
}

bool ScriptContext::is_mouse_down(MouseButton button) const
{
    if (!input_state) return false;
    return input_state->is_mouse_down(button);
}

bool ScriptContext::is_mouse_pressed(MouseButton button) const
{
    if (!input_state) return false;
    return input_state->is_mouse_pressed(button);
}

bool ScriptContext::is_mouse_released(MouseButton button) const
{
    if (!input_state) return false;
    return input_state->is_mouse_released(button);
}

Vector2 ScriptContext::mouse_position() const
{
    if (!input_state) return Vector2(0.0f);
    return input_state->get_mouse_position();
}

Vector2 ScriptContext::mouse_delta() const
{
    if (!input_state) return Vector2(0.0f);
    return input_state->get_mouse_delta();
}

Vector2 ScriptContext::mouse_scroll() const
{
    if (!input_state) return Vector2(0.0f);
    return input_state->get_mouse_scroll();
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
