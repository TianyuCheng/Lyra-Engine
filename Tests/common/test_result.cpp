#include <doctest/doctest.h>
#include <Lyra/Utilities/Result.h>

namespace
{
    auto divide(int a, int b) -> lyra::Result<int, lyra::String>
    {
        if (b == 0) {
            return lyra::make_unexpected("division by zero");
        }
        return a / b;
    }

    auto divide_default_error(int a, int b) -> lyra::Result<int>
    {
        if (b == 0) {
            return lyra::make_unexpected("division by zero");
        }
        return a / b;
    }

    auto validate_positive(int a) -> lyra::Result<void, lyra::String>
    {
        if (a <= 0) {
            return lyra::make_unexpected("value must be positive");
        }
        return {};
    }
} // namespace

TEST_CASE("utilities::result" * doctest::description("Lyra Result<T, E> and Expected/Unexpected"))
{
    SUBCASE("Basic value and error inspection")
    {
        auto success = divide(20, 4);
        CHECK(success.has_value());
        CHECK(static_cast<bool>(success));
        CHECK(success.value() == 5);
        CHECK(*success == 5);

        auto failure = divide(20, 0);
        CHECK_FALSE(failure.has_value());
        CHECK_FALSE(static_cast<bool>(failure));
        CHECK(failure.error() == "division by zero");
    }

    SUBCASE("Result<T> synonym with default String error")
    {
        auto success = divide_default_error(100, 5);
        CHECK(success.has_value());
        CHECK(*success == 20);

        auto failure = divide_default_error(100, 0);
        CHECK_FALSE(failure.has_value());
        CHECK(failure.error() == "division by zero");
    }

    SUBCASE("Result<void, E>")
    {
        auto ok_res = validate_positive(10);
        CHECK(ok_res.has_value());

        auto err_res = validate_positive(-5);
        CHECK_FALSE(err_res.has_value());
        CHECK(err_res.error() == "value must be positive");
    }

    SUBCASE("value_or fallback")
    {
        auto success = divide(10, 2);
        CHECK(success.value_or(999) == 5);

        auto failure = divide(10, 0);
        CHECK(failure.value_or(999) == 999);
    }

    SUBCASE("Monadic map and map_error")
    {
        auto mapped = divide(10, 2).map([](int val) {
            return val * 10;
        });
        CHECK(mapped.has_value());
        CHECK(*mapped == 50);

        auto err_mapped = divide(10, 0).map_error([](const lyra::String& err) {
            return "Fatal: " + err;
        });
        CHECK_FALSE(err_mapped.has_value());
        CHECK(err_mapped.error() == "Fatal: division by zero");
    }

    SUBCASE("Monadic and_then and or_else")
    {
        auto chained = divide(100, 2).and_then([](int val) {
            return divide(val, 5);
        });
        CHECK(chained.has_value());
        CHECK(*chained == 10);

        auto chained_fail = divide(100, 2).and_then([](int val) {
            return divide(val, 0);
        });
        CHECK_FALSE(chained_fail.has_value());
        CHECK(chained_fail.error() == "division by zero");

        auto recovered = divide(100, 0).or_else([](const lyra::String&) -> lyra::Result<int, lyra::String> {
            return 42;
        });
        CHECK(recovered.has_value());
        CHECK(*recovered == 42);
    }

    SUBCASE("Unexpected construction with deduction guide and Expected alias")
    {
        lyra::Unexpected                 unexp("custom error");
        lyra::Expected<int, const char*> exp = unexp;
        CHECK_FALSE(exp.has_value());
        CHECK(exp.error() == doctest::String("custom error"));
    }
}
