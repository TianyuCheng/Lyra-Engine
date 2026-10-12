#include "helper.h"
#include <Lyra/Scene/QuadTree.h>
#include <Lyra/Utilities/Math.h>
#include <algorithm>

TEST_CASE("spatial::quadtree" * doctest::description("QuadTree Spatial Partitioning Structure Unit Tests"))
{
    using TestTree = lyra::QuadTree<lyra::uint>;

    SUBCASE("Insert, Contains, Bounds and Clear")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 4);
        CHECK(tree.empty());
        CHECK_EQ(tree.size(), 0);

        // Valid insert
        CHECK(tree.insert(1, {{-10.0f, -10.0f}, {10.0f, 10.0f}}));
        CHECK(tree.contains(1));
        CHECK_EQ(tree.size(), 1);
        CHECK(!tree.empty());

        auto b = tree.get_bounds(1);
        REQUIRE(b.has_value());
        CHECK_EQ(b->min, lyra::Vector2(-10.0f, -10.0f));
        CHECK_EQ(b->max, lyra::Vector2(10.0f, 10.0f));

        // Duplicate insert rejected
        CHECK(!tree.insert(1, {{-5.0f, -5.0f}, {5.0f, 5.0f}}));
        CHECK_EQ(tree.size(), 1);

        // Out of bounds insert rejected
        CHECK(!tree.insert(2, {{200.0f, 200.0f}, {300.0f, 300.0f}}));
        CHECK(!tree.contains(2));
        CHECK_EQ(tree.size(), 1);

        // Clear
        tree.clear();
        CHECK(tree.empty());
        CHECK_EQ(tree.size(), 0);
        CHECK(!tree.contains(1));
        CHECK(!tree.get_bounds(1).has_value());
    }

    SUBCASE("Remove and Fast-path Update")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        tree.insert(10, {{5.0f, 5.0f}, {15.0f, 15.0f}});
        tree.insert(20, {{20.0f, 20.0f}, {30.0f, 30.0f}});
        tree.insert(30, {{-50.0f, -50.0f}, {-40.0f, -40.0f}});
        CHECK_EQ(tree.size(), 3);

        // Remove
        CHECK(tree.remove(20));
        CHECK(!tree.contains(20));
        CHECK_EQ(tree.size(), 2);
        CHECK(!tree.remove(20));  // Already removed
        CHECK(!tree.remove(999)); // Nonexistent

        // Update in-place
        CHECK(tree.update(10, {{6.0f, 6.0f}, {14.0f, 14.0f}}));
        auto b10 = tree.get_bounds(10);
        REQUIRE(b10.has_value());
        CHECK_EQ(b10->min, lyra::Vector2(6.0f, 6.0f));
        CHECK_EQ(b10->max, lyra::Vector2(14.0f, 14.0f));

        // Update moving across quadrants
        CHECK(tree.update(10, {{-30.0f, -30.0f}, {-20.0f, -20.0f}}));
        b10 = tree.get_bounds(10);
        REQUIRE(b10.has_value());
        CHECK_EQ(b10->min, lyra::Vector2(-30.0f, -30.0f));

        // Update nonexistent
        CHECK(!tree.update(999, {{0.0f, 0.0f}, {1.0f, 1.0f}}));
    }

    SUBCASE("Subdivision and Node Allocation")
    {
        // Max capacity 2 elements per node
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        CHECK_EQ(tree.node_count(), 1); // Only root

        // Insert elements cleanly in top-right quadrant (+X, +Y)
        tree.insert(1, {{10.0f, 10.0f}, {20.0f, 20.0f}});
        tree.insert(2, {{30.0f, 30.0f}, {40.0f, 40.0f}});
        CHECK_EQ(tree.node_count(), 1);

        // 3rd element triggers subdivision
        tree.insert(3, {{50.0f, 50.0f}, {60.0f, 60.0f}});
        CHECK_GT(tree.node_count(), 1); // Root + 4 children = 5 nodes
    }

    SUBCASE("Rect Query")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{-50.0f, -50.0f}, {-40.0f, -40.0f}});
        tree.insert(2, {{10.0f, 10.0f}, {20.0f, 20.0f}});
        tree.insert(3, {{15.0f, 15.0f}, {25.0f, 25.0f}});
        tree.insert(4, {{80.0f, 80.0f}, {90.0f, 90.0f}});

        auto results = tree.query(lyra::Bounds2D({0.0f, 0.0f}, {30.0f, 30.0f}));
        CHECK_EQ(results.size(), 2);
        CHECK(std::find(results.begin(), results.end(), 2) != results.end());
        CHECK(std::find(results.begin(), results.end(), 3) != results.end());
        CHECK(std::find(results.begin(), results.end(), 1) == results.end());
        CHECK(std::find(results.begin(), results.end(), 4) == results.end());

        // Early exit via callback
        lyra::uint visit_count = 0;
        tree.query(lyra::Bounds2D({0.0f, 0.0f}, {30.0f, 30.0f}), [&](const lyra::uint&, const lyra::Bounds2D&) {
            ++visit_count;
            return false; // Stop immediately
        });
        CHECK_EQ(visit_count, 1);
    }

    SUBCASE("Point Query")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{0.0f, 0.0f}, {10.0f, 10.0f}});
        tree.insert(2, {{5.0f, 5.0f}, {15.0f, 15.0f}});
        tree.insert(3, {{20.0f, 20.0f}, {30.0f, 30.0f}});

        // Point inside intersection of 1 and 2
        auto results = tree.query(lyra::Vector2(7.0f, 7.0f));
        CHECK_EQ(results.size(), 2);
        CHECK(std::find(results.begin(), results.end(), 1) != results.end());
        CHECK(std::find(results.begin(), results.end(), 2) != results.end());

        // Point inside 3 only
        results = tree.query(lyra::Vector2(25.0f, 25.0f));
        CHECK_EQ(results.size(), 1);
        CHECK_EQ(results[0], 3);

        // Point in empty space
        results = tree.query(lyra::Vector2(-50.0f, -50.0f));
        CHECK(results.empty());
    }

    SUBCASE("Circle Query")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{0.0f, 0.0f}, {10.0f, 10.0f}});
        tree.insert(2, {{50.0f, 50.0f}, {60.0f, 60.0f}});

        auto results = tree.query(lyra::Circle({12.0f, 5.0f}, 3.0f));
        CHECK_EQ(results.size(), 1);
        CHECK_EQ(results[0], 1);

        results = tree.query(lyra::Circle({-50.0f, -50.0f}, 5.0f));
        CHECK(results.empty());
    }

    SUBCASE("Ray Query with Distance Sorting")
    {
        TestTree tree(lyra::Bounds2D({-100.0f, -100.0f}, {100.0f, 100.0f}), 4, 2);

        tree.insert(1, {{40.0f, -5.0f}, {50.0f, 5.0f}});   // Further along +X
        tree.insert(2, {{10.0f, -5.0f}, {20.0f, 5.0f}});   // Closer along +X
        tree.insert(3, {{-50.0f, -5.0f}, {-40.0f, 5.0f}}); // Behind along -X

        lyra::Ray2D ray({0.0f, 0.0f}, {1.0f, 0.0f});
        auto        hits = tree.query_ray(ray);

        REQUIRE_EQ(hits.size(), 2);
        // Sorted by distance
        CHECK_EQ(hits[0].element, 2);
        CHECK_EQ(hits[0].distance, doctest::Approx(10.0f));
        CHECK_EQ(hits[1].element, 1);
        CHECK_EQ(hits[1].distance, doctest::Approx(40.0f));

        // Max distance clamping
        auto hits_clamped = tree.query_ray(ray, 25.0f);
        REQUIRE_EQ(hits_clamped.size(), 1);
        CHECK_EQ(hits_clamped[0].element, 2);
    }

    SUBCASE("Rebuild and Grow to Fit")
    {
        TestTree tree(lyra::Bounds2D({-10.0f, -10.0f}, {10.0f, 10.0f}), 4, 2);
        tree.insert(1, {{-5.0f, -5.0f}, {5.0f, 5.0f}});

        // Element outside current bounds
        CHECK(!tree.insert(2, {{50.0f, 50.0f}, {60.0f, 60.0f}}));

        // Grow to fit
        tree.grow_to_fit(lyra::Bounds2D({50.0f, 50.0f}, {60.0f, 60.0f}));
        CHECK(tree.insert(2, {{50.0f, 50.0f}, {60.0f, 60.0f}}));
        CHECK_EQ(tree.size(), 2);
        CHECK(tree.contains(1));
        CHECK(tree.contains(2));
    }
}
