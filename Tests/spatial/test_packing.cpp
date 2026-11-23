#include "helper.h"
#include <Lyra/Utilities/Packing.h>

TEST_CASE("spatial::packing" * doctest::description("Octahedral Normal Encoding & Data Packing Unit Tests"))
{
    SUBCASE("Octahedral Encoding - Cardinal Axes")
    {
        const lyra::Vector3 axes[] = {
            {1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
        };

        for (const auto& n : axes) {
            lyra::Vector2 encoded = lyra::octahedral_encode(n);
            CHECK(encoded.x >= -1.0f);
            CHECK(encoded.x <= 1.0f);
            CHECK(encoded.y >= -1.0f);
            CHECK(encoded.y <= 1.0f);

            lyra::Vector3 decoded = lyra::octahedral_decode(encoded);
            CHECK(glm::dot(decoded, n) > 0.9999f);
            CHECK(glm::length(decoded - n) < 1e-4f);

            // test alias
            CHECK_EQ(lyra::oct_encode(n), encoded);
            CHECK_EQ(lyra::oct_decode(encoded), decoded);
        }
    }

    SUBCASE("Octahedral Encoding - Diagonal Vectors")
    {
        const lyra::Vector3 diagonals[] = {
            glm::normalize(lyra::Vector3(1.0f, 1.0f, 1.0f)),
            glm::normalize(lyra::Vector3(-1.0f, 1.0f, 1.0f)),
            glm::normalize(lyra::Vector3(1.0f, -1.0f, 1.0f)),
            glm::normalize(lyra::Vector3(-1.0f, -1.0f, 1.0f)),
            glm::normalize(lyra::Vector3(1.0f, 1.0f, -1.0f)),
            glm::normalize(lyra::Vector3(-1.0f, 1.0f, -1.0f)),
            glm::normalize(lyra::Vector3(1.0f, -1.0f, -1.0f)),
            glm::normalize(lyra::Vector3(-1.0f, -1.0f, -1.0f)),
        };

        for (const auto& n : diagonals) {
            lyra::Vector2 encoded = lyra::octahedral_encode(n);
            CHECK(encoded.x >= -1.0f);
            CHECK(encoded.x <= 1.0f);
            CHECK(encoded.y >= -1.0f);
            CHECK(encoded.y <= 1.0f);

            lyra::Vector3 decoded = lyra::octahedral_decode(encoded);
            CHECK(glm::dot(decoded, n) > 0.9999f);
            CHECK(glm::length(decoded - n) < 1e-4f);
        }
    }

    SUBCASE("Octahedral Encoding - Dense Spherical Sweep")
    {
        constexpr int theta_steps = 36;
        constexpr int phi_steps   = 72;

        for (int i = 0; i <= theta_steps; ++i) {
            float theta = glm::pi<float>() * float(i) / float(theta_steps);
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            for (int j = 0; j < phi_steps; ++j) {
                float phi = 2.0f * glm::pi<float>() * float(j) / float(phi_steps);
                float sin_phi = std::sin(phi);
                float cos_phi = std::cos(phi);

                lyra::Vector3 n(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);
                if (glm::length(n) < 1e-5f) {
                    continue;
                }
                n = glm::normalize(n);

                lyra::Vector2 encoded = lyra::octahedral_encode(n);
                CHECK(encoded.x >= -1.0f);
                CHECK(encoded.x <= 1.0f);
                CHECK(encoded.y >= -1.0f);
                CHECK(encoded.y <= 1.0f);

                lyra::Vector3 decoded = lyra::octahedral_decode(encoded);
                CHECK(glm::dot(decoded, n) > 0.9999f);
                CHECK(glm::length(decoded - n) < 1e-3f);
            }
        }
    }

    SUBCASE("SNORM & UNORM 16-bit Packing & Unpacking")
    {
        const lyra::Vector2 snorm_values[] = {
            {-1.0f, -1.0f},
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            {-0.5f, 0.5f},
            {0.333f, -0.666f},
        };

        for (const auto& v : snorm_values) {
            lyra::uint packed = lyra::pack_snorm_2x16(v);
            lyra::Vector2 unpacked = lyra::unpack_snorm_2x16(packed);
            CHECK(std::abs(unpacked.x - v.x) <= (1.0f / 32767.0f));
            CHECK(std::abs(unpacked.y - v.y) <= (1.0f / 32767.0f));
        }

        const lyra::Vector2 unorm_values[] = {
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            {0.5f, 0.25f},
            {0.1234f, 0.8765f},
        };

        for (const auto& v : unorm_values) {
            lyra::uint packed = lyra::pack_unorm_2x16(v);
            lyra::Vector2 unpacked = lyra::unpack_unorm_2x16(packed);
            CHECK(std::abs(unpacked.x - v.x) <= (1.0f / 65535.0f));
            CHECK(std::abs(unpacked.y - v.y) <= (1.0f / 65535.0f));
        }
    }

    SUBCASE("SNORM & UNORM 8-bit Packing & Unpacking")
    {
        const lyra::Vector4 unorm_values[] = {
            {0.0f, 0.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.1f, 0.3f, 0.6f, 0.9f},
        };

        for (const auto& v : unorm_values) {
            lyra::uint packed = lyra::pack_unorm_4x8(v);
            lyra::Vector4 unpacked = lyra::unpack_unorm_4x8(packed);
            CHECK(std::abs(unpacked.x - v.x) <= (1.0f / 255.0f));
            CHECK(std::abs(unpacked.y - v.y) <= (1.0f / 255.0f));
            CHECK(std::abs(unpacked.z - v.z) <= (1.0f / 255.0f));
            CHECK(std::abs(unpacked.w - v.w) <= (1.0f / 255.0f));
        }

        const lyra::Vector4 snorm_values[] = {
            {-1.0f, -0.5f, 0.0f, 1.0f},
            {0.25f, -0.25f, 0.75f, -0.75f},
        };

        for (const auto& v : snorm_values) {
            lyra::uint packed = lyra::pack_snorm_4x8(v);
            lyra::Vector4 unpacked = lyra::unpack_snorm_4x8(packed);
            CHECK(std::abs(unpacked.x - v.x) <= (1.0f / 127.0f));
            CHECK(std::abs(unpacked.y - v.y) <= (1.0f / 127.0f));
            CHECK(std::abs(unpacked.z - v.z) <= (1.0f / 127.0f));
            CHECK(std::abs(unpacked.w - v.w) <= (1.0f / 127.0f));
        }
    }
}
