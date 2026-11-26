#pragma once

#ifndef LYRA_ENGINE_PHYSICS_PHYSICSAPI_H
#define LYRA_ENGINE_PHYSICS_PHYSICSAPI_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/JobSystem/JobScheduler.h>
#include <Lyra/Physics/PhysicsTypes.h>
#include <Lyra/Physics/PhysicsDescs.h>
#include <Lyra/Physics/PhysicsQueries.h>
#include <Lyra/Physics/PhysicsEvents.h>

namespace lyra
{
    /**
     * @brief C-ABI compatible interface table for physics engines and swappable plugins.
     */
    struct PhysicsAPI
    {
        // Identification
        CString (*get_api_name)();

        // World Lifecycle & Simulation
        bool (*create_world)(const PhysicsWorldDescriptor& desc, PhysicsWorldHandle& out_world);
        void (*delete_world)(PhysicsWorldHandle world);
        void (*step_simulation)(PhysicsWorldHandle world, float delta_time, uint collision_steps, JobScheduler* scheduler);

        // Materials
        bool (*create_material)(PhysicsWorldHandle world, const PhysicsMaterialDescriptor& desc, PhysicsMaterialHandle& out_material);
        void (*release_material)(PhysicsWorldHandle world, PhysicsMaterialHandle material);

        // Shape Creation & Lifetime
        bool (*create_box_shape)(const Vector3& half_extents, PhysicsShapeHandle& out_shape);
        bool (*create_sphere_shape)(float radius, PhysicsShapeHandle& out_shape);
        bool (*create_capsule_shape)(float half_height, float radius, PhysicsShapeHandle& out_shape);
        bool (*create_cylinder_shape)(float half_height, float radius, PhysicsShapeHandle& out_shape);
        bool (*create_convex_hull_shape)(Span<Vector3> points, PhysicsShapeHandle& out_shape);
        bool (*create_triangle_mesh_shape)(Span<Vector3> vertices, Span<uint> indices, PhysicsShapeHandle& out_shape);
        bool (*create_compound_shape)(Span<PhysicsShapeHandle> child_shapes, Span<Vector3> positions, Span<Quaternion> rotations, PhysicsShapeHandle& out_shape);
        void (*release_shape)(PhysicsShapeHandle shape);

        // Rigid Bodies
        bool (*create_body)(PhysicsWorldHandle world, const RigidBodyDescriptor& desc, PhysicsBodyHandle& out_body);
        void (*delete_body)(PhysicsWorldHandle world, PhysicsBodyHandle body);
        void (*set_transform)(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& pos, const Quaternion& rot);
        void (*get_transform)(PhysicsWorldHandle world, PhysicsBodyHandle body, Vector3& out_pos, Quaternion& out_rot);
        void (*set_motion_type)(PhysicsWorldHandle world, PhysicsBodyHandle body, MotionType motion_type);
        auto (*get_motion_type)(PhysicsWorldHandle world, PhysicsBodyHandle body) -> MotionType;
        void (*add_force)(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& force);
        void (*add_impulse)(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& impulse);
        void (*set_linear_velocity)(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& velocity);
        auto (*get_linear_velocity)(PhysicsWorldHandle world, PhysicsBodyHandle body) -> Vector3;
        void (*set_angular_velocity)(PhysicsWorldHandle world, PhysicsBodyHandle body, const Vector3& angular_velocity);
        auto (*get_angular_velocity)(PhysicsWorldHandle world, PhysicsBodyHandle body) -> Vector3;
        bool (*is_active)(PhysicsWorldHandle world, PhysicsBodyHandle body);
        void (*activate_body)(PhysicsWorldHandle world, PhysicsBodyHandle body);

        // Constraints & Joints
        bool (*create_joint)(PhysicsWorldHandle world, const JointDescriptor& desc, PhysicsJointHandle& out_joint);
        void (*delete_joint)(PhysicsWorldHandle world, PhysicsJointHandle joint);

        // Scene Queries & Raycasting
        bool (*raycast_closest)(PhysicsWorldHandle world, const RaycastQuery& query, RaycastHit& out_hit);
        void (*raycast_all)(PhysicsWorldHandle world, const RaycastQuery& query, Vector<RaycastHit>& out_hits);
        bool (*shape_sweep)(PhysicsWorldHandle world, const ShapeSweepQuery& query, RaycastHit& out_hit);
        void (*query_overlap_sphere)(PhysicsWorldHandle world, const OverlapSphereQuery& query, Vector<PhysicsBodyHandle>& out_bodies);
        void (*query_overlap_box)(PhysicsWorldHandle world, const OverlapBoxQuery& query, Vector<PhysicsBodyHandle>& out_bodies);

        // Virtual Character Controller
        bool (*create_character)(PhysicsWorldHandle world, const CharacterDescriptor& desc, PhysicsCharacterHandle& out_character);
        void (*delete_character)(PhysicsWorldHandle world, PhysicsCharacterHandle character);
        void (*update_character)(PhysicsWorldHandle world, PhysicsCharacterHandle character, const Vector3& desired_velocity, float delta_time, const Vector3& gravity);
        void (*get_character_state)(PhysicsWorldHandle world, PhysicsCharacterHandle character, Vector3& out_position, Quaternion& out_rotation, bool& out_is_on_ground);

        // Event Extraction
        void (*fetch_contact_events)(PhysicsWorldHandle world, Vector<ContactEvent>& out_events);
        void (*fetch_trigger_events)(PhysicsWorldHandle world, Vector<TriggerEvent>& out_events);

        // Diagnostics & Debug Visualization
        void (*extract_debug_lines)(PhysicsWorldHandle world, Vector<Vector3>& out_lines, Vector<uint>& out_colors);
    };

} // namespace lyra

#endif // LYRA_ENGINE_PHYSICS_PHYSICSAPI_H
