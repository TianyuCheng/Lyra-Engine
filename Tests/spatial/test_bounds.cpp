#include "helper.h"
#include <Lyra/Utilities/Bounds.h>
#include <Lyra/Utilities/Math.h>

TEST_CASE("spatial::bounds2d" * doctest::description("Exhaustive Bounds2D / Rect Unit Tests"))
{
    SUBCASE("construction and dimensions")
    {
        lyra::Bounds2D default_bounds;
        CHECK_EQ(default_bounds.min, lyra::Vector2(0.0f, 0.0f));
        CHECK_EQ(default_bounds.max, lyra::Vector2(0.0f, 0.0f));
        CHECK_EQ(default_bounds.width(), 0.0f);
        CHECK_EQ(default_bounds.height(), 0.0f);

        lyra::Bounds2D b({-10.0f, -20.0f}, {30.0f, 40.0f});
        CHECK_EQ(b.min.x, -10.0f);
        CHECK_EQ(b.min.y, -20.0f);
        CHECK_EQ(b.max.x, 30.0f);
        CHECK_EQ(b.max.y, 40.0f);
        CHECK_EQ(b.width(), 40.0f);
        CHECK_EQ(b.height(), 60.0f);
        CHECK_EQ(b.size(), lyra::Vector2(40.0f, 60.0f));
        CHECK_EQ(b.center(), lyra::Vector2(10.0f, 10.0f));
        CHECK_EQ(b.extents(), lyra::Vector2(20.0f, 30.0f));

        lyra::Bounds2D float_b(-5.0f, -6.0f, 5.0f, 6.0f);
        CHECK_EQ(float_b.width(), 10.0f);
        CHECK_EQ(float_b.height(), 12.0f);
        CHECK_EQ(float_b.center(), lyra::Vector2(0.0f, 0.0f));
    }

    SUBCASE("contains point")
    {
        lyra::Bounds2D b({0.0f, 0.0f}, {10.0f, 20.0f});

        // interior
        CHECK(b.contains(lyra::Vector2(5.0f, 10.0f)));

        // boundary inclusivity
        CHECK(b.contains(lyra::Vector2(0.0f, 0.0f)));
        CHECK(b.contains(lyra::Vector2(10.0f, 20.0f)));
        CHECK(b.contains(lyra::Vector2(0.0f, 10.0f)));
        CHECK(b.contains(lyra::Vector2(10.0f, 10.0f)));
        CHECK(b.contains(lyra::Vector2(5.0f, 0.0f)));
        CHECK(b.contains(lyra::Vector2(5.0f, 20.0f)));

        // exterior points
        CHECK(!b.contains(lyra::Vector2(-0.01f, 10.0f)));
        CHECK(!b.contains(lyra::Vector2(10.01f, 10.0f)));
        CHECK(!b.contains(lyra::Vector2(5.0f, -0.01f)));
        CHECK(!b.contains(lyra::Vector2(5.0f, 20.01f)));
        CHECK(!b.contains(lyra::Vector2(100.0f, 100.0f)));
    }

    SUBCASE("contains bounds")
    {
        lyra::Bounds2D parent({0.0f, 0.0f}, {100.0f, 100.0f});
        lyra::Bounds2D inside({10.0f, 10.0f}, {50.0f, 50.0f});
        lyra::Bounds2D exact({0.0f, 0.0f}, {100.0f, 100.0f});
        lyra::Bounds2D straddle({-10.0f, 10.0f}, {50.0f, 50.0f});
        lyra::Bounds2D outside({110.0f, 110.0f}, {120.0f, 120.0f});

        CHECK(parent.contains(parent));
        CHECK(parent.contains(exact));
        CHECK(parent.contains(inside));
        CHECK(!parent.contains(straddle));
        CHECK(!parent.contains(outside));
    }

    SUBCASE("overlaps")
    {
        lyra::Bounds2D a({0.0f, 0.0f}, {10.0f, 10.0f});

        // overlapping interior
        CHECK(a.overlaps(lyra::Bounds2D({5.0f, 5.0f}, {15.0f, 15.0f})));
        CHECK(a.overlaps(lyra::Bounds2D({-5.0f, -5.0f}, {5.0f, 5.0f})));
        CHECK(a.overlaps(lyra::Bounds2D({2.0f, 2.0f}, {8.0f, 8.0f})));

        // touching edges
        CHECK(a.overlaps(lyra::Bounds2D({10.0f, 0.0f}, {20.0f, 10.0f})));
        CHECK(a.overlaps(lyra::Bounds2D({0.0f, 10.0f}, {10.0f, 20.0f})));

        // disjoint on x or y
        CHECK(!a.overlaps(lyra::Bounds2D({10.01f, 0.0f}, {20.0f, 10.0f})));
        CHECK(!a.overlaps(lyra::Bounds2D({0.0f, 10.01f}, {10.0f, 20.0f})));
        CHECK(!a.overlaps(lyra::Bounds2D({-20.0f, -20.0f}, {-0.01f, -0.01f})));
    }

    SUBCASE("expand and merged")
    {
        lyra::Bounds2D b({5.0f, 5.0f}, {10.0f, 10.0f});

        b.expand(lyra::Vector2(0.0f, 15.0f));
        CHECK_EQ(b.min, lyra::Vector2(0.0f, 5.0f));
        CHECK_EQ(b.max, lyra::Vector2(10.0f, 15.0f));

        b.expand(lyra::Bounds2D({-5.0f, 2.0f}, {20.0f, 25.0f}));
        CHECK_EQ(b.min, lyra::Vector2(-5.0f, 2.0f));
        CHECK_EQ(b.max, lyra::Vector2(20.0f, 25.0f));

        lyra::Bounds2D b1({0.0f, 0.0f}, {10.0f, 10.0f});
        lyra::Bounds2D b2({-5.0f, 20.0f}, {15.0f, 30.0f});
        auto           m1 = b1.merged(b2);
        auto           m2 = b2.merged(b1);
        CHECK_EQ(m1.min, lyra::Vector2(-5.0f, 0.0f));
        CHECK_EQ(m1.max, lyra::Vector2(15.0f, 30.0f));
        CHECK_EQ(m1.min, m2.min);
        CHECK_EQ(m1.max, m2.max);
    }
}

TEST_CASE("spatial::bounds3d" * doctest::description("Exhaustive Bounds3D / Box Unit Tests"))
{
    SUBCASE("construction and dimensions")
    {
        lyra::Bounds3D b({-1.0f, -2.0f, -3.0f}, {4.0f, 6.0f, 7.0f});
        CHECK_EQ(b.width(), 5.0f);
        CHECK_EQ(b.height(), 8.0f);
        CHECK_EQ(b.depth(), 10.0f);
        CHECK_EQ(b.size(), lyra::Vector3(5.0f, 8.0f, 10.0f));
        CHECK_EQ(b.center(), lyra::Vector3(1.5f, 2.0f, 2.0f));
        CHECK_EQ(b.extents(), lyra::Vector3(2.5f, 4.0f, 5.0f));
    }

    SUBCASE("contains point")
    {
        lyra::Bounds3D b({-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f});
        CHECK(b.contains(lyra::Vector3(0.0f)));
        CHECK(b.contains(lyra::Vector3(-5.0f, 5.0f, -5.0f)));
        CHECK(b.contains(lyra::Vector3(5.0f, -5.0f, 5.0f)));

        CHECK(!b.contains(lyra::Vector3(5.01f, 0.0f, 0.0f)));
        CHECK(!b.contains(lyra::Vector3(0.0f, -5.01f, 0.0f)));
        CHECK(!b.contains(lyra::Vector3(0.0f, 0.0f, 5.01f)));
    }

    SUBCASE("contains bounds")
    {
        lyra::Bounds3D parent({-10.0f, -10.0f, -10.0f}, {10.0f, 10.0f, 10.0f});
        CHECK(parent.contains(parent));
        CHECK(parent.contains(lyra::Bounds3D({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f})));
        CHECK(!parent.contains(lyra::Bounds3D({0.0f, 0.0f, 0.0f}, {10.5f, 1.0f, 1.0f})));
        CHECK(!parent.contains(lyra::Bounds3D({20.0f, 20.0f, 20.0f}, {30.0f, 30.0f, 30.0f})));
    }

    SUBCASE("overlaps")
    {
        lyra::Bounds3D a({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
        CHECK(a.overlaps(lyra::Bounds3D({5.0f, 5.0f, 5.0f}, {15.0f, 15.0f, 15.0f})));
        CHECK(a.overlaps(lyra::Bounds3D({10.0f, 0.0f, 0.0f}, {20.0f, 10.0f, 10.0f}))); // touching face

        CHECK(!a.overlaps(lyra::Bounds3D({10.01f, 0.0f, 0.0f}, {20.0f, 10.0f, 10.0f})));
        CHECK(!a.overlaps(lyra::Bounds3D({0.0f, 10.01f, 0.0f}, {10.0f, 20.0f, 10.0f})));
        CHECK(!a.overlaps(lyra::Bounds3D({0.0f, 0.0f, 10.01f}, {10.0f, 10.0f, 20.0f})));
    }

    SUBCASE("transformed by matrix")
    {
        lyra::Bounds3D unit_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});

        // 1. translation
        lyra::Matrix4x4 t_mat = glm::translate(lyra::Matrix4x4(1.0f), lyra::Vector3(10.0f, 20.0f, 30.0f));
        auto            t_box = unit_box.transformed(t_mat);
        CHECK_EQ(t_box.center().x, doctest::Approx(10.0f));
        CHECK_EQ(t_box.center().y, doctest::Approx(20.0f));
        CHECK_EQ(t_box.center().z, doctest::Approx(30.0f));
        CHECK_EQ(t_box.extents().x, doctest::Approx(1.0f));
        CHECK_EQ(t_box.extents().y, doctest::Approx(1.0f));
        CHECK_EQ(t_box.extents().z, doctest::Approx(1.0f));

        // 2. non-uniform scale
        lyra::Matrix4x4 s_mat = glm::scale(lyra::Matrix4x4(1.0f), lyra::Vector3(2.0f, 3.0f, 4.0f));
        auto            s_box = unit_box.transformed(s_mat);
        CHECK_EQ(s_box.extents().x, doctest::Approx(2.0f));
        CHECK_EQ(s_box.extents().y, doctest::Approx(3.0f));
        CHECK_EQ(s_box.extents().z, doctest::Approx(4.0f));

        // 3. 90 deg rotation around Y axis
        lyra::Bounds3D  rect_box({-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f});
        lyra::Matrix4x4 r_mat = glm::rotate(lyra::Matrix4x4(1.0f), glm::radians(90.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        auto            r_box = rect_box.transformed(r_mat);
        // rotated 90 deg around Y swaps X and Z extents
        CHECK_EQ(r_box.extents().x, doctest::Approx(3.0f));
        CHECK_EQ(r_box.extents().y, doctest::Approx(2.0f));
        CHECK_EQ(r_box.extents().z, doctest::Approx(1.0f));
    }
}
