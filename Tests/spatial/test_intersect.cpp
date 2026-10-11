#include "helper.h"
#include <Lyra/Utilities/Intersect.h>
#include <Lyra/Utilities/Math.h>

TEST_CASE("spatial::intersect" * doctest::description("Intersection & Containment Algorithms Unit Tests"))
{
    SUBCASE("Ray2D vs Bounds2D")
    {
        lyra::Bounds2D box({-5.0f, -5.0f}, {5.0f, 5.0f});

        // 1. Ray aiming directly through box center
        lyra::Ray2D r1({-10.0f, 0.0f}, {1.0f, 0.0f});
        float       t_min = 0.0f;
        float       t_max = 0.0f;
        CHECK(lyra::intersects(r1, box, t_min, t_max));
        CHECK_EQ(t_min, doctest::Approx(5.0f));
        CHECK_EQ(t_max, doctest::Approx(15.0f));

        float hit_t = 0.0f;
        CHECK(lyra::intersects(r1, box, hit_t));
        CHECK_EQ(hit_t, doctest::Approx(5.0f));
        CHECK(lyra::intersects(r1, box));

        // 2. Ray starting inside the box
        lyra::Ray2D r_inside({0.0f, 0.0f}, {1.0f, 0.0f});
        CHECK(lyra::intersects(r_inside, box, t_min, t_max));
        CHECK_EQ(t_min, doctest::Approx(-5.0f));
        CHECK_EQ(t_max, doctest::Approx(5.0f));
        CHECK(lyra::intersects(r_inside, box, hit_t));
        CHECK_EQ(hit_t, 0.0f); // inside ray clamps to 0.0f

        // 3. Ray pointing away from box
        lyra::Ray2D r_away({-10.0f, 0.0f}, {-1.0f, 0.0f});
        CHECK(!lyra::intersects(r_away, box, t_min, t_max));
        CHECK(!lyra::intersects(r_away, box, hit_t));
        CHECK(!lyra::intersects(r_away, box));

        // 4. Ray parallel to slab (outside)
        lyra::Ray2D r_par_outside({0.0f, 10.0f}, {1.0f, 0.0f});
        CHECK(!lyra::intersects(r_par_outside, box));

        // 5. Ray parallel to slab (inside)
        lyra::Ray2D r_par_inside({-10.0f, 2.0f}, {1.0f, 0.0f});
        CHECK(lyra::intersects(r_par_inside, box));

        // 6. Ray missing diagonally
        lyra::Ray2D r_miss({-10.0f, -10.0f}, {0.0f, 1.0f});
        CHECK(!lyra::intersects(r_miss, box));
    }

    SUBCASE("Ray3D vs Bounds3D")
    {
        lyra::Bounds3D box({-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f});

        // 1. Ray through center
        lyra::Ray3D r1({0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f});
        float       t_min = 0.0f;
        float       t_max = 0.0f;
        CHECK(lyra::intersects(r1, box, t_min, t_max));
        CHECK_EQ(t_min, doctest::Approx(8.0f));
        CHECK_EQ(t_max, doctest::Approx(12.0f));

        float hit_t = 0.0f;
        CHECK(lyra::intersects(r1, box, hit_t));
        CHECK_EQ(hit_t, doctest::Approx(8.0f));
        CHECK(lyra::intersects(r1, box));

        // 2. Ray inside box
        lyra::Ray3D r_inside({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        CHECK(lyra::intersects(r_inside, box, hit_t));
        CHECK_EQ(hit_t, 0.0f);

        // 3. Ray pointing backwards
        lyra::Ray3D r_back({0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f});
        CHECK(!lyra::intersects(r_back, box));

        // 4. Glancing / diagonal ray hit
        lyra::Ray3D r_diag({-5.0f, -5.0f, -5.0f}, glm::normalize(lyra::Vector3(1.0f, 1.0f, 1.0f)));
        CHECK(lyra::intersects(r_diag, box));

        // 5. Parallel to X and Y axes outside Z slab
        lyra::Ray3D r_par({10.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
        CHECK(!lyra::intersects(r_par, box));
    }

    SUBCASE("Circle vs Bounds2D Overlaps")
    {
        lyra::Bounds2D box({0.0f, 0.0f}, {10.0f, 10.0f});

        // 1. Circle inside box
        CHECK(lyra::overlaps(lyra::Circle({5.0f, 5.0f}, 2.0f), box));
        CHECK(lyra::overlaps(box, lyra::Circle({5.0f, 5.0f}, 2.0f)));

        // 2. Circle overlapping edge
        CHECK(lyra::overlaps(lyra::Circle({11.0f, 5.0f}, 2.0f), box));

        // 3. Circle overlapping corner
        CHECK(lyra::overlaps(lyra::Circle({12.0f, 12.0f}, 3.0f), box)); // dist to (10, 10) is sqrt(8) ~ 2.828 <= 3

        // 4. Circle outside corner
        CHECK(!lyra::overlaps(lyra::Circle({12.0f, 12.0f}, 2.0f), box)); // dist ~ 2.828 > 2

        // 5. Circle completely disjoint
        CHECK(!lyra::overlaps(lyra::Circle({50.0f, 50.0f}, 5.0f), box));
    }

    SUBCASE("Sphere vs Bounds3D Overlaps")
    {
        lyra::Bounds3D box({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});

        // 1. Sphere inside box
        CHECK(lyra::overlaps(lyra::Sphere({5.0f, 5.0f, 5.0f}, 2.0f), box));
        CHECK(lyra::overlaps(box, lyra::Sphere({5.0f, 5.0f, 5.0f}, 2.0f)));

        // 2. Sphere overlapping face
        CHECK(lyra::overlaps(lyra::Sphere({11.0f, 5.0f, 5.0f}, 2.0f), box));

        // 3. Sphere overlapping corner (dist = sqrt(3) ~ 1.732)
        CHECK(lyra::overlaps(lyra::Sphere({11.0f, 11.0f, 11.0f}, 2.0f), box));

        // 4. Sphere outside corner
        CHECK(!lyra::overlaps(lyra::Sphere({11.0f, 11.0f, 11.0f}, 1.5f), box));

        // 5. Disjoint
        CHECK(!lyra::overlaps(lyra::Sphere({100.0f, 0.0f, 0.0f}, 5.0f), box));
    }

    SUBCASE("Frustum vs Bounds3D Containment & Intersect")
    {
        lyra::Matrix4x4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);
        lyra::Matrix4x4 view = glm::lookAtRH(lyra::Vector3(0.0f, 0.0f, 0.0f), lyra::Vector3(0.0f, 0.0f, -1.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        lyra::Matrix4x4 vp   = proj * view;

        lyra::Frustum frustum = lyra::Frustum::from_view_projection(vp);

        // 1. Fully contained box inside frustum
        lyra::Bounds3D inside_box({-1.0f, -1.0f, -11.0f}, {1.0f, 1.0f, -9.0f});
        CHECK_EQ(lyra::test_containment(frustum, inside_box), lyra::ContainmentType::CONTAINS);
        CHECK(lyra::intersects(frustum, inside_box));

        // 2. Box straddling the near plane (-1.0f in Z)
        lyra::Bounds3D near_straddle({-1.0f, -1.0f, -2.0f}, {1.0f, 1.0f, 0.5f});
        CHECK_EQ(lyra::test_containment(frustum, near_straddle), lyra::ContainmentType::INTERSECTS);
        CHECK(lyra::intersects(frustum, near_straddle));

        // 3. Box straddling lateral boundary
        lyra::Bounds3D side_straddle({8.0f, -1.0f, -11.0f}, {12.0f, 1.0f, -9.0f});
        CHECK_EQ(lyra::test_containment(frustum, side_straddle), lyra::ContainmentType::INTERSECTS);
        CHECK(lyra::intersects(frustum, side_straddle));

        // 4. Box completely behind camera
        lyra::Bounds3D behind_box({-1.0f, -1.0f, 5.0f}, {1.0f, 1.0f, 10.0f});
        CHECK_EQ(lyra::test_containment(frustum, behind_box), lyra::ContainmentType::DISJOINT);
        CHECK(!lyra::intersects(frustum, behind_box));

        // 5. Box completely past far plane
        lyra::Bounds3D far_box({-1.0f, -1.0f, -150.0f}, {1.0f, 1.0f, -120.0f});
        CHECK_EQ(lyra::test_containment(frustum, far_box), lyra::ContainmentType::DISJOINT);
        CHECK(!lyra::intersects(frustum, far_box));

        // 6. Frustum vs Sphere
        lyra::Sphere inside_sphere({0.0f, 0.0f, -10.0f}, 2.0f);
        CHECK(lyra::intersects(frustum, inside_sphere));

        lyra::Sphere behind_sphere({0.0f, 0.0f, 10.0f}, 2.0f);
        CHECK(!lyra::intersects(frustum, behind_sphere));
    }
}
