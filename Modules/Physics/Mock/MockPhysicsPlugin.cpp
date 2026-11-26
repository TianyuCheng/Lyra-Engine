#include "MockPhysicsPlugin.h"
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Sync.h>

using namespace lyra;

namespace
{
    Atomic<uint> g_handle_counter{1};

    template <typename T>
    auto make_mock_handle() -> T
    {
        uint id = g_handle_counter.fetch_add(1);
        return T(id);
    }

    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("MockPhysics", LogLevel::info);
        return LOGGER;
    }

    struct MockShapeData
    {
        ShapeType shape_type   = ShapeType::Box;
        Vector3   half_extents = Vector3(0.5f);
        float     radius       = 0.5f;
        float     height       = 1.0f;
    };

    struct MockBodyData
    {
        RigidBodyDescriptor desc;
        Vector3             position         = Vector3(0.0f);
        Quaternion          rotation         = glm::identity<Quaternion>();
        Vector3             linear_velocity  = Vector3(0.0f);
        Vector3             angular_velocity = Vector3(0.0f);
        bool                active           = true;
    };

    struct MockWorldData
    {
        PhysicsWorldDescriptor       desc;
        HashMap<uint, MockBodyData>  bodies;
        HashMap<uint, MockShapeData> shapes;
        Vector<ContactEvent>         contacts;
        Vector<TriggerEvent>         triggers;
    };

    HashMap<uint, MockWorldData> g_worlds;
    HashMap<uint, MockShapeData> g_global_shapes;

} // namespace

CString lyra::mockphysics::get_api_name()
{
    return "MockPhysics";
}

bool lyra::mockphysics::create_world(const PhysicsWorldDescriptor& desc, PhysicsWorldHandle& out_world)
{
    out_world = make_mock_handle<PhysicsWorldHandle>();
    MockWorldData world{};
    world.desc                = desc;
    g_worlds[out_world.value] = std::move(world);
    return true;
}

void lyra::mockphysics::delete_world(PhysicsWorldHandle world)
{
    g_worlds.erase(world.value);
}

void lyra::mockphysics::step_simulation(PhysicsWorldHandle world, float delta_time, uint /*collision_steps*/, JobScheduler* /*scheduler*/)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;

    auto& w = it->second;
    for (auto& [id, body] : w.bodies) {
        if (!body.active || body.desc.motion_type != MotionType::Dynamic) {
            continue;
        }
        // integrate simple gravity and velocity
        body.linear_velocity += w.desc.gravity * (body.desc.gravity_factor * delta_time);
        body.position += body.linear_velocity * delta_time;

        // apply simple linear damping
        body.linear_velocity *= std::max(0.0f, 1.0f - body.desc.linear_damping * delta_time);
        body.angular_velocity *= std::max(0.0f, 1.0f - body.desc.angular_damping * delta_time);
    }
}

bool lyra::mockphysics::create_material(PhysicsWorldHandle /*world*/, const PhysicsMaterialDescriptor& /*desc*/, PhysicsMaterialHandle& out_material)
{
    out_material = make_mock_handle<PhysicsMaterialHandle>();
    return true;
}

void lyra::mockphysics::release_material(PhysicsWorldHandle /*world*/, PhysicsMaterialHandle /*material*/)
{
}

bool lyra::mockphysics::create_box_shape(const Vector3& half_extents, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::Box;
    shape.half_extents               = half_extents;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_sphere_shape(float radius, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::Sphere;
    shape.radius                     = radius;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_capsule_shape(float half_height, float radius, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::Capsule;
    shape.radius                     = radius;
    shape.height                     = half_height * 2.0f;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_cylinder_shape(float half_height, float radius, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::Cylinder;
    shape.radius                     = radius;
    shape.height                     = half_height * 2.0f;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_convex_hull_shape(Span<Vector3> /*points*/, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::ConvexHull;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_triangle_mesh_shape(Span<Vector3> /*vertices*/, Span<uint> /*indices*/, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::TriangleMesh;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

bool lyra::mockphysics::create_compound_shape(Span<PhysicsShapeHandle> /*child_shapes*/, Span<Vector3> /*positions*/, Span<Quaternion> /*rotations*/, PhysicsShapeHandle& out_shape)
{
    out_shape = make_mock_handle<PhysicsShapeHandle>();
    MockShapeData shape{};
    shape.shape_type                 = ShapeType::Compound;
    g_global_shapes[out_shape.value] = shape;
    return true;
}

void lyra::mockphysics::release_shape(PhysicsShapeHandle shape)
{
    g_global_shapes.erase(shape.value);
}

bool lyra::mockphysics::create_body(PhysicsWorldHandle world, const RigidBodyDescriptor& desc, PhysicsBodyHandle& out_body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return false;

    out_body = make_mock_handle<PhysicsBodyHandle>();
    MockBodyData body{};
    body.desc             = desc;
    body.position         = desc.position;
    body.rotation         = desc.rotation;
    body.linear_velocity  = desc.linear_velocity;
    body.angular_velocity = desc.angular_velocity;
    body.active           = true;

    it->second.bodies[out_body.value] = body;
    return true;
}

void lyra::mockphysics::delete_body(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it != g_worlds.end()) {
        it->second.bodies.erase(body.value);
    }
}

void lyra::mockphysics::set_transform(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& pos, const Quaternion& rot)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return;
    bit->second.position = pos;
    bit->second.rotation = rot;
}

void lyra::mockphysics::get_transform(PhysicsWorldHandle world, PhysicsBodyHandle body, Vector3& out_pos, Quaternion& out_rot)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return;
    out_pos = bit->second.position;
    out_rot = bit->second.rotation;
}

void lyra::mockphysics::set_motion_type(PhysicsWorldHandle world, PhysicsBodyHandle body, MotionType motion_type)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return;
    bit->second.desc.motion_type = motion_type;
}

MotionType lyra::mockphysics::get_motion_type(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return MotionType::Static;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return MotionType::Static;
    return bit->second.desc.motion_type;
}

void lyra::mockphysics::add_force(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& force)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end() || bit->second.desc.motion_type != MotionType::Dynamic) return;
    if (bit->second.desc.mass > 0.0f) {
        bit->second.linear_velocity += (force / bit->second.desc.mass);
    }
}

void lyra::mockphysics::add_impulse(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& impulse)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end() || bit->second.desc.motion_type != MotionType::Dynamic) return;
    if (bit->second.desc.mass > 0.0f) {
        bit->second.linear_velocity += (impulse / bit->second.desc.mass);
    }
}

void lyra::mockphysics::set_linear_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& velocity)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return;
    bit->second.linear_velocity = velocity;
}

Vector3 lyra::mockphysics::get_linear_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return Vector3(0.0f);
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return Vector3(0.0f);
    return bit->second.linear_velocity;
}

void lyra::mockphysics::set_angular_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& angular_velocity)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return;
    bit->second.angular_velocity = angular_velocity;
}

Vector3 lyra::mockphysics::get_angular_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return Vector3(0.0f);
    auto bit = it->second.bodies.find(body.value);
    if (bit == it->second.bodies.end()) return Vector3(0.0f);
    return bit->second.angular_velocity;
}

bool lyra::mockphysics::is_active(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return false;
    auto bit = it->second.bodies.find(body.value);
    return (bit != it->second.bodies.end()) && bit->second.active;
}

void lyra::mockphysics::activate_body(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;
    auto bit = it->second.bodies.find(body.value);
    if (bit != it->second.bodies.end()) {
        bit->second.active = true;
    }
}

bool lyra::mockphysics::create_joint(PhysicsWorldHandle /*world*/, const JointDescriptor& /*desc*/, PhysicsJointHandle& out_joint)
{
    out_joint = make_mock_handle<PhysicsJointHandle>();
    return true;
}

void lyra::mockphysics::delete_joint(PhysicsWorldHandle /*world*/, PhysicsJointHandle /*joint*/)
{
}

bool lyra::mockphysics::raycast_closest(PhysicsWorldHandle world, const RaycastQuery& query, RaycastHit& out_hit)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return false;

    float closest_dist = query.max_distance;
    bool  found        = false;

    for (const auto& [id, body] : it->second.bodies) {
        uint layer_bit = 1u << body.desc.collision_layer;
        if ((query.layer_mask & layer_bit) == 0) continue;

        // simple sphere approximation for mock raycast
        Vector3 diff = body.position - query.origin;
        float   proj = glm::dot(diff, query.direction);
        if (proj < 0.0f || proj > closest_dist) continue;

        float perp_sq = glm::dot(diff, diff) - (proj * proj);
        float radius  = 1.0f;
        auto  sit     = g_global_shapes.find(body.desc.shape.value);
        if (sit != g_global_shapes.end()) {
            radius = sit->second.radius > 0.0f ? sit->second.radius : sit->second.half_extents.x;
        }

        if (perp_sq <= radius * radius) {
            closest_dist      = proj;
            out_hit.has_hit   = true;
            out_hit.distance  = proj;
            out_hit.position  = query.origin + query.direction * proj;
            out_hit.normal    = glm::normalize(out_hit.position - body.position);
            out_hit.body      = PhysicsBodyHandle(id);
            out_hit.user_data = body.desc.user_data;
            found             = true;
        }
    }
    return found;
}

void lyra::mockphysics::raycast_all(PhysicsWorldHandle world, const RaycastQuery& query, Vector<RaycastHit>& out_hits)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;

    for (const auto& [id, body] : it->second.bodies) {
        uint layer_bit = 1u << body.desc.collision_layer;
        if ((query.layer_mask & layer_bit) == 0) continue;

        Vector3 diff = body.position - query.origin;
        float   proj = glm::dot(diff, query.direction);
        if (proj < 0.0f || proj > query.max_distance) continue;

        float perp_sq = glm::dot(diff, diff) - (proj * proj);
        float radius  = 1.0f;
        auto  sit     = g_global_shapes.find(body.desc.shape.value);
        if (sit != g_global_shapes.end()) {
            radius = sit->second.radius > 0.0f ? sit->second.radius : sit->second.half_extents.x;
        }

        if (perp_sq <= radius * radius) {
            RaycastHit hit{};
            hit.has_hit   = true;
            hit.distance  = proj;
            hit.position  = query.origin + query.direction * proj;
            hit.normal    = glm::normalize(hit.position - body.position);
            hit.body      = PhysicsBodyHandle(id);
            hit.user_data = body.desc.user_data;
            out_hits.push_back(hit);
        }
    }
}

bool lyra::mockphysics::shape_sweep(PhysicsWorldHandle world, const ShapeSweepQuery& query, RaycastHit& out_hit)
{
    RaycastQuery ray{query.start_position, query.direction, query.max_distance, query.layer_mask};
    return raycast_closest(world, ray, out_hit);
}

void lyra::mockphysics::query_overlap_sphere(PhysicsWorldHandle world, const OverlapSphereQuery& query, Vector<PhysicsBodyHandle>& out_bodies)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;

    float r_sq = query.radius * query.radius;
    for (const auto& [id, body] : it->second.bodies) {
        uint layer_bit = 1u << body.desc.collision_layer;
        if ((query.layer_mask & layer_bit) == 0) continue;

        Vector3 diff = body.position - query.center;
        if (glm::dot(diff, diff) <= r_sq) {
            out_bodies.push_back(PhysicsBodyHandle(id));
        }
    }
}

void lyra::mockphysics::query_overlap_box(PhysicsWorldHandle world, const OverlapBoxQuery& query, Vector<PhysicsBodyHandle>& out_bodies)
{
    OverlapSphereQuery sphere_query{query.center, glm::length(query.half_extents), query.layer_mask};
    query_overlap_sphere(world, sphere_query, out_bodies);
}

bool lyra::mockphysics::create_character(PhysicsWorldHandle /*world*/, const CharacterDescriptor& /*desc*/, PhysicsCharacterHandle& out_character)
{
    out_character = make_mock_handle<PhysicsCharacterHandle>();
    return true;
}

void lyra::mockphysics::delete_character(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/)
{
}

void lyra::mockphysics::update_character(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/, const Vector3& /*desired_velocity*/, float /*delta_time*/, const Vector3& /*gravity*/)
{
}

void lyra::mockphysics::get_character_state(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/, Vector3& out_position, Quaternion& out_rotation, bool& out_is_on_ground)
{
    out_position     = Vector3(0.0f);
    out_rotation     = glm::identity<Quaternion>();
    out_is_on_ground = true;
}

void lyra::mockphysics::fetch_contact_events(PhysicsWorldHandle world, Vector<ContactEvent>& out_events)
{
    auto it = g_worlds.find(world.value);
    if (it != g_worlds.end()) {
        out_events = std::move(it->second.contacts);
        it->second.contacts.clear();
    }
}

void lyra::mockphysics::fetch_trigger_events(PhysicsWorldHandle world, Vector<TriggerEvent>& out_events)
{
    auto it = g_worlds.find(world.value);
    if (it != g_worlds.end()) {
        out_events = std::move(it->second.triggers);
        it->second.triggers.clear();
    }
}

void lyra::mockphysics::extract_debug_lines(PhysicsWorldHandle world, Vector<Vector3>& out_lines, Vector<uint>& out_colors)
{
    auto it = g_worlds.find(world.value);
    if (it == g_worlds.end()) return;

    for (const auto& [id, body] : it->second.bodies) {
        // draw a small coordinate cross for each body
        Vector3 p = body.position;
        out_lines.push_back(p);
        out_lines.push_back(p + Vector3(0.5f, 0.0f, 0.0f));
        out_colors.push_back(0xFF0000FF);
        out_colors.push_back(0xFF0000FF);

        out_lines.push_back(p);
        out_lines.push_back(p + Vector3(0.0f, 0.5f, 0.0f));
        out_colors.push_back(0xFF00FF00);
        out_colors.push_back(0xFF00FF00);

        out_lines.push_back(p);
        out_lines.push_back(p + Vector3(0.0f, 0.0f, 0.5f));
        out_colors.push_back(0xFFFF0000);
        out_colors.push_back(0xFFFF0000);
    }
}

void lyra::mockphysics::prepare() {}
void lyra::mockphysics::cleanup() {}

auto lyra::mockphysics::create() -> PhysicsAPI
{
    PhysicsAPI api{};
    api.get_api_name               = get_api_name;
    api.create_world               = create_world;
    api.delete_world               = delete_world;
    api.step_simulation            = step_simulation;
    api.create_material            = create_material;
    api.release_material           = release_material;
    api.create_box_shape           = create_box_shape;
    api.create_sphere_shape        = create_sphere_shape;
    api.create_capsule_shape       = create_capsule_shape;
    api.create_cylinder_shape      = create_cylinder_shape;
    api.create_convex_hull_shape   = create_convex_hull_shape;
    api.create_triangle_mesh_shape = create_triangle_mesh_shape;
    api.create_compound_shape      = create_compound_shape;
    api.release_shape              = release_shape;
    api.create_body                = create_body;
    api.delete_body                = delete_body;
    api.set_transform              = set_transform;
    api.get_transform              = get_transform;
    api.set_motion_type            = set_motion_type;
    api.get_motion_type            = get_motion_type;
    api.add_force                  = add_force;
    api.add_impulse                = add_impulse;
    api.set_linear_velocity        = set_linear_velocity;
    api.get_linear_velocity        = get_linear_velocity;
    api.set_angular_velocity       = set_angular_velocity;
    api.get_angular_velocity       = get_angular_velocity;
    api.is_active                  = is_active;
    api.activate_body              = activate_body;
    api.create_joint               = create_joint;
    api.delete_joint               = delete_joint;
    api.raycast_closest            = raycast_closest;
    api.raycast_all                = raycast_all;
    api.shape_sweep                = shape_sweep;
    api.query_overlap_sphere       = query_overlap_sphere;
    api.query_overlap_box          = query_overlap_box;
    api.create_character           = create_character;
    api.delete_character           = delete_character;
    api.update_character           = update_character;
    api.get_character_state        = get_character_state;
    api.fetch_contact_events       = fetch_contact_events;
    api.fetch_trigger_events       = fetch_trigger_events;
    api.extract_debug_lines        = extract_debug_lines;
    return api;
}
