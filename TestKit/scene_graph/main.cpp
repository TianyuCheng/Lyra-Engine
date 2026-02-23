#include <Lyra/Scene/SceneTree.h>
#include "helper.h"

TEST_CASE("scn::basic_scene_graph" * doctest::description("Basic Scene Graph"))
{
    lyra::World world;

    SUBCASE("entity creation")
    {
        auto entity = world.create();

        CHECK(world.registry.valid(entity));
        CHECK(world.registry.all_of<lyra::TransformLocal>(entity));
        CHECK(world.registry.all_of<lyra::TransformWorld>(entity));
    }

    SUBCASE("child management")
    {
        auto parent = world.create();
        auto child  = world.create();

        world.add_child(parent, child);

        CHECK_EQ(world.registry.get<lyra::Parent>(child).node.entity, parent);
        {
            auto& children = world.registry.get<lyra::Children>(parent).nodes;
            bool  found    = false;
            for (auto c : children) {
                if (c.entity == child) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }

        world.del_child(parent, child);

        CHECK(!world.registry.any_of<lyra::Parent>(child));
        {
            auto& children = world.registry.get<lyra::Children>(parent).nodes;
            bool  found    = false;
            for (auto c : children) {
                if (c.entity == child) {
                    found = true;
                    break;
                }
            }
            CHECK(!found);
        }
    }

    SUBCASE("transform manipulation")
    {
        auto  node  = world.create();
        auto& local = world.registry.get<lyra::TransformLocal>(node);

        CHECK_EQ(local.position, lyra::Vector3(0.0f));
        CHECK_EQ(local.scale, lyra::Vector3(1.0f));
        CHECK_EQ(local.rotation.w, 1.0f);
        CHECK_EQ(local.flags, lyra::TransformFlag::NONE);

        world.translate(node, {1.0f, 2.0f, 3.0f});
        CHECK_EQ(world.registry.get<lyra::TransformLocal>(node).position, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ((world.registry.get<lyra::TransformLocal>(node).flags & lyra::TransformFlag::LOCAL_DIRTY), lyra::TransformFlag::LOCAL_DIRTY);

        world.scale(node, {2.0f, 2.0f, 2.0f});
        CHECK_EQ(world.registry.get<lyra::TransformLocal>(node).scale, lyra::Vector3(2.0f, 2.0f, 2.0f));

        world.rotate(node, {0.0f, 1.0f, 0.0f}, 90.0f);
        auto& final_local = world.registry.get<lyra::TransformLocal>(node);
        auto  expected_q  = glm::angleAxis(glm::radians(90.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        CHECK_EQ(final_local.rotation.w, doctest::Approx(expected_q.w));
        CHECK_EQ(final_local.rotation.x, doctest::Approx(expected_q.x));
        CHECK_EQ(final_local.rotation.y, doctest::Approx(expected_q.y));
        CHECK_EQ(final_local.rotation.z, doctest::Approx(expected_q.z));
    }

    SUBCASE("hierarchy and transform propagation")
    {
        auto parent = world.create();
        auto child  = world.create();
        world.add_child(parent, child);

        lyra::SceneTree hierarchy(world);
        hierarchy.rebuild();

        CHECK_EQ(hierarchy.size(), 2);

        world.translate(parent, {10.0f, 0.0f, 0.0f});
        world.translate(child, {0.0f, 5.0f, 0.0f});
        hierarchy.update();

        auto& parent_world = world.registry.get<lyra::TransformWorld>(parent);
        auto& child_world  = world.registry.get<lyra::TransformWorld>(child);

        // check parent translation (10, 0, 0)
        CHECK_EQ(parent_world.xform[3][0], doctest::Approx(10.0f));
        CHECK_EQ(parent_world.xform[3][1], doctest::Approx(0.0f));
        CHECK_EQ(parent_world.xform[3][2], doctest::Approx(0.0f));

        // check child translation (10, 5, 0) - inherited from parent
        CHECK_EQ(child_world.xform[3][0], doctest::Approx(10.0f));
        CHECK_EQ(child_world.xform[3][1], doctest::Approx(5.0f));
        CHECK_EQ(child_world.xform[3][2], doctest::Approx(0.0f));
    }
}
