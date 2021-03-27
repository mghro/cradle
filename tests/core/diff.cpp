#include <cradle/core/diff.hpp>

#include <cradle/common.hpp>
#include <cradle/core/testing.hpp>

using namespace cradle;

void
test_diff(dynamic const& a, dynamic const& b, value_diff const& expected_diff)
{
    auto diff = compute_value_diff(a, b);
    REQUIRE(diff == expected_diff);
    REQUIRE(apply_value_diff(a, diff) == b);
}

TEST_CASE("simple diffs", "[core][diff]")
{
    test_diff(
        dynamic("foo"),
        dynamic("bar"),
        {make_value_diff_item(
            {},
            value_diff_op::UPDATE,
            some(dynamic("foo")),
            some(dynamic("bar")))});
}

TEST_CASE("array diffs", "[core][diff]")
{
    test_diff(
        dynamic{0., 1., 2.},
        dynamic{0., 1., 3.},
        {make_value_diff_item(
            {integer(2)},
            value_diff_op::UPDATE,
            some(dynamic(2.)),
            some(dynamic(3.)))});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{1., 3.},
        {make_value_diff_item(
            {},
            value_diff_op::UPDATE,
            some(dynamic{0., 1., 2.}),
            some(dynamic{1., 3.}))});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{1., 3., 2.},
        {make_value_diff_item(
             {integer(0)},
             value_diff_op::UPDATE,
             some(dynamic(0.)),
             some(dynamic(1.))),
         make_value_diff_item(
             {integer(1)},
             value_diff_op::UPDATE,
             some(dynamic(1.)),
             some(dynamic(3.)))});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{1., 1., 2.},
        {make_value_diff_item(
            {integer(0)},
            value_diff_op::UPDATE,
            some(dynamic(0.)),
            some(dynamic(1.)))});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{0., 1.},
        {make_value_diff_item(
            {integer(2)}, value_diff_op::DELETE, some(dynamic(2.)), none)});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{0., 2.},
        {make_value_diff_item(
            {integer(1)}, value_diff_op::DELETE, some(dynamic(1.)), none)});

    test_diff(
        dynamic{0., 1., 2.},
        dynamic{1., 2.},
        {make_value_diff_item(
            {integer(0)}, value_diff_op::DELETE, some(dynamic(0.)), none)});

    test_diff(
        dynamic{3., 1., 2.},
        dynamic{2.},
        {make_value_diff_item(
             {integer(1)}, value_diff_op::DELETE, some(dynamic(1.)), none),
         make_value_diff_item(
             {integer(0)}, value_diff_op::DELETE, some(dynamic(3.)), none)});

    test_diff(
        dynamic{3., 1., 0., 2.},
        dynamic{2.},
        {make_value_diff_item(
            {},
            value_diff_op::UPDATE,
            some(dynamic{3., 1., 0., 2.}),
            some(dynamic{2.}))});

    test_diff(
        dynamic{0., 1.},
        dynamic{0., 2., 1.},
        {make_value_diff_item(
            {integer(1)}, value_diff_op::INSERT, none, dynamic(2.))});

    test_diff(
        dynamic{1., 2.},
        dynamic{0., 1., 2.},
        {make_value_diff_item(
            {integer(0)}, value_diff_op::INSERT, none, dynamic(0.))});

    test_diff(
        dynamic{0., 1.},
        dynamic{0., 1., 2.},
        {make_value_diff_item(
            {integer(2)}, value_diff_op::INSERT, none, dynamic(2.))});

    test_diff(
        dynamic{0.},
        dynamic{0., 3., 2.},
        {make_value_diff_item(
             {integer(1)}, value_diff_op::INSERT, none, dynamic(3.)),
         make_value_diff_item(
             {integer(2)}, value_diff_op::INSERT, none, dynamic(2.))});
}

TEST_CASE("map diffs", "[core][diff]")
{
    test_diff(
        dynamic{{"foo", 0.}, {"bar", 1.}},
        dynamic{{"foo", 3.}, {"bar", 1.}},
        {make_value_diff_item(
            {dynamic("foo")},
            value_diff_op::UPDATE,
            some(dynamic(0.)),
            some(dynamic(3.)))});

    test_diff(
        dynamic{{"foo", 0.}, {"bar", 1.}},
        dynamic{{"foo", 0.}},
        {make_value_diff_item(
            {dynamic("bar")},
            value_diff_op::DELETE,
            some(dynamic(1.)),
            none)});

    test_diff(
        dynamic{{"foo", 0.}},
        dynamic{{"foo", 0.}, {"bar", 1.}},
        {make_value_diff_item(
            {dynamic("bar")},
            value_diff_op::INSERT,
            none,
            some(dynamic(1.)))});

    test_diff(
        dynamic{{"abc", 1.}, {"foo", 0.}, {"bar", 1.}},
        dynamic{{"abc", 1.}, {"foo", 3.}, {"baz", 0.}},
        {make_value_diff_item(
             {dynamic("bar")}, value_diff_op::DELETE, dynamic(1.), none),
         make_value_diff_item(
             {dynamic("baz")}, value_diff_op::INSERT, none, some(dynamic(0.))),
         make_value_diff_item(
             {dynamic("foo")},
             value_diff_op::UPDATE,
             some(dynamic(0.)),
             some(dynamic(3.)))});
}

TEST_CASE("nested diffs", "[core][diff]")
{
    auto map_a = dynamic{{"foo", 0.}, {"bar", 1.}};
    auto map_b = dynamic{{"foo", 3.}, {"baz", 0.}};
    auto map_c = dynamic{{"related", 0.}};
    auto map_d = dynamic{{"un", 5.}, {"related", 0.}};

    test_diff(
        dynamic{map_c, map_a},
        dynamic{map_d, map_b},
        {make_value_diff_item(
             {integer(0), dynamic("un")},
             value_diff_op::INSERT,
             none,
             some(dynamic(5.))),
         make_value_diff_item(
             {integer(1)}, value_diff_op::UPDATE, some(map_a), some(map_b))});

    auto map_e = dynamic{{"un", {0., 5.}}, {"related", 0.}};
    auto map_f = dynamic{{"un", {0., 4.}}, {"related", 0.}};

    test_diff(
        dynamic{map_a, map_b, map_e},
        dynamic{map_a, map_b, map_f},
        {make_value_diff_item(
            {integer(2), dynamic("un"), integer(1)},
            value_diff_op::UPDATE,
            some(dynamic(5.)),
            some(dynamic(4.)))});
}
