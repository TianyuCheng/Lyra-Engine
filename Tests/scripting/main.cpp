#include "helper.h"
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scripting/Scripting.h>

namespace
{
    struct [[lyra::component("Gameplay")]] TestMover
    {
        float speed = 10.0f;
    };

    struct [[lyra::component("Gameplay")]] TestHealth
    {
        int hp = 100;
    };
} // namespace

TEST_CASE("scr::command_queue" * doctest::description("ScriptCommandQueue deferred structural operations"))
{
    lyra::World              world;
    lyra::ScriptCommandQueue queue;

    auto node = world.create();
    CHECK_EQ(queue.size(), 0);

    // deferred add_component
    queue.add_component<TestMover>(node, 42.0f);
    CHECK_EQ(queue.size(), 1);
    CHECK(!world.registry.any_of<TestMover>(node));

    queue.flush(world);
    CHECK_EQ(queue.size(), 0);
    CHECK(world.registry.any_of<TestMover>(node));
    CHECK_EQ(world.registry.get<TestMover>(node).speed, 42.0f);

    // deferred remove_component
    queue.remove_component<TestMover>(node);
    queue.flush(world);
    CHECK(!world.registry.any_of<TestMover>(node));

    // deferred destroy
    queue.destroy(node);
    queue.flush(world);
    CHECK(!world.registry.valid(node));
}

TEST_CASE("scr::script_context" * doctest::description("ScriptContext facade and query iteration"))
{
    lyra::World              world;
    lyra::ScriptCommandQueue queue;
    lyra::MemoryArena        arena(16 * 1024);
    lyra::WindowInput        raw_input{};
    lyra::InputManager       input_mgr{};

    lyra::ScriptContext ctx(&world, &queue, &arena, &raw_input, &input_mgr, 0.016f, 1.25f);

    CHECK_EQ(ctx.dt(), 0.016f);
    CHECK_EQ(ctx.time(), 1.25f);
    CHECK_EQ(ctx.raw_input(), &raw_input);
    CHECK_EQ(ctx.input(), &input_mgr);

    // scratch allocation
    int* scratch_val = ctx.scratch().allocate<int>(123);
    CHECK_NE(scratch_val, nullptr);
    CHECK_EQ(*scratch_val, 123);

    auto e1 = world.create();
    auto e2 = world.create();

    world.add_component<TestMover>(e1, 5.0f);
    world.add_component<TestHealth>(e1, 50);

    world.add_component<TestMover>(e2, 15.0f);
    world.add_component<TestHealth>(e2, 75);

    SUBCASE("transform manipulation sets local dirty")
    {
        auto& xform = world.registry.get<lyra::TransformLocal>(e1);
        CHECK_EQ(xform.flags, lyra::TransformFlag::NONE);

        ctx.translate(xform, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ(xform.position, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK(xform.flags.contains(lyra::TransformFlag::LOCAL_DIRTY));

        ctx.rotate(xform, lyra::Vector3(0, 1, 0), 90.0f);
        CHECK(xform.flags.contains(lyra::TransformFlag::LOCAL_DIRTY));

        ctx.scale(xform, lyra::Vector3(2.0f));
        CHECK_EQ(xform.scale, lyra::Vector3(2.0f));
    }

    SUBCASE("batched each iteration")
    {
        int count = 0;
        ctx.each<TestMover, TestHealth>([&](TestMover& mover, TestHealth& health) {
            mover.speed += 1.0f;
            health.hp -= 10;
            count++;
        });

        CHECK_EQ(count, 2);
        CHECK_EQ(world.registry.get<TestMover>(e1).speed, 6.0f);
        CHECK_EQ(world.registry.get<TestHealth>(e1).hp, 40);
        CHECK_EQ(world.registry.get<TestMover>(e2).speed, 16.0f);
        CHECK_EQ(world.registry.get<TestHealth>(e2).hp, 65);
    }

    SUBCASE("disabled component exclusion")
    {
        // disable TestMover on e1
        world.add_component<lyra::Disabled<TestMover>>(e1);

        int count = 0;
        ctx.each<TestMover, TestHealth>([&](TestMover&, TestHealth&) {
            count++;
        });

        // only e2 should be iterated
        CHECK_EQ(count, 1);
    }

    SUBCASE("structured binding query loop")
    {
        int count = 0;
        for (auto [node, mover] : ctx.query<TestMover>()) {
            CHECK(world.registry.valid(node));
            count++;
        }
        CHECK_EQ(count, 2);
    }
}

TEST_CASE("scr::script_layer" * doctest::description("ScriptLayer API registration and execution"))
{
    lyra::ScriptLayer layer;

    static bool pre_ran  = false;
    static bool main_ran = false;
    static bool post_ran = false;

    pre_ran  = false;
    main_ran = false;
    post_ran = false;

    static lyra::ScriptDescriptor descs[] = {
        {
            .name  = "TestPreSystem",
            .group = "Gameplay/Test",
            .stage = lyra::AppEvent::UPDATE_PRE,
            .flags = lyra::ScriptFlag::RUN_IN_EDITOR,
        },
        {
            .name  = "TestMainSystem",
            .group = "Gameplay/Test",
            .stage = lyra::AppEvent::UPDATE,
            .flags = lyra::ScriptFlag::NONE,
        },
        {
            .name  = "TestPostSystem",
            .group = "Gameplay/Test",
            .stage = lyra::AppEvent::UPDATE_POST,
            .flags = lyra::ScriptFlag::NONE,
        }};

    lyra::ScriptAPI api = {
        .get_api_name = []() -> lyra::CString { return "TestScriptAPI"; },
        .get_scripts  = [](lyra::ScriptDescriptor* out) -> lyra::uint {
        if (out) {
            out[0] = descs[0];
            out[1] = descs[1];
            out[2] = descs[2];
        }
        return 3;
    },
        .run = [](lyra::ScriptID id, lyra::ScriptContext& ctx) {
        if (id == lyra::hash_script_name("TestPreSystem")) {
            pre_ran = true;
        } else if (id == lyra::hash_script_name("TestMainSystem")) {
            main_ran = true;
        } else if (id == lyra::hash_script_name("TestPostSystem")) {
            post_ran = true;
        }
    },
        .get_params = nullptr,
    };

    layer.register_api(api);
    CHECK_EQ(layer.get_scripts().size(), 3);

    lyra::World        world;
    lyra::AppContext   ctx;
    lyra::WindowInput  raw_input{};
    lyra::InputManager input_mgr{};
    ctx.toolboard.add(&world);
    ctx.toolboard.add(&raw_input);
    ctx.toolboard.add(&input_mgr);

    SUBCASE("edit mode only runs systems with RUN_IN_EDITOR")
    {
        layer.set_simulation_state(lyra::SimulationState::EDIT);

        layer.run_pre(ctx);
        layer.run_main(ctx);
        layer.run_post(ctx);

        CHECK(pre_ran);
        CHECK(!main_ran);
        CHECK(!post_ran);
    }

    SUBCASE("play mode runs all enabled systems")
    {
        layer.set_simulation_state(lyra::SimulationState::PLAY);

        layer.run_pre(ctx);
        layer.run_main(ctx);
        layer.run_post(ctx);

        CHECK(pre_ran);
        CHECK(main_ran);
        CHECK(post_ran);
    }

    SUBCASE("disabled script does not run")
    {
        layer.set_simulation_state(lyra::SimulationState::PLAY);
        layer.set_script_enabled(lyra::hash_script_name("TestMainSystem"), false);

        layer.run_pre(ctx);
        layer.run_main(ctx);
        layer.run_post(ctx);

        CHECK(pre_ran);
        CHECK(!main_ran);
        CHECK(post_ran);
    }
}

#include "SampleScripts.gen.h"

TEST_CASE("scr::generated_bindings" * doctest::description("Verified bindings generated by lyra-reflect"))
{
    lyra::World world;
    lyra::generated::preregister_component_pools(world);

    auto cam_node = world.create();
    world.add_component<OrbitCamera>(cam_node, lyra::Vector3(0.0f, 1.0f, 0.0f), 45.0f);

    lyra::ScriptLayer layer;
    layer.register_api(lyra::generated::create_script_api());

    CHECK_EQ(layer.get_scripts().size(), 1);
    CHECK_EQ(std::string(layer.get_scripts()[0].name), "orbit");

    lyra::AppContext   ctx;
    lyra::WindowInput  raw_input{};
    lyra::InputManager input_mgr{};
    ctx.toolboard.add(&world);
    ctx.toolboard.add(&raw_input);
    ctx.toolboard.add(&input_mgr);

    layer.set_simulation_state(lyra::SimulationState::PLAY);

    // Initial rotation should be identity
    auto& xform = world.registry.get<lyra::TransformLocal>(cam_node);
    CHECK_EQ(xform.rotation.w, 1.0f);
    CHECK_EQ(xform.flags, lyra::TransformFlag::NONE);

    // Run the main update stage
    layer.run_main(ctx);

    // Should have rotated and flagged LOCAL_DIRTY!
    CHECK(xform.flags.contains(lyra::TransformFlag::LOCAL_DIRTY));
}

#include <Lyra/Runtime/InputLayer.h>
#include <Lyra/Windowing/WSIState.h>

namespace
{
    struct MockInputLayout
    {
        lyra::InputState                                   states[2];
        lyra::uint                                         state_index = 0;
        float                                              delta_time  = 0.0f;
        std::chrono::time_point<std::chrono::steady_clock> elapsed_time;
    };
} // namespace

TEST_CASE("scr::input_manager" * doctest::description("InputManager actions and composite axes"))
{
    lyra::InputManager manager;

    SUBCASE("initial default states are inactive")
    {
        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_pressed(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_released(lyra::InputAction::MOVE_FORWARD));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::HORIZONTAL), 0.0f);
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE), lyra::Vector2(0.0f));
    }

    SUBCASE("evaluating keyboard and mouse actions with mock input")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        // Frame 1: press W
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::W)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);

        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(manager.is_action_pressed(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_released(lyra::InputAction::MOVE_FORWARD));

        // 2d move composite should be (0, 1)
        auto move_vec = manager.get_axis_2d(lyra::InputAxis2D::MOVE);
        CHECK_EQ(move_vec.x, doctest::Approx(0.0f));
        CHECK_EQ(move_vec.y, doctest::Approx(1.0f));

        // Frame 2: hold W, also press D
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::D)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);

        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_pressed(lyra::InputAction::MOVE_FORWARD)); // already held
        CHECK(manager.is_action_pressed(lyra::InputAction::MOVE_RIGHT));

        // diagonal movement should be normalized
        move_vec       = manager.get_axis_2d(lyra::InputAxis2D::MOVE);
        float expected = 1.0f / std::sqrt(2.0f);
        CHECK_EQ(move_vec.x, doctest::Approx(expected));
        CHECK_EQ(move_vec.y, doctest::Approx(expected));

        // Frame 3: release W
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::W)] = lyra::ButtonState::OFF;
        manager.update(raw_input, 0.016f);

        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(manager.is_action_released(lyra::InputAction::MOVE_FORWARD));
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_RIGHT));

        // Frame 4: release D, press S (backward)
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::D)] = lyra::ButtonState::OFF;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::S)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);

        move_vec = manager.get_axis_2d(lyra::InputAxis2D::MOVE);
        CHECK_EQ(move_vec.x, doctest::Approx(0.0f));
        CHECK_EQ(move_vec.y, doctest::Approx(-1.0f));
    }

    SUBCASE("mouse look delta and zoom scroll")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        mock.state_index                   = 0;
        mock.states[0].mouse.position.xpos = 25.0f;
        mock.states[0].mouse.position.ypos = 12.0f;
        mock.states[1].mouse.position.xpos = 10.0f;
        mock.states[1].mouse.position.ypos = 20.0f;
        mock.states[0].mouse.scroll.y      = 2.5f;

        manager.update(raw_input, 0.016f);

        auto look = manager.get_axis_2d(lyra::InputAxis2D::LOOK);
        CHECK_EQ(look.x, doctest::Approx(15.0f));
        CHECK_EQ(look.y, doctest::Approx(-8.0f));

        float zoom = manager.get_axis(lyra::InputAxis::ZOOM);
        CHECK_EQ(zoom, doctest::Approx(2.5f));
    }

    SUBCASE("ScriptContext facade queries InputManager")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        // verify E elevates up and SPACE/CTRL do not elevate
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::E)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);

        lyra::ScriptCommandQueue queue;
        lyra::MemoryArena        arena(16 * 1024);
        lyra::ScriptContext      ctx(nullptr, &queue, &arena, raw_input, &manager, 0.016f, 1.0f);
        CHECK_EQ(ctx.raw_input(), raw_input);
        CHECK_EQ(ctx.input(), &manager);
        CHECK(ctx.is_action_down(lyra::InputAction::MOVE_UP));
        CHECK_EQ(ctx.get_axis(lyra::InputAxis::ELEVATION), doctest::Approx(1.0f));

        // verify space and ctrl do not trigger camera elevation or move_up/move_down
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::E)]     = lyra::ButtonState::OFF;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::SPACE)] = lyra::ButtonState::ON;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::CTRL)]  = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(!ctx.is_action_down(lyra::InputAction::MOVE_UP));
        CHECK(!ctx.is_action_down(lyra::InputAction::MOVE_DOWN));
        CHECK_EQ(ctx.get_axis(lyra::InputAxis::ELEVATION), doctest::Approx(0.0f));
    }

    SUBCASE("custom actions and operations")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        manager.bind_custom_action(0, lyra::DeviceButton::key(lyra::KeyButton::G));
        manager.register_action_alias("SpecialAbility", lyra::custom_action(0));

        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::G)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);

        CHECK(manager.is_custom_action_down(0));
        CHECK(manager.is_action_down("SpecialAbility"));

        lyra::ScriptCommandQueue queue;
        lyra::MemoryArena        arena(16 * 1024);
        lyra::ScriptContext      ctx(nullptr, &queue, &arena, raw_input, &manager, 0.016f, 1.0f);
        CHECK(ctx.is_custom_action_down(0));
        CHECK(ctx.is_action_down("SpecialAbility"));
    }

    SUBCASE("key combinations and chord bindings for actions and axes")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        manager.clear_all_bindings();

        // 1. Action chord: Ctrl + S for CUSTOM_0 ("Save")
        manager.bind_action(lyra::custom_action(0), lyra::ButtonChord::key(lyra::KeyButton::S, lyra::ModifierKey::CTRL));

        // 2. Multi-modifier chord: Ctrl + Shift + Z for CUSTOM_1 ("Redo")
        manager.bind_action(lyra::custom_action(1), lyra::ButtonChord::key(lyra::KeyButton::Z, lyra::ModifierKey::CTRL | lyra::ModifierKey::SHIFT));

        // 3. Axis chord: Shift + W for boost forward (+1.0) on CUSTOM_0 1D axis
        lyra::Axis1DComposite boost_axis{};
        boost_axis.positive.push_back(lyra::ButtonChord::key(lyra::KeyButton::W, lyra::ModifierKey::SHIFT));
        boost_axis.negative.push_back(lyra::ButtonChord::key(lyra::KeyButton::S, lyra::ModifierKey::SHIFT));
        manager.bind_custom_axis_1d(0, boost_axis);

        // Frame 1: Only S is pressed (Ctrl is OFF) -> Save action should NOT trigger
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::S)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(!manager.is_custom_action_down(0));

        // Frame 2: Ctrl + S -> Save action should trigger!
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::CTRL)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_custom_action_down(0));
        CHECK(manager.is_custom_action_pressed(0));

        // Frame 3: Only Ctrl + Z (missing Shift) -> Redo should NOT trigger
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::S)] = lyra::ButtonState::OFF;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::Z)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(!manager.is_custom_action_down(1));

        // Frame 4: Ctrl + Shift + Z -> Redo triggers!
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::SHIFT)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_custom_action_down(1));

        // Frame 5: Shift + W -> Boost axis activates (+1.0)
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::CTRL)] = lyra::ButtonState::OFF;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::Z)]    = lyra::ButtonState::OFF;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::W)]    = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK_EQ(manager.get_custom_axis(0), doctest::Approx(1.0f));

        // Frame 6: Plain W (without Shift) -> Boost axis does NOT activate (0.0)
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::SHIFT)] = lyra::ButtonState::OFF;
        manager.update(raw_input, 0.016f);
        CHECK_EQ(manager.get_custom_axis(0), doctest::Approx(0.0f));
    }

    SUBCASE("clearing bindings and custom configuration")
    {
        MockInputLayout mock{};
        auto*           raw_input                                                   = reinterpret_cast<const lyra::WindowInput*>(&mock);
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::W)] = lyra::ButtonState::ON;

        // with default bindings, W triggers MOVE_FORWARD and MOVE axis (0, 1)
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE).y, doctest::Approx(1.0f));

        // clear all bindings
        manager.clear_all_bindings();
        manager.update(raw_input, 0.016f);
        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE), lyra::Vector2(0.0f));

        // rebind custom action: I for MOVE_FORWARD
        manager.bind_action(lyra::InputAction::MOVE_FORWARD, lyra::DeviceButton::key(lyra::KeyButton::I));
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::I)] = lyra::ButtonState::ON;
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));

        // test reset back to default
        manager.set_default_bindings();
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
    }

    SUBCASE("input gating with set_enabled")
    {
        MockInputLayout mock{};
        auto*           raw_input = reinterpret_cast<const lyra::WindowInput*>(&mock);

        mock.state_index                                                              = 0;
        mock.states[0].keyboard.status[static_cast<lyra::uint>(lyra::KeyButton::W)]   = lyra::ButtonState::ON;
        mock.states[0].mouse.status[static_cast<lyra::uint>(lyra::MouseButton::LEFT)] = lyra::ButtonState::ON;
        mock.states[0].mouse.position.xpos                                            = 25.0f;
        mock.states[0].mouse.position.ypos                                            = 12.0f;
        mock.states[1].mouse.position.xpos                                            = 10.0f;
        mock.states[1].mouse.position.ypos                                            = 20.0f;
        mock.states[0].mouse.scroll.y                                                 = 2.5f;

        CHECK(manager.is_enabled());

        // with input enabled: both keyboard and mouse evaluate simultaneously
        manager.update(raw_input, 0.016f);
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(manager.is_action_down(lyra::InputAction::ATTACK));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE).y, doctest::Approx(1.0f));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::LOOK).x, doctest::Approx(15.0f));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::ZOOM), doctest::Approx(2.5f));

        // disable input:
        manager.set_enabled(false);
        CHECK(!manager.is_enabled());
        // verify immediate zeroing on set_enabled(false)
        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_down(lyra::InputAction::ATTACK));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::LOOK), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::ZOOM), doctest::Approx(0.0f));

        // updating while disabled keeps everything zeroed
        manager.update(raw_input, 0.016f);
        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_down(lyra::InputAction::ATTACK));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::LOOK), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::ZOOM), doctest::Approx(0.0f));

        // re-enable input: both keyboard and mouse work together again
        manager.set_enabled(true);
        CHECK(manager.is_enabled());

        manager.update(raw_input, 0.016f);
        CHECK(manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(manager.is_action_down(lyra::InputAction::ATTACK));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE).y, doctest::Approx(1.0f));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::LOOK).x, doctest::Approx(15.0f));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::ZOOM), doctest::Approx(2.5f));

        // test raw_input == nullptr early return
        manager.update(nullptr, 0.016f);
        CHECK(!manager.is_action_down(lyra::InputAction::MOVE_FORWARD));
        CHECK(!manager.is_action_down(lyra::InputAction::ATTACK));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::MOVE), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis_2d(lyra::InputAxis2D::LOOK), lyra::Vector2(0.0f));
        CHECK_EQ(manager.get_axis(lyra::InputAxis::ZOOM), doctest::Approx(0.0f));
    }
}
