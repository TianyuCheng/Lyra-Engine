#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSTYPES_H
#define LYRA_ENGINE_PHYSICS_PHYSICSTYPES_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Physics/PhysicsEnums.h>

namespace lyra
{
    // forward declarations
    struct PhysicsWorldDescriptor;
    struct PhysicsMaterialDescriptor;
    struct RigidBodyDescriptor;
    struct CharacterDescriptor;
    struct RaycastQuery;
    struct RaycastHit;
    struct ShapeSweepQuery;
    struct OverlapSphereQuery;
    struct OverlapBoxQuery;
    struct ContactEvent;
    struct TriggerEvent;
    struct PhysicsAPI;

    // strongly-typed handle tags
    // clang-format off
    struct PhysicsWorldTag {};
    struct PhysicsBodyTag {};
    struct PhysicsShapeTag {};
    struct PhysicsMaterialTag {};
    struct PhysicsJointTag {};
    struct PhysicsCharacterTag {};
    // clang-format on

    /**
     * @brief Type-safe generation handle wrapper for physics resources.
     */
    template <typename Tag>
    struct PhysicsHandle
    {
        uint value = 0;

        constexpr PhysicsHandle() = default;
        constexpr explicit PhysicsHandle(uint val) : value(val) {}

        [[nodiscard]] constexpr bool valid() const { return value != 0; }

        constexpr void reset() { value = 0; }

        constexpr bool operator==(const PhysicsHandle& other) const = default;
        constexpr bool operator!=(const PhysicsHandle& other) const = default;
    };

    // opaque runtime handles
    using PhysicsWorldHandle     = PhysicsHandle<PhysicsWorldTag>;
    using PhysicsBodyHandle      = PhysicsHandle<PhysicsBodyTag>;
    using PhysicsShapeHandle     = PhysicsHandle<PhysicsShapeTag>;
    using PhysicsMaterialHandle  = PhysicsHandle<PhysicsMaterialTag>;
    using PhysicsJointHandle     = PhysicsHandle<PhysicsJointTag>;
    using PhysicsCharacterHandle = PhysicsHandle<PhysicsCharacterTag>;

    // forward declarations of wrappers
    struct PhysicsWorld;
    struct PhysicsBody;
    struct PhysicsShape;
    struct PhysicsMaterial;
    struct PhysicsCharacter;

    /**
     * @brief Physics material wrapper.
     */
    struct PhysicsMaterial
    {
        PhysicsMaterialHandle handle;
        PhysicsWorldHandle    world;

        FORCE_INLINE      operator PhysicsMaterialHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        void destroy();
    };

    /**
     * @brief Physics collision shape wrapper.
     */
    struct PhysicsShape
    {
        PhysicsShapeHandle handle;
        ShapeType          type = ShapeType::Box;

        FORCE_INLINE      operator PhysicsShapeHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        static auto create_box(const Vector3& half_extents) -> PhysicsShape;
        static auto create_sphere(float radius) -> PhysicsShape;
        static auto create_capsule(float half_height, float radius) -> PhysicsShape;
        static auto create_cylinder(float half_height, float radius) -> PhysicsShape;
        static auto create_convex_hull(Span<Vector3> points) -> PhysicsShape;
        static auto create_triangle_mesh(Span<Vector3> vertices, Span<uint> indices) -> PhysicsShape;
        static auto create_compound(Span<PhysicsShapeHandle> shapes, Span<Vector3> positions = {}, Span<Quaternion> rotations = {}) -> PhysicsShape;

        void destroy();
    };

    /**
     * @brief Physics rigid body wrapper with fluent manipulation methods.
     */
    struct PhysicsBody
    {
        PhysicsBodyHandle  handle;
        PhysicsWorldHandle world;

        FORCE_INLINE      operator PhysicsBodyHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        void set_transform(const Vector3& pos, const Quaternion& rot);
        void get_transform(Vector3& out_pos, Quaternion& out_rot) const;
        auto get_position() const -> Vector3;
        auto get_rotation() const -> Quaternion;

        void set_motion_type(MotionType motion);
        auto get_motion_type() const -> MotionType;

        void add_force(const Vector3& force);
        void add_impulse(const Vector3& impulse);
        void set_linear_velocity(const Vector3& velocity);
        auto get_linear_velocity() const -> Vector3;
        void set_angular_velocity(const Vector3& velocity);
        auto get_angular_velocity() const -> Vector3;

        bool is_active() const;
        void activate();
        void destroy();
    };

    /**
     * @brief Kinematic virtual character controller wrapper.
     */
    struct PhysicsCharacter
    {
        PhysicsCharacterHandle handle;
        PhysicsWorldHandle     world;

        FORCE_INLINE      operator PhysicsCharacterHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        void update(const Vector3& desired_velocity, float delta_time, const Vector3& gravity = Vector3(0.0f, -9.81f, 0.0f));
        void get_state(Vector3& out_position, Quaternion& out_rotation, bool& out_is_on_ground) const;
        auto get_position() const -> Vector3;
        bool is_on_ground() const;
        void destroy();
    };

    /**
     * @brief Simulation world wrapper encapsulating scene queries, lifecycle, and stepping.
     */
    struct PhysicsWorld
    {
        PhysicsWorldHandle handle;

        FORCE_INLINE      operator PhysicsWorldHandle() const { return handle; }
        FORCE_INLINE bool valid() const { return handle.valid(); }

        static auto create() -> PhysicsWorld;
        static auto create(const PhysicsWorldDescriptor& desc) -> PhysicsWorld;
        void        destroy();

        void step(float delta_time, uint collision_steps = 1);

        auto create_body(const RigidBodyDescriptor& desc) -> PhysicsBody;
        auto create_material(const PhysicsMaterialDescriptor& desc) -> PhysicsMaterial;
        auto create_character(const CharacterDescriptor& desc) -> PhysicsCharacter;

        auto raycast(const RaycastQuery& query) -> Optional<RaycastHit>;
        auto raycast_all(const RaycastQuery& query) -> Vector<RaycastHit>;
        auto sweep(const ShapeSweepQuery& query) -> Optional<RaycastHit>;
        auto overlap_sphere(const OverlapSphereQuery& query) -> Vector<PhysicsBody>;
        auto overlap_box(const OverlapBoxQuery& query) -> Vector<PhysicsBody>;

        auto fetch_contact_events() -> Vector<ContactEvent>;
        auto fetch_trigger_events() -> Vector<TriggerEvent>;
        void extract_debug_lines(Vector<Vector3>& out_lines, Vector<uint>& out_colors);
    };

    /**
     * @brief Global physics subsystem coordinator and plugin manager.
     */
    struct Physics
    {
        static bool init(PhysicsBackend backend = PhysicsBackend::Jolt);
        static bool init(StringView custom_backend);
        static void shutdown();

        static auto api() -> PhysicsAPI*;
        static auto get_active_backend() -> PhysicsBackend;
        static auto get_active_world() -> PhysicsWorld;
        static void set_active_world(PhysicsWorld world);
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSTYPES_H
