#include <Lyra/Physics/PhysicsAPI.h>
#include <Lyra/Physics/PhysicsDescs.h>
#include <Lyra/Physics/PhysicsEvents.h>
#include <Lyra/Physics/PhysicsQueries.h>
#include <Lyra/Physics/PhysicsTypes.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Sync.h>

using namespace lyra;

// forward declarations for inlined plugins
FORWARD_DECLARE_API(lyra::mockphysics, PhysicsAPI)
FORWARD_DECLARE_API(lyra::jolt, PhysicsAPI)

using PhysicsPlugin        = Plugin<PhysicsAPI>;
using BuiltinPhysicsPlugin = BuiltinPlugin<PhysicsAPI>;

namespace
{
    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("Physics", LogLevel::info);
        return LOGGER;
    }

    struct PhysicsSlot
    {
        Own<PhysicsPlugin>        plugin;
        Own<BuiltinPhysicsPlugin> builtin_plugin;
        PhysicsAPI*               api          = nullptr;
        Optional<PhysicsBackend>  backend_type = std::nullopt;
        bool                      active       = false;
    };

    auto& get_slot_mutex()
    {
        static Mutex s_mtx;
        return s_mtx;
    }

    PhysicsSlot    g_slot;
    PhysicsBackend g_active_backend = PhysicsBackend::Jolt;
    PhysicsWorld   g_active_world;
} // namespace

#pragma region Physics Subsystem Manager

Result<void> Physics::init(PhysicsBackend backend)
{
    LockGuard<Mutex> lock(get_slot_mutex());

    if (g_slot.active && g_slot.backend_type == backend && g_slot.api) {
        return {};
    }

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api          = nullptr;
    g_slot.active       = false;
    g_slot.backend_type = backend;

    switch (backend) {
        case PhysicsBackend::Jolt:
        {
            g_slot.builtin_plugin = make_own<BuiltinPhysicsPlugin>(
                lyra::jolt::create,
                lyra::jolt::prepare,
                lyra::jolt::cleanup);
            g_slot.api = g_slot.builtin_plugin->get_api();
            if (!g_slot.api) {
                get_logger()->warn("Builtin Jolt plugin not available, attempting dynamic load...");
                g_slot.plugin = make_own<PhysicsPlugin>("lyra-jolt");
                g_slot.api    = g_slot.plugin->get_api();
            }
            break;
        }
        case PhysicsBackend::Mock:
            g_slot.builtin_plugin = make_own<BuiltinPhysicsPlugin>(
                lyra::mockphysics::create,
                lyra::mockphysics::prepare,
                lyra::mockphysics::cleanup);
            g_slot.api = g_slot.builtin_plugin->get_api();
            break;
        case PhysicsBackend::Custom:
        {
            get_logger()->error("Use Physics::init(StringView custom_backend) for custom plugins");
            return make_unexpected("Use Physics::init(StringView custom_backend) for custom plugins");
        }
    }

    if (!g_slot.api) {
        get_logger()->error("Failed to initialize physics backend");
        return make_unexpected("Failed to initialize physics backend");
    }

    g_slot.active    = true;
    g_active_backend = backend;
    get_logger()->info("Physics subsystem initialized with backend: {}", g_slot.api->get_api_name ? g_slot.api->get_api_name() : "unknown");
    return {};
}

Result<void> Physics::init(StringView custom_backend)
{
    LockGuard<Mutex> lock(get_slot_mutex());

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api          = nullptr;
    g_slot.active       = false;
    g_slot.backend_type = PhysicsBackend::Custom;

    g_slot.plugin = make_own<PhysicsPlugin>();
    String plugin_name(custom_backend);
    auto loaded = g_slot.plugin->load(plugin_name.c_str());
    if (!loaded) {
        get_logger()->error("Failed to load custom physics plugin: {}: {}", custom_backend, loaded.error());
        return make_unexpected("Failed to load custom physics plugin '" + plugin_name + "': " + loaded.error());
    }
    g_slot.api = g_slot.plugin->get_api();

    if (!g_slot.api) {
        get_logger()->error("Failed to get API from custom physics plugin: {}", custom_backend);
        return make_unexpected("Failed to get API from custom physics plugin: " + plugin_name);
    }

    g_slot.active    = true;
    g_active_backend = PhysicsBackend::Custom;
    get_logger()->info("Physics subsystem initialized with custom backend: {}", custom_backend);
    return {};
}

void Physics::shutdown()
{
    LockGuard<Mutex> lock(get_slot_mutex());

    if (g_active_world.valid()) {
        g_active_world.destroy();
    }

    g_slot.builtin_plugin.reset();
    g_slot.plugin.reset();
    g_slot.api    = nullptr;
    g_slot.active = false;
    get_logger()->info("Physics subsystem shut down");
}

PhysicsAPI* Physics::api()
{
    return g_slot.active ? g_slot.api : nullptr;
}

PhysicsBackend Physics::get_active_backend()
{
    return g_active_backend;
}

PhysicsWorld Physics::get_active_world()
{
    return g_active_world;
}

void Physics::set_active_world(PhysicsWorld world)
{
    g_active_world = world;
}

#pragma endregion

#pragma region PhysicsMaterial

void PhysicsMaterial::destroy()
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->release_material(world, handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region PhysicsShape

PhysicsShape PhysicsShape::create_box(const Vector3& half_extents)
{
    PhysicsShape s{};
    s.type = ShapeType::Box;
    if (Physics::api()) {
        Physics::api()->create_box_shape(half_extents, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_sphere(float radius)
{
    PhysicsShape s{};
    s.type = ShapeType::Sphere;
    if (Physics::api()) {
        Physics::api()->create_sphere_shape(radius, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_capsule(float half_height, float radius)
{
    PhysicsShape s{};
    s.type = ShapeType::Capsule;
    if (Physics::api()) {
        Physics::api()->create_capsule_shape(half_height, radius, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_cylinder(float half_height, float radius)
{
    PhysicsShape s{};
    s.type = ShapeType::Cylinder;
    if (Physics::api()) {
        Physics::api()->create_cylinder_shape(half_height, radius, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_convex_hull(Span<Vector3> points)
{
    PhysicsShape s{};
    s.type = ShapeType::ConvexHull;
    if (Physics::api()) {
        Physics::api()->create_convex_hull_shape(points, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_triangle_mesh(Span<Vector3> vertices, Span<uint> indices)
{
    PhysicsShape s{};
    s.type = ShapeType::TriangleMesh;
    if (Physics::api()) {
        Physics::api()->create_triangle_mesh_shape(vertices, indices, s.handle);
    }
    return s;
}

PhysicsShape PhysicsShape::create_compound(Span<PhysicsShapeHandle> shapes, Span<Vector3> positions, Span<Quaternion> rotations)
{
    PhysicsShape s{};
    s.type = ShapeType::Compound;
    if (Physics::api()) {
        Physics::api()->create_compound_shape(shapes, positions, rotations, s.handle);
    }
    return s;
}

void PhysicsShape::destroy()
{
    if (valid() && Physics::api()) {
        Physics::api()->release_shape(handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region PhysicsBody

void PhysicsBody::set_transform(const Vector3& pos, const Quaternion& rot)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->set_transform(world, handle, pos, rot);
    }
}

void PhysicsBody::get_transform(Vector3& out_pos, Quaternion& out_rot) const
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->get_transform(world, handle, out_pos, out_rot);
    }
}

Vector3 PhysicsBody::get_position() const
{
    Vector3    pos(0.0f);
    Quaternion rot = glm::identity<Quaternion>();
    get_transform(pos, rot);
    return pos;
}

Quaternion PhysicsBody::get_rotation() const
{
    Vector3    pos(0.0f);
    Quaternion rot = glm::identity<Quaternion>();
    get_transform(pos, rot);
    return rot;
}

void PhysicsBody::set_motion_type(MotionType motion)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->set_motion_type(world, handle, motion);
    }
}

MotionType PhysicsBody::get_motion_type() const
{
    if (valid() && world.valid() && Physics::api()) {
        return Physics::api()->get_motion_type(world, handle);
    }
    return MotionType::Static;
}

void PhysicsBody::add_force(const Vector3& force)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->add_force(world, handle, force);
    }
}

void PhysicsBody::add_impulse(const Vector3& impulse)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->add_impulse(world, handle, impulse);
    }
}

void PhysicsBody::set_linear_velocity(const Vector3& velocity)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->set_linear_velocity(world, handle, velocity);
    }
}

Vector3 PhysicsBody::get_linear_velocity() const
{
    if (valid() && world.valid() && Physics::api()) {
        return Physics::api()->get_linear_velocity(world, handle);
    }
    return Vector3(0.0f);
}

void PhysicsBody::set_angular_velocity(const Vector3& velocity)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->set_angular_velocity(world, handle, velocity);
    }
}

Vector3 PhysicsBody::get_angular_velocity() const
{
    if (valid() && world.valid() && Physics::api()) {
        return Physics::api()->get_angular_velocity(world, handle);
    }
    return Vector3(0.0f);
}

bool PhysicsBody::is_active() const
{
    if (valid() && world.valid() && Physics::api()) {
        return Physics::api()->is_active(world, handle);
    }
    return false;
}

void PhysicsBody::activate()
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->activate_body(world, handle);
    }
}

void PhysicsBody::destroy()
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->delete_body(world, handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region PhysicsCharacter

void PhysicsCharacter::update(const Vector3& desired_velocity, float delta_time, const Vector3& gravity)
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->update_character(world, handle, desired_velocity, delta_time, gravity);
    }
}

void PhysicsCharacter::get_state(Vector3& out_position, Quaternion& out_rotation, bool& out_is_on_ground) const
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->get_character_state(world, handle, out_position, out_rotation, out_is_on_ground);
    }
}

Vector3 PhysicsCharacter::get_position() const
{
    Vector3    pos(0.0f);
    Quaternion rot      = glm::identity<Quaternion>();
    bool       grounded = false;
    get_state(pos, rot, grounded);
    return pos;
}

bool PhysicsCharacter::is_on_ground() const
{
    Vector3    pos(0.0f);
    Quaternion rot      = glm::identity<Quaternion>();
    bool       grounded = false;
    get_state(pos, rot, grounded);
    return grounded;
}

void PhysicsCharacter::destroy()
{
    if (valid() && world.valid() && Physics::api()) {
        Physics::api()->delete_character(world, handle);
        handle.reset();
    }
}

#pragma endregion

#pragma region PhysicsWorld

PhysicsWorld PhysicsWorld::create()
{
    PhysicsWorldDescriptor desc{};
    return create(desc);
}

PhysicsWorld PhysicsWorld::create(const PhysicsWorldDescriptor& desc)
{
    PhysicsWorld w{};
    if (Physics::api() && Physics::api()->create_world) {
        Physics::api()->create_world(desc, w.handle);
    }
    return w;
}

void PhysicsWorld::destroy()
{
    if (valid() && Physics::api()) {
        Physics::api()->delete_world(handle);
        handle.reset();
    }
}

void PhysicsWorld::step(float delta_time, uint collision_steps)
{
    if (valid() && Physics::api()) {
        Physics::api()->step_simulation(handle, delta_time, collision_steps, nullptr);
    }
}

PhysicsBody PhysicsWorld::create_body(const RigidBodyDescriptor& desc)
{
    PhysicsBody b{};
    b.world = handle;
    if (valid() && Physics::api() && Physics::api()->create_body) {
        Physics::api()->create_body(handle, desc, b.handle);
    }
    return b;
}

PhysicsMaterial PhysicsWorld::create_material(const PhysicsMaterialDescriptor& desc)
{
    PhysicsMaterial m{};
    m.world = handle;
    if (valid() && Physics::api() && Physics::api()->create_material) {
        Physics::api()->create_material(handle, desc, m.handle);
    }
    return m;
}

PhysicsCharacter PhysicsWorld::create_character(const CharacterDescriptor& desc)
{
    PhysicsCharacter c{};
    c.world = handle;
    if (valid() && Physics::api() && Physics::api()->create_character) {
        Physics::api()->create_character(handle, desc, c.handle);
    }
    return c;
}

Optional<RaycastHit> PhysicsWorld::raycast(const RaycastQuery& query)
{
    if (!valid() || !Physics::api()) return std::nullopt;
    RaycastHit hit{};
    if (Physics::api()->raycast_closest(handle, query, hit)) {
        return hit;
    }
    return std::nullopt;
}

Vector<RaycastHit> PhysicsWorld::raycast_all(const RaycastQuery& query)
{
    Vector<RaycastHit> hits;
    if (valid() && Physics::api()) {
        Physics::api()->raycast_all(handle, query, hits);
    }
    return hits;
}

Optional<RaycastHit> PhysicsWorld::sweep(const ShapeSweepQuery& query)
{
    if (!valid() || !Physics::api()) return std::nullopt;
    RaycastHit hit{};
    if (Physics::api()->shape_sweep(handle, query, hit)) {
        return hit;
    }
    return std::nullopt;
}

Vector<PhysicsBody> PhysicsWorld::overlap_sphere(const OverlapSphereQuery& query)
{
    Vector<PhysicsBody> bodies;
    if (valid() && Physics::api()) {
        Vector<PhysicsBodyHandle> handles;
        Physics::api()->query_overlap_sphere(handle, query, handles);
        bodies.reserve(handles.size());
        for (const auto& h : handles) {
            PhysicsBody b{};
            b.handle = h;
            b.world  = handle;
            bodies.push_back(b);
        }
    }
    return bodies;
}

Vector<PhysicsBody> PhysicsWorld::overlap_box(const OverlapBoxQuery& query)
{
    Vector<PhysicsBody> bodies;
    if (valid() && Physics::api()) {
        Vector<PhysicsBodyHandle> handles;
        Physics::api()->query_overlap_box(handle, query, handles);
        bodies.reserve(handles.size());
        for (const auto& h : handles) {
            PhysicsBody b{};
            b.handle = h;
            b.world  = handle;
            bodies.push_back(b);
        }
    }
    return bodies;
}

Vector<ContactEvent> PhysicsWorld::fetch_contact_events()
{
    Vector<ContactEvent> events;
    if (valid() && Physics::api()) {
        Physics::api()->fetch_contact_events(handle, events);
    }
    return events;
}

Vector<TriggerEvent> PhysicsWorld::fetch_trigger_events()
{
    Vector<TriggerEvent> events;
    if (valid() && Physics::api()) {
        Physics::api()->fetch_trigger_events(handle, events);
    }
    return events;
}

void PhysicsWorld::extract_debug_lines(Vector<Vector3>& out_lines, Vector<uint>& out_colors)
{
    if (valid() && Physics::api()) {
        Physics::api()->extract_debug_lines(handle, out_lines, out_colors);
    }
}

#pragma endregion
