#include <Lyra/Scripting/ScriptContext.h>

using namespace lyra;

ScriptContext::ScriptContext(World* world, ScriptCommandQueue* queue, MemoryArena* scratch, float dt, float time)
    : world(world), cmd_queue(queue), scratch_arena(scratch), delta_time(dt), total_time(time)
{
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
    if (!world) {
        return;
    }
    world->rotate(node, axis, angle_deg);
}

void ScriptContext::rotate(SceneNode node, const Quaternion& rot)
{
    if (!world) {
        return;
    }
    world->rotate(node, rot);
}

void ScriptContext::translate(SceneNode node, const Vector3& translation)
{
    if (!world) {
        return;
    }
    world->translate(node, translation);
}

void ScriptContext::scale(SceneNode node, const Vector3& scale)
{
    if (!world) {
        return;
    }
    world->scale(node, scale);
}
