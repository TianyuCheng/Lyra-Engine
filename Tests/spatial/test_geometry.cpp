#include "helper.h"
#include <Lyra/Utilities/Geometry.h>
#include <Lyra/Utilities/Math.h>

TEST_CASE("spatial::geometry" * doctest::description("Geometry Shapes Unit Tests"))
{
    SUBCASE("Circle")
    {
        lyra::Circle c({10.0f, 20.0f}, 5.0f);
        CHECK_EQ(c.center, lyra::Vector2(10.0f, 20.0f));
        CHECK_EQ(c.radius, 5.0f);

        // contains
        CHECK(c.contains(lyra::Vector2(10.0f, 20.0f))); // center
        CHECK(c.contains(lyra::Vector2(15.0f, 20.0f))); // boundary (+x)
        CHECK(c.contains(lyra::Vector2(10.0f, 15.0f))); // boundary (-y)
        CHECK(c.contains(lyra::Vector2(13.0f, 24.0f))); // 3^2 + 4^2 = 25 == 5^2

        CHECK(!c.contains(lyra::Vector2(15.01f, 20.0f)));
        CHECK(!c.contains(lyra::Vector2(10.0f, 25.01f)));
        CHECK(!c.contains(lyra::Vector2(13.01f, 24.01f)));
        CHECK(!c.contains(lyra::Vector2(0.0f, 0.0f)));
    }

    SUBCASE("Sphere")
    {
        lyra::Sphere s({1.0f, 2.0f, 3.0f}, 3.0f);
        CHECK_EQ(s.center, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ(s.radius, 3.0f);

        CHECK(s.contains(lyra::Vector3(1.0f, 2.0f, 3.0f)));  // center
        CHECK(s.contains(lyra::Vector3(4.0f, 2.0f, 3.0f)));  // boundary (+x)
        CHECK(s.contains(lyra::Vector3(1.0f, -1.0f, 3.0f))); // boundary (-y)
        CHECK(s.contains(lyra::Vector3(1.0f, 2.0f, 6.0f)));  // boundary (+z)
        CHECK(s.contains(lyra::Vector3(2.0f, 4.0f, 5.0f)));  // (1^2 + 2^2 + 2^2) = 9 == 3^2

        CHECK(!s.contains(lyra::Vector3(4.01f, 2.0f, 3.0f)));
        CHECK(!s.contains(lyra::Vector3(1.0f, -1.01f, 3.0f)));
        CHECK(!s.contains(lyra::Vector3(2.01f, 4.0f, 5.0f)));
        CHECK(!s.contains(lyra::Vector3(0.0f, 0.0f, 0.0f)));
    }

    SUBCASE("Ray2D")
    {
        lyra::Ray2D r({1.0f, 2.0f}, {0.0f, 1.0f});
        CHECK_EQ(r.origin, lyra::Vector2(1.0f, 2.0f));
        CHECK_EQ(r.direction, lyra::Vector2(0.0f, 1.0f));

        CHECK_EQ(r.at(0.0f), lyra::Vector2(1.0f, 2.0f));
        CHECK_EQ(r.at(5.0f), lyra::Vector2(1.0f, 7.0f));
        CHECK_EQ(r.at(-2.0f), lyra::Vector2(1.0f, 0.0f));
    }

    SUBCASE("Ray3D")
    {
        lyra::Ray3D r({1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, -1.0f});
        CHECK_EQ(r.origin, lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ(r.direction, lyra::Vector3(0.0f, 0.0f, -1.0f));

        CHECK_EQ(r.at(0.0f), lyra::Vector3(1.0f, 2.0f, 3.0f));
        CHECK_EQ(r.at(10.0f), lyra::Vector3(1.0f, 2.0f, -7.0f));
        CHECK_EQ(r.at(-5.0f), lyra::Vector3(1.0f, 2.0f, 8.0f));
    }

    SUBCASE("Plane")
    {
        // plane at Y = 5 facing +Y: dot(n, p) + d = 0 -> (0, 1, 0) * (x, 5, z) - 5 = 0 -> d = -5
        lyra::Plane p({0.0f, 1.0f, 0.0f}, -5.0f);
        CHECK_EQ(p.signed_distance(lyra::Vector3(0.0f, 5.0f, 0.0f)), doctest::Approx(0.0f));
        CHECK_EQ(p.signed_distance(lyra::Vector3(10.0f, 7.0f, -2.0f)), doctest::Approx(2.0f)); // in front
        CHECK_EQ(p.signed_distance(lyra::Vector3(0.0f, 2.0f, 0.0f)), doctest::Approx(-3.0f));  // behind

        // normalization
        lyra::Plane unnormalized({0.0f, 2.0f, 0.0f}, -10.0f);
        unnormalized.normalize();
        CHECK_EQ(unnormalized.normal.y, doctest::Approx(1.0f));
        CHECK_EQ(unnormalized.distance, doctest::Approx(-5.0f));
    }

    SUBCASE("Frustum - Perspective")
    {
        // Create standard perspective projection with ZO depth range [0, 1]
        lyra::Matrix4x4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);
        // Camera at origin looking down -Z, up is +Y
        lyra::Matrix4x4 view = glm::lookAtRH(lyra::Vector3(0.0f, 0.0f, 0.0f), lyra::Vector3(0.0f, 0.0f, -1.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        lyra::Matrix4x4 vp   = proj * view;

        lyra::Frustum frustum = lyra::Frustum::from_view_projection(vp);

        // Point inside frustum
        CHECK(frustum.contains(lyra::Vector3(0.0f, 0.0f, -10.0f)));
        CHECK(frustum.contains(lyra::Vector3(2.0f, 2.0f, -10.0f)));

        // Behind camera (z > 0)
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 0.0f, 5.0f)));

        // Closer than near plane (0 < -z < 1)
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 0.0f, -0.5f)));

        // Farther than far plane (-z > 100)
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 0.0f, -105.0f)));

        // Outside horizontal FOV
        CHECK(!frustum.contains(lyra::Vector3(20.0f, 0.0f, -10.0f)));
    }

    SUBCASE("Frustum - Orthographic")
    {
        lyra::Matrix4x4 proj = glm::orthoRH_ZO(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 50.0f);
        lyra::Matrix4x4 view = glm::lookAtRH(lyra::Vector3(0.0f, 0.0f, 0.0f), lyra::Vector3(0.0f, 0.0f, -1.0f), lyra::Vector3(0.0f, 1.0f, 0.0f));
        lyra::Matrix4x4 vp   = proj * view;

        lyra::Frustum frustum = lyra::Frustum::from_view_projection(vp);

        CHECK(frustum.contains(lyra::Vector3(0.0f, 0.0f, -10.0f)));
        CHECK(frustum.contains(lyra::Vector3(8.0f, 8.0f, -20.0f)));

        // Outside ortho box
        CHECK(!frustum.contains(lyra::Vector3(15.0f, 0.0f, -10.0f)));
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 15.0f, -10.0f)));
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 0.0f, -0.5f)));
        CHECK(!frustum.contains(lyra::Vector3(0.0f, 0.0f, -60.0f)));
    }
}
