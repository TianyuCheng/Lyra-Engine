#include <doctest/doctest.h>
#include <Lyra/Physics/PhysicsAPI.h>
#include <Lyra/Physics/PhysicsDescs.h>
#include <Lyra/Physics/PhysicsEnums.h>
#include <Lyra/Physics/PhysicsTypes.h>

using namespace lyra;

TEST_CASE("Physics Subsystem - Mock Backend Lifecycle and Simulation")
{
    // 1. Subsystem initialization
    auto init_ok = Physics::init(PhysicsBackend::Mock);
    REQUIRE(init_ok.has_value());
    CHECK(Physics::api() != nullptr);
    CHECK(Physics::get_active_backend() == PhysicsBackend::Mock);

    // 2. World creation
    auto world = PhysicsWorld::create();
    CHECK(world.valid());

    Physics::set_active_world(world);
    CHECK(Physics::get_active_world().valid());

    // 3. Material creation
    PhysicsMaterialDescriptor mat_desc{};
    mat_desc.static_friction  = 0.5f;
    mat_desc.dynamic_friction = 0.5f;
    mat_desc.restitution      = 0.2f;
    auto mat                  = world.create_material(mat_desc);
    CHECK(mat.valid());

    // 4. Collision shape creation
    auto box_shape = PhysicsShape::create_box(Vector3(1.0f, 1.0f, 1.0f));
    CHECK(box_shape.valid());

    // 5. Rigid body creation
    RigidBodyDescriptor body_desc{};
    body_desc.shape       = box_shape.handle;
    body_desc.material    = mat.handle;
    body_desc.motion_type = MotionType::Dynamic;
    body_desc.position    = Vector3(0.0f, 10.0f, 0.0f);

    auto body = world.create_body(body_desc);
    CHECK(body.valid());

    body.set_linear_velocity(Vector3(0.0f, -5.0f, 0.0f));
    body.add_force(Vector3(0.0f, -9.8f, 0.0f));

    // 6. Character creation
    CharacterDescriptor char_desc{};
    char_desc.position = Vector3(2.0f, 0.0f, 2.0f);

    auto character = world.create_character(char_desc);
    CHECK(character.valid());

    character.update(Vector3(1.0f, 0.0f, 0.0f), 0.016f);

    // 7. Simulation step
    world.step(0.016f);

    // 8. Cleanup resources
    character.destroy();
    CHECK(!character.valid());

    body.destroy();
    CHECK(!body.valid());

    box_shape.destroy();
    CHECK(!box_shape.valid());

    mat.destroy();
    CHECK(!mat.valid());

    world.destroy();
    CHECK(!world.valid());

    Physics::shutdown();
    CHECK(Physics::api() == nullptr);
}
