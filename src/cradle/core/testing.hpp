#ifndef CRADLE_CORE_TESTING_HPP
#define CRADLE_CORE_TESTING_HPP

#define CATCH_CONFIG_CPP11_NO_NULLPTR
#include <catch.hpp>

#include <cradle/core/dynamic.hpp>
#include <cradle/core/regular.hpp>

namespace cradle {

// Test that a type correctly implements the CRADLE Regular type interface for
// the given value. Note that this only tests the portions of the interface that
// are relevant for a single value. (In particular, it's intended to work for
// types that only have one possible value.)
template<class T>
void
test_regular_value(T const& x)
{
    BOOST_CONCEPT_ASSERT((Regular<T>) );

    {
        INFO("Copy construction should produce an equal value.")
        T y = x;
        REQUIRE(y == x);
    }

    {
        INFO("Assignment should produce an equal value.")
        T y;
        y = x;
        REQUIRE(y == x);
    }

    {
        INFO("std::swap should swap values.")

        T default_initialized = T();

        T y = x;
        T z = default_initialized;
        REQUIRE(y == x);
        REQUIRE(z == default_initialized);

        using std::swap;
        swap(y, z);
        REQUIRE(z == x);
        REQUIRE(y == default_initialized);

        INFO("A second std::swap should restore the original values.")
        swap(y, z);
        REQUIRE(y == x);
        REQUIRE(z == default_initialized);
    }

    {
        INFO(
            "Conversion to cradle::dynamic and then back should produce an "
            "equal value.")
        cradle::dynamic v;
        to_dynamic(&v, x);
        T y;
        from_dynamic(&y, v);
        REQUIRE(y == x);
    }
}

// Test that a type correctly implements the CRADLE Regular type interface for
// the given pair of values. This does additional tests that require two
// different values. It assumes that :x < :y and that the two produce different
// hash values.
template<class T>
void
test_regular_value_pair(T const& x, T const& y)
{
    // Test the values individually.
    test_regular_value(x);
    test_regular_value(y);

    // :test_pair accepts a second pair of values (:a, :b) which is meant to
    // match the original pair (:x, :y). It tests that this is true and also
    // tests the original assumptions about the relationship between :x and :y.
    auto test_pair = [&](T const& a, T const& b) {
        REQUIRE(a != b);
        REQUIRE(invoke_hash(a) != invoke_hash(b));
        REQUIRE(a < b);
        REQUIRE(a == x);
        REQUIRE(b == y);
    };

    // Test the original values.
    test_pair(x, y);

    // Test copy construction, assignment, and swapping.
    {
        using std::swap;
        T a = x;
        T b = y;
        test_pair(a, b);
        b = x;
        a = y;
        test_pair(b, a);
        swap(a, b);
        test_pair(a, b);
        swap(a, b);
        test_pair(b, a);
    }
}

} // namespace cradle

#endif
