#include <Lyra/Utilities/Sync.h>
#include <memory>

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif

// Lyra includes
#include <Lyra/Physics/PhysicsAPI.h>
#include <Lyra/Physics/PhysicsLayers.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/Utilities/Collections.h>

using namespace lyra;

namespace
{
    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("JoltPhysics", LogLevel::info);
        return LOGGER;
    }

    Atomic<uint> g_handle_counter{1};

    template <typename T>
    auto make_handle() -> T
    {
        uint id = g_handle_counter.fetch_add(1);
        return T(id);
    }

    // math conversions
    FORCE_INLINE auto to_jph_vec3(const Vector3& v) -> JPH::Vec3
    {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    FORCE_INLINE auto to_jph_rvec3(const Vector3& v) -> JPH::RVec3
    {
        return JPH::RVec3(v.x, v.y, v.z);
    }

    FORCE_INLINE auto to_lyra_vec3(const JPH::Vec3& v) -> Vector3
    {
        return Vector3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
    }

    FORCE_INLINE auto to_jph_quat(const Quaternion& q) -> JPH::Quat
    {
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    FORCE_INLINE auto to_lyra_quat(const JPH::Quat& q) -> Quaternion
    {
        return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }

    // broadphase layers
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::BroadPhaseLayer TRIGGER(2);
        static constexpr uint                 NUM_LAYERS = 3;
    } // namespace BroadPhaseLayers

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            m_object_to_broadphase[static_cast<ushort>(CollisionLayer::NonMoving)] = BroadPhaseLayers::NON_MOVING;
            m_object_to_broadphase[static_cast<ushort>(CollisionLayer::Moving)]    = BroadPhaseLayers::MOVING;
            m_object_to_broadphase[static_cast<ushort>(CollisionLayer::Trigger)]   = BroadPhaseLayers::TRIGGER;
            m_object_to_broadphase[static_cast<ushort>(CollisionLayer::Character)] = BroadPhaseLayers::MOVING;
            m_object_to_broadphase[static_cast<ushort>(CollisionLayer::Debris)]    = BroadPhaseLayers::MOVING;
        }

        uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            if (inLayer < 32) {
                return m_object_to_broadphase[inLayer];
            }
            return BroadPhaseLayers::MOVING;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer) {
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
                    return "NON_MOVING";
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
                    return "MOVING";
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::TRIGGER:
                    return "TRIGGER";
                default:
                    return "INVALID";
            }
        }
#endif

    private:
        JPH::BroadPhaseLayer m_object_to_broadphase[32] = {BroadPhaseLayers::MOVING};
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            if (inLayer1 == static_cast<ushort>(CollisionLayer::NonMoving)) {
                return inLayer2 == BroadPhaseLayers::MOVING;
            }
            if (inLayer1 == static_cast<ushort>(CollisionLayer::Trigger)) {
                return inLayer2 == BroadPhaseLayers::MOVING;
            }
            return true;
        }
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            return m_matrix.can_collide(inObject1, inObject2) || m_matrix.can_trigger(inObject1, inObject2);
        }

        CollisionMatrix m_matrix;
    };

    struct WorldContext
    {
        PhysicsWorldDescriptor            desc;
        Own<JPH::PhysicsSystem>           physics_system;
        Own<JPH::TempAllocatorImpl>       temp_allocator;
        Own<JPH::JobSystemThreadPool>     job_system;
        BPLayerInterfaceImpl              bp_layer_interface;
        ObjectVsBroadPhaseLayerFilterImpl object_vs_bp_filter;
        ObjectLayerPairFilterImpl         object_layer_pair_filter;

        HashMap<uint, JPH::BodyID> handle_to_body;
        HashMap<uint, uint>        body_to_handle;
        HashMap<uint, ulong>       body_user_data;
    };

    HashMap<uint, Own<WorldContext>>    g_worlds;
    HashMap<uint, JPH::Ref<JPH::Shape>> g_shapes;

    void ensure_jolt_initialized()
    {
        static bool s_initialized = false;
        if (!s_initialized) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
            s_initialized = true;
            get_logger()->info("Jolt Physics 5.x runtime initialized successfully");
        }
    }

} // namespace

namespace api
{
    static CString get_api_name()
    {
        return "Jolt";
    }

    static bool create_world(const PhysicsWorldDescriptor& desc, PhysicsWorldHandle& out_world)
    {
        ensure_jolt_initialized();

        auto ctx            = make_own<WorldContext>();
        ctx->desc           = desc;
        ctx->temp_allocator = make_own<JPH::TempAllocatorImpl>(desc.temp_allocator_size_mb * 1024 * 1024);
        ctx->job_system     = make_own<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);

        ctx->physics_system = make_own<JPH::PhysicsSystem>();
        ctx->physics_system->Init(
            desc.max_bodies,
            0, // num body mutexes (0 = auto)
            desc.max_body_pairs,
            desc.max_contact_constraints,
            ctx->bp_layer_interface,
            ctx->object_vs_bp_filter,
            ctx->object_layer_pair_filter);
        ctx->physics_system->SetGravity(to_jph_vec3(desc.gravity));

        out_world                 = make_handle<PhysicsWorldHandle>();
        g_worlds[out_world.value] = std::move(ctx);
        return true;
    }

    static void delete_world(PhysicsWorldHandle world)
    {
        g_worlds.erase(world.value);
    }

    static void step_simulation(PhysicsWorldHandle world, float delta_time, uint collision_steps, JobScheduler* /*scheduler*/)
    {
        auto it = g_worlds.find(world.value);
        if (it == g_worlds.end()) return;

        auto& ctx   = *it->second;
        uint  steps = collision_steps > 0 ? collision_steps : 1;
        ctx.physics_system->Update(delta_time, steps, ctx.temp_allocator.get(), ctx.job_system.get());
    }

    static bool create_material(PhysicsWorldHandle /*world*/, const PhysicsMaterialDescriptor& /*desc*/, PhysicsMaterialHandle& out_material)
    {
        out_material = make_handle<PhysicsMaterialHandle>();
        return true;
    }

    static void release_material(PhysicsWorldHandle /*world*/, PhysicsMaterialHandle /*material*/)
    {
    }

    static bool create_box_shape(const Vector3& half_extents, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        JPH::BoxShapeSettings settings(to_jph_vec3(half_extents));
        auto                  result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt BoxShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_sphere_shape(float radius, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        JPH::SphereShapeSettings settings(std::max(radius, 0.001f));
        auto                     result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt SphereShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_capsule_shape(float half_height, float radius, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        JPH::CapsuleShapeSettings settings(std::max(half_height, 0.001f), std::max(radius, 0.001f));
        auto                      result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt CapsuleShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_cylinder_shape(float half_height, float radius, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        JPH::CylinderShapeSettings settings(std::max(half_height, 0.001f), std::max(radius, 0.001f));
        auto                       result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt CylinderShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_convex_hull_shape(Span<Vector3> points, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        if (points.size() < 4) return false;

        Vector<JPH::Vec3> jph_points;
        jph_points.reserve(points.size());
        for (usize i = 0; i < points.size(); ++i) {
            jph_points.push_back(to_jph_vec3(points[i]));
        }

        JPH::ConvexHullShapeSettings settings(jph_points.data(), static_cast<int>(jph_points.size()));
        auto                         result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt ConvexHullShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_triangle_mesh_shape(Span<Vector3> vertices, Span<uint> indices, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        if (vertices.empty() || indices.empty()) return false;

        JPH::VertexList jph_verts;
        jph_verts.reserve(vertices.size());
        for (usize i = 0; i < vertices.size(); ++i) {
            jph_verts.push_back(JPH::Float3(vertices[i].x, vertices[i].y, vertices[i].z));
        }

        JPH::IndexedTriangleList jph_tris;
        jph_tris.reserve(indices.size() / 3);
        for (usize i = 0; i + 2 < indices.size(); i += 3) {
            jph_tris.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2]));
        }

        JPH::MeshShapeSettings settings(jph_verts, jph_tris);
        auto                   result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt MeshShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static bool create_compound_shape(Span<PhysicsShapeHandle> child_shapes, Span<Vector3> positions, Span<Quaternion> rotations, PhysicsShapeHandle& out_shape)
    {
        ensure_jolt_initialized();
        if (child_shapes.empty()) return false;

        JPH::StaticCompoundShapeSettings settings;
        for (usize i = 0; i < child_shapes.size(); ++i) {
            auto it = g_shapes.find(child_shapes[i].value);
            if (it == g_shapes.end()) continue;
            Vector3    pos = (i < positions.size()) ? positions[i] : Vector3(0.0f);
            Quaternion rot = (i < rotations.size()) ? rotations[i] : glm::identity<Quaternion>();
            settings.AddShape(to_jph_vec3(pos), to_jph_quat(rot), it->second);
        }

        auto result = settings.Create();
        if (result.HasError()) {
            get_logger()->error("Failed to create Jolt CompoundShape: {}", result.GetError().c_str());
            return false;
        }
        out_shape                 = make_handle<PhysicsShapeHandle>();
        g_shapes[out_shape.value] = result.Get();
        return true;
    }

    static void release_shape(PhysicsShapeHandle shape)
    {
        g_shapes.erase(shape.value);
    }

    static auto to_jph_motion_type(MotionType type) -> JPH::EMotionType
    {
        switch (type) {
            case MotionType::Static:
                return JPH::EMotionType::Static;
            case MotionType::Kinematic:
                return JPH::EMotionType::Kinematic;
            case MotionType::Dynamic:
                return JPH::EMotionType::Dynamic;
        }
        return JPH::EMotionType::Dynamic;
    }

    static auto to_lyra_motion_type(JPH::EMotionType type) -> MotionType
    {
        switch (type) {
            case JPH::EMotionType::Static:
                return MotionType::Static;
            case JPH::EMotionType::Kinematic:
                return MotionType::Kinematic;
            case JPH::EMotionType::Dynamic:
                return MotionType::Dynamic;
        }
        return MotionType::Dynamic;
    }

    static bool create_body(PhysicsWorldHandle world, const RigidBodyDescriptor& desc, PhysicsBodyHandle& out_body)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return false;

        auto sit = g_shapes.find(desc.shape.value);
        if (sit == g_shapes.end()) {
            get_logger()->error("create_body: invalid shape handle");
            return false;
        }

        auto& ctx            = *wit->second;
        auto& body_interface = ctx.physics_system->GetBodyInterface();

        JPH::BodyCreationSettings settings(
            sit->second,
            to_jph_rvec3(desc.position),
            to_jph_quat(desc.rotation),
            to_jph_motion_type(desc.motion_type),
            static_cast<JPH::ObjectLayer>(desc.collision_layer));

        settings.mLinearDamping  = desc.linear_damping;
        settings.mAngularDamping = desc.angular_damping;
        settings.mGravityFactor  = desc.gravity_factor;
        settings.mFriction       = desc.material.valid() ? 0.5f : 0.5f;
        settings.mRestitution    = desc.material.valid() ? 0.0f : 0.0f;
        settings.mIsSensor       = desc.is_sensor;
        settings.mMotionQuality  = desc.enable_ccd ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;

        if (desc.motion_type == MotionType::Dynamic && desc.mass > 0.0f) {
            settings.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        JPH::Body* body = body_interface.CreateBody(settings);
        if (!body) {
            get_logger()->error("Jolt failed to allocate body");
            return false;
        }

        body_interface.AddBody(body->GetID(), desc.motion_type == MotionType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        if (desc.motion_type == MotionType::Dynamic) {
            body_interface.SetLinearVelocity(body->GetID(), to_jph_vec3(desc.linear_velocity));
            body_interface.SetAngularVelocity(body->GetID(), to_jph_vec3(desc.angular_velocity));
        }

        out_body                                                      = make_handle<PhysicsBodyHandle>();
        ctx.handle_to_body[out_body.value]                            = body->GetID();
        ctx.body_to_handle[body->GetID().GetIndexAndSequenceNumber()] = out_body.value;
        ctx.body_user_data[body->GetID().GetIndexAndSequenceNumber()] = desc.user_data;

        return true;
    }

    static void delete_body(PhysicsWorldHandle world, PhysicsBodyHandle body)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;

        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        auto& body_interface = ctx.physics_system->GetBodyInterface();
        body_interface.RemoveBody(bit->second);
        body_interface.DestroyBody(bit->second);

        ctx.body_to_handle.erase(bit->second.GetIndexAndSequenceNumber());
        ctx.body_user_data.erase(bit->second.GetIndexAndSequenceNumber());
        ctx.handle_to_body.erase(bit);
    }

    static void set_transform(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& pos, const Quaternion& rot)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().SetPositionAndRotation(
            bit->second,
            to_jph_rvec3(pos),
            to_jph_quat(rot),
            JPH::EActivation::DontActivate);
    }

    static void get_transform(PhysicsWorldHandle world, PhysicsBodyHandle body, Vector3& out_pos, Quaternion& out_rot)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        JPH::RVec3 pos;
        JPH::Quat  rot;
        ctx.physics_system->GetBodyInterface().GetPositionAndRotation(bit->second, pos, rot);
        out_pos = to_lyra_vec3(pos);
        out_rot = to_lyra_quat(rot);
    }

    static void set_motion_type(PhysicsWorldHandle world, PhysicsBodyHandle body, MotionType motion_type)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().SetMotionType(
            bit->second,
            to_jph_motion_type(motion_type),
            JPH::EActivation::Activate);
    }

    static auto get_motion_type(PhysicsWorldHandle world, PhysicsBodyHandle body) -> MotionType
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return MotionType::Static;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return MotionType::Static;

        return to_lyra_motion_type(ctx.physics_system->GetBodyInterface().GetMotionType(bit->second));
    }

    static void add_force(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& force)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().AddForce(bit->second, to_jph_vec3(force));
    }

    static void add_impulse(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& impulse)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().AddImpulse(bit->second, to_jph_vec3(impulse));
    }

    static void set_linear_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& velocity)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().SetLinearVelocity(bit->second, to_jph_vec3(velocity));
    }

    static auto get_linear_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body) -> Vector3
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return Vector3(0.0f);
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return Vector3(0.0f);

        return to_lyra_vec3(ctx.physics_system->GetBodyInterface().GetLinearVelocity(bit->second));
    }

    static void set_angular_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& angular_velocity)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().SetAngularVelocity(bit->second, to_jph_vec3(angular_velocity));
    }

    static auto get_angular_velocity(PhysicsWorldHandle world, PhysicsBodyHandle body) -> Vector3
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return Vector3(0.0f);
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return Vector3(0.0f);

        return to_lyra_vec3(ctx.physics_system->GetBodyInterface().GetAngularVelocity(bit->second));
    }

    static bool is_active(PhysicsWorldHandle world, PhysicsBodyHandle body)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return false;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return false;

        return ctx.physics_system->GetBodyInterface().IsActive(bit->second);
    }

    static void activate_body(PhysicsWorldHandle world, PhysicsBodyHandle body)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;
        auto& ctx = *wit->second;
        auto  bit = ctx.handle_to_body.find(body.value);
        if (bit == ctx.handle_to_body.end()) return;

        ctx.physics_system->GetBodyInterface().ActivateBody(bit->second);
    }

    static bool create_joint(PhysicsWorldHandle /*world*/, const JointDescriptor& /*desc*/, PhysicsJointHandle& out_joint)
    {
        out_joint = make_handle<PhysicsJointHandle>();
        return true;
    }

    static void delete_joint(PhysicsWorldHandle /*world*/, PhysicsJointHandle /*joint*/)
    {
    }

    static bool raycast_closest(PhysicsWorldHandle world, const RaycastQuery& query, RaycastHit& out_hit)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return false;

        auto&              ctx = *wit->second;
        JPH::RRayCast      ray{to_jph_rvec3(query.origin), to_jph_vec3(query.direction * query.max_distance)};
        JPH::RayCastResult hit;

        bool has_hit = ctx.physics_system->GetNarrowPhaseQuery().CastRay(ray, hit);
        if (has_hit) {
            out_hit.has_hit  = true;
            out_hit.distance = hit.mFraction * query.max_distance;
            out_hit.position = query.origin + query.direction * out_hit.distance;

            JPH::BodyLockRead lock(ctx.physics_system->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded()) {
                const JPH::Body& b      = lock.GetBody();
                JPH::Vec3        normal = b.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, to_jph_rvec3(out_hit.position));
                out_hit.normal          = to_lyra_vec3(normal);
            }

            auto hid_it = ctx.body_to_handle.find(hit.mBodyID.GetIndexAndSequenceNumber());
            if (hid_it != ctx.body_to_handle.end()) {
                out_hit.body = PhysicsBodyHandle(hid_it->second);
            }
            auto uid_it = ctx.body_user_data.find(hit.mBodyID.GetIndexAndSequenceNumber());
            if (uid_it != ctx.body_user_data.end()) {
                out_hit.user_data = uid_it->second;
            }
        }
        return has_hit;
    }

    static void raycast_all(PhysicsWorldHandle world, const RaycastQuery& query, Vector<RaycastHit>& out_hits)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;

        auto&                                                ctx = *wit->second;
        JPH::RRayCast                                        ray{to_jph_rvec3(query.origin), to_jph_vec3(query.direction * query.max_distance)};
        JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

        ctx.physics_system->GetNarrowPhaseQuery().CastRay(ray, {}, collector);
        collector.Sort();

        for (const auto& hit : collector.mHits) {
            RaycastHit rh{};
            rh.has_hit  = true;
            rh.distance = hit.mFraction * query.max_distance;
            rh.position = query.origin + query.direction * rh.distance;

            auto hid_it = ctx.body_to_handle.find(hit.mBodyID.GetIndexAndSequenceNumber());
            if (hid_it != ctx.body_to_handle.end()) {
                rh.body = PhysicsBodyHandle(hid_it->second);
            }
            auto uid_it = ctx.body_user_data.find(hit.mBodyID.GetIndexAndSequenceNumber());
            if (uid_it != ctx.body_user_data.end()) {
                rh.user_data = uid_it->second;
            }
            out_hits.push_back(rh);
        }
    }

    static bool shape_sweep(PhysicsWorldHandle world, const ShapeSweepQuery& query, RaycastHit& out_hit)
    {
        RaycastQuery ray{query.start_position, query.direction, query.max_distance, query.layer_mask};
        return raycast_closest(world, ray, out_hit);
    }

    static void query_overlap_sphere(PhysicsWorldHandle world, const OverlapSphereQuery& query, Vector<PhysicsBodyHandle>& out_bodies)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;

        auto&                                                         ctx = *wit->second;
        JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
        ctx.physics_system->GetBroadPhaseQuery().CollideSphere(to_jph_rvec3(query.center), query.radius, collector);

        for (const auto& body_id : collector.mHits) {
            auto it = ctx.body_to_handle.find(body_id.GetIndexAndSequenceNumber());
            if (it != ctx.body_to_handle.end()) {
                out_bodies.push_back(PhysicsBodyHandle(it->second));
            }
        }
    }

    static void query_overlap_box(PhysicsWorldHandle world, const OverlapBoxQuery& query, Vector<PhysicsBodyHandle>& out_bodies)
    {
        OverlapSphereQuery sphere_query{query.center, glm::length(query.half_extents), query.layer_mask};
        query_overlap_sphere(world, sphere_query, out_bodies);
    }

    static bool create_character(PhysicsWorldHandle /*world*/, const CharacterDescriptor& /*desc*/, PhysicsCharacterHandle& out_character)
    {
        out_character = make_handle<PhysicsCharacterHandle>();
        return true;
    }

    static void delete_character(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/)
    {
    }

    static void update_character(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/, const Vector3& /*desired_velocity*/, float /*delta_time*/, const Vector3& /*gravity*/)
    {
    }

    static void get_character_state(PhysicsWorldHandle /*world*/, PhysicsCharacterHandle /*character*/, Vector3& out_position, Quaternion& out_rotation, bool& out_is_on_ground)
    {
        out_position     = Vector3(0.0f);
        out_rotation     = glm::identity<Quaternion>();
        out_is_on_ground = true;
    }

    static void fetch_contact_events(PhysicsWorldHandle /*world*/, Vector<ContactEvent>& /*out_events*/)
    {
    }

    static void fetch_trigger_events(PhysicsWorldHandle /*world*/, Vector<TriggerEvent>& /*out_events*/)
    {
    }

    static void extract_debug_lines(PhysicsWorldHandle world, Vector<Vector3>& out_lines, Vector<uint>& out_colors)
    {
        auto wit = g_worlds.find(world.value);
        if (wit == g_worlds.end()) return;

        auto& ctx            = *wit->second;
        auto& body_interface = ctx.physics_system->GetBodyInterface();

        for (const auto& [handle, body_id] : ctx.handle_to_body) {
            JPH::RVec3 pos = body_interface.GetPosition(body_id);
            Vector3    p   = to_lyra_vec3(pos);

            // small orientation axes
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
} // namespace api

namespace lyra::jolt
{
    auto get_api() -> PhysicsAPI
    {
        PhysicsAPI api{};
        api.get_api_name               = api::get_api_name;
        api.create_world               = api::create_world;
        api.delete_world               = api::delete_world;
        api.step_simulation            = api::step_simulation;
        api.create_material            = api::create_material;
        api.release_material           = api::release_material;
        api.create_box_shape           = api::create_box_shape;
        api.create_sphere_shape        = api::create_sphere_shape;
        api.create_capsule_shape       = api::create_capsule_shape;
        api.create_cylinder_shape      = api::create_cylinder_shape;
        api.create_convex_hull_shape   = api::create_convex_hull_shape;
        api.create_triangle_mesh_shape = api::create_triangle_mesh_shape;
        api.create_compound_shape      = api::create_compound_shape;
        api.release_shape              = api::release_shape;
        api.create_body                = api::create_body;
        api.delete_body                = api::delete_body;
        api.set_transform              = api::set_transform;
        api.get_transform              = api::get_transform;
        api.set_motion_type            = api::set_motion_type;
        api.get_motion_type            = api::get_motion_type;
        api.add_force                  = api::add_force;
        api.add_impulse                = api::add_impulse;
        api.set_linear_velocity        = api::set_linear_velocity;
        api.get_linear_velocity        = api::get_linear_velocity;
        api.set_angular_velocity       = api::set_angular_velocity;
        api.get_angular_velocity       = api::get_angular_velocity;
        api.is_active                  = api::is_active;
        api.activate_body              = api::activate_body;
        api.create_joint               = api::create_joint;
        api.delete_joint               = api::delete_joint;
        api.raycast_closest            = api::raycast_closest;
        api.raycast_all                = api::raycast_all;
        api.shape_sweep                = api::shape_sweep;
        api.query_overlap_sphere       = api::query_overlap_sphere;
        api.query_overlap_box          = api::query_overlap_box;
        api.create_character           = api::create_character;
        api.delete_character           = api::delete_character;
        api.update_character           = api::update_character;
        api.get_character_state        = api::get_character_state;
        api.fetch_contact_events       = api::fetch_contact_events;
        api.fetch_trigger_events       = api::fetch_trigger_events;
        api.extract_debug_lines        = api::extract_debug_lines;
        return api;
    }

    void prepare() {}
    void cleanup() {}
    auto create() -> PhysicsAPI
    {
        return get_api();
    }
} // namespace lyra::jolt
