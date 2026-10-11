#include <doctest/doctest.h>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/InputSystem/InputEnums.h>
#include <Lyra/JobSystem/Jobs.h>
#include <Lyra/Utilities/Geometry.h>

namespace
{
    enum class SampleColor : uint8_t
    {
        RED,
        GREEN,
        BLUE
    };
}

TEST_CASE("utilities::enums" * doctest::description("Enum utilities and magic_enum wrappers"))
{
    SUBCASE("enum_count_v and enum_size")
    {
        CHECK(lyra::enum_count_v<SampleColor> == 3);
        CHECK(lyra::enum_size_v<SampleColor> == 3);
        CHECK(lyra::enum_size<SampleColor>() == 3);
        CHECK(lyra::enum_count<SampleColor>() == 3);

        // engine enums without COUNT
        CHECK(lyra::enum_count_v<lyra::JobPriority> == 3);
        CHECK(lyra::enum_count_v<lyra::Frustum::PlaneIndex> == 6);
        CHECK(lyra::enum_count_v<lyra::InputAxis2D> == 6);
    }

    SUBCASE("enum_to_string and enum_name")
    {
        CHECK(lyra::enum_name(SampleColor::RED) == "RED");
        CHECK(lyra::enum_name(SampleColor::GREEN) == "GREEN");
        CHECK(lyra::enum_name(SampleColor::BLUE) == "BLUE");

        lyra::String red_str = lyra::enum_to_string(SampleColor::RED);
        CHECK(red_str == "RED");
    }

    SUBCASE("enum_from_name and enum_is_valid")
    {
        auto red = lyra::enum_from_name<SampleColor>("RED");
        CHECK(red.has_value());
        CHECK(*red == SampleColor::RED);

        auto invalid = lyra::enum_from_name<SampleColor>("YELLOW");
        CHECK(!invalid.has_value());

        CHECK(lyra::enum_is_valid(SampleColor::GREEN));
        CHECK(lyra::enum_is_valid(static_cast<SampleColor>(1)));
        CHECK(!lyra::enum_is_valid(static_cast<SampleColor>(99)));
    }

    SUBCASE("enum_values_v and enum_for_each")
    {
        size_t count = 0;
        lyra::enum_for_each<SampleColor>([&](SampleColor color) {
            count++;
        });
        CHECK(count == 3);
        CHECK(lyra::enum_values_v<SampleColor>.size() == 3);
    }
}
