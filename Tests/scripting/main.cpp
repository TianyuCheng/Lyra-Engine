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
    lyra::World world;
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
    lyra::World world;
    lyra::ScriptCommandQueue queue;
    lyra::detail::MemoryArena arena(16 * 1024);

    lyra::ScriptContext ctx(&world, &queue, &arena, 0.016f, 1.25f);

    CHECK_EQ(ctx.dt(), 0.016f);
    CHECK_EQ(ctx.time(), 1.25f);

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

    static lyra::ScriptDesc descs[] = {
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
        }
    };

    lyra::ScriptAPI api = {
        .get_api_name = []() -> lyra::CString { return "TestScriptAPI"; },
        .get_scripts = [](lyra::ScriptDesc* out) -> lyra::uint {
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

    lyra::World world;
    lyra::AppContext ctx;
    ctx.toolboard.add(&world);

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
