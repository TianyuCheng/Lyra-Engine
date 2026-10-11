#include "helper.h"
#include <Lyra/Scene/OctTree.h>
#include <Lyra/Utilities/Math.h>
#include <algorithm>

TEST_CASE("spatial::octree" * doctest::description("OctTree Spatial Partitioning Structure Unit Tests"))
{
    using TestTree = lyra::OctTree<lyra::uint>;

    SUBCASE("Insert, Contains, Bounds and Clear")
    {
        TestTree tree(lyra::Bounds3D({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}), 4, 4);
        CHECK(tree.empty());
        CHECK_EQ(tree.size(), 0);

        // Valid insert
        CHECK(tree.insert(1, {{-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f}}));
        CHECK(tree.contains(1));
        CHECK_EQ(tree.size(), 1);
        CHECK(!tree.empty());

        auto b = tree.get_bounds(1);
        REQUIRE(b.has_value());
        CHECK_EQ(b->min, lyra::Vector3(-10.0f, -10.0f, -10.0f));
        CHECK_EQ(b->max, lyra::Vector3(10.0f, 10.0f, 10.0f));

        // Duplicate insert rejected
        CHECK(!tree.insert(1, {{-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f}}));
        CHECK_EQ(tree.size(), 1);

        // Out of bounds insert rejected
        CHECK(!tree.insert(2, {{200.0f, 200.0f, 200.0f}, {300.0f, 300.0f, 300.0f}}));
        CHECK(!tree.contains(2));
        CHECK_EQ(tree.size(), 1);

        // Clear
        tree.clear();
        CHECK(tree.empty());
        CHECK_EQ(tree.size(), 0);
        CHECK(!tree.contains(1));
    }

    SUBCASE("Remove and Fast-path Update")
    {
        TestTree tree(lyra::Bounds3D({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}), 4, 2);

        tree.insert(10, {{5.0f, 5.0f, 5.0f}, {15.0f, 15.0f, 15.0f}});
        tree.insert(20, {{20.0f, 20.0f, 20.0f}, {30.0f, 30.0f, 30.0f}});
        tree.insert(30, {{-50.0f, -50.0f, -50.0f}, {-40.0f, -40.0f, -40.0f}});
        CHECK_EQ(tree.size(), 3);

        // Remove
        CHECK(tree.remove(20));
        CHECK(!tree.contains(20));
        CHECK_EQ(tree.size(), 2);
        CHECK(!tree.remove(20)); // Already removed
        CHECK(!tree.remove(999)); // Nonexistent

        // Update in-place
        CHECK(tree.update(10, {{6.0f, 6.0f, 6.0f}, {14.0f, 14.0f, 14.0f}}));
        auto b10 = tree.get_bounds(10);
        REQUIRE(b10.has_value());
        CHECK_EQ(b10->min, lyra::Vector3(6.0f, 6.0f, 6.0f));

        // Update moving across octants
        CHECK(tree.update(10, {{-30.0f, -30.0f, -30.0f}, {-20.0f, -20.0f, -20.0f}}));
        b10 = tree.get_bounds(10);
        REQUIRE(b10.has_value());
        CHECK_EQ(b10->min, lyra::Vector3(-30.0f, -30.0f, -30.0f));

        // Update nonexistent
        CHECK(!tree.update(999, {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}));
    }

    SUBCASE("Subdivision and Node Allocation")
    {
        // Max capacity 2 elements per node
        TestTree tree(lyra::Bounds3D({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}), 4, 2);

        CHECK_EQ(tree.node_count(), 1); // Only root

        // Insert elements cleanly in (+X, +Y, +Z) octant
        tree.insert(1, {{10.0f, 10.0f, 10.0f}, {20.0f, 20.0f, 20.0f}});
        tree.insert(2, {{30.0f, 30.0f, 30.0f}, {40.0f, 40.0f, 40.0f}});
        CHECK_EQ(tree.node_count(), 1);

        // 3rd element triggers subdivision
        tree.insert(3, {{50.0f, 50.0f, 50.0f}, {60.0f, 60.0f, 60.0f}});
        CHECK_GT(tree.node_count(), 1); // Root + 8 children = 9 nodes
    }

    SUBCASE("Box and Point Query")
    {
        TestTree tree(lyra::Bounds3D({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{-50.0f, -50.0f, -50.0f}, {-40.0f, -40.0f, -40.0f}});
        tree.insert(2, {{10.0f, 10.0f, 10.0f}, {20.0f, 20.0f, 20.0f}});
        tree.insert(3, {{15.0f, 15.0f, 15.0f}, {25.0f, 25.0f, 25.0f}});
        tree.insert(4, {{80.0f, 80.0f, 80.0f}, {90.0f, 90.0f, 90.0f}});

        auto results = tree.query(lyra::Bounds3D({0.0f, 0.0f, 0.0f}, {30.0f, 30.0f, 30.0f}));
        CHECK_EQ(results.size(), 2);
        CHECK(std::find(results.begin(), results.end(), 2) != results.end());
        CHECK(std::find(results.begin(), results.end(), 3) != results.end());

        // Point query
        auto pt_results = tree.query(lyra::Vector3(17.0f, 17.0f, 17.0f));
        CHECK_EQ(pt_results.size(), 2);

        pt_results = tree.query(lyra::Vector3(85.0f, 85.0f, 85.0f));
        CHECK_EQ(pt_results.size(), 1);
        CHECK_EQ(pt_results[0], 4);
    }

    SUBCASE("Sphere and Ray Query")
    {
        TestTree tree(lyra::Bounds3D({-100.0f, -100.0f, -100.0f}, {100.0f, 100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f}});
        tree.insert(2, {{50.0f, 50.0f, 50.0f}, {60.0f, 60.0f, 60.0f}});

        // Sphere
        auto sphere_res = tree.query(lyra::Sphere({12.0f, 5.0f, 5.0f}, 3.0f));
        CHECK_EQ(sphere_res.size(), 1);
        CHECK_EQ(sphere_res[0], 1);

        // Ray
        lyra::Ray3D ray({-20.0f, 5.0f, 5.0f}, {1.0f, 0.0f, 0.0f});
        auto        hits = tree.query_ray(ray);
        REQUIRE_EQ(hits.size(), 1);
        CHECK_EQ(hits[0].element, 1);
        CHECK_EQ(hits[0].distance, doctest::Approx(20.0f));
    }

    SUBCASE("Frustum Culling Query (Direct Frustum and Camera Component)")
    {
        TestTree tree(lyra::Bounds3D({-1000.0f, -1000.0f, -1000.0f}, {1000.0f, 1000.0f, 1000.0f}), 4, 2);

        // Place entities
        tree.insert(1, {{-1.0f, -1.0f, -10.0f}, {1.0f, 1.0f, -8.0f}}); // In front of camera
        tree.insert(2, {{-1.0f, -1.0f, -20.0f}, {1.0f, 1.0f, -18.0f}}); // Further in front
        tree.insert(3, {{-1.0f, -1.0f, 10.0f}, {1.0f, 1.0f, 12.0f}}); // Behind camera
        tree.insert(4, {{-100.0f, -1.0f, -10.0f}, {-90.0f, 1.0f, -8.0f}}); // Far to the left outside FOV

        lyra::Camera camera;
        camera.projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);

        lyra::TransformWorld xform;
        xform.xform = lyra::Matrix4x4(1.0f); // Camera at origin, looking down -Z

        // 1. Frustum query directly
        lyra::Frustum frustum = tree.extract_camera_frustum(camera, xform);
        auto          visible = tree.query(frustum);

        CHECK_EQ(visible.size(), 2);
        CHECK(std::find(visible.begin(), visible.end(), 1) != visible.end());
        CHECK(std::find(visible.begin(), visible.end(), 2) != visible.end());
        CHECK(std::find(visible.begin(), visible.end(), 3) == visible.end());
        CHECK(std::find(visible.begin(), visible.end(), 4) == visible.end());

        // 2. Query via (Camera, TransformWorld) overload
        auto camera_visible = tree.query(camera, xform);
        CHECK_EQ(camera_visible.size(), 2);
    }
}
