#include <cradle/background/requests.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("value requests", "[background]")
{
    auto four = rq::value(4);
    auto two = rq::value(2);
    auto another_four = rq::value(4);

    REQUIRE(four.is_resolved());

    REQUIRE(four.value_id() == another_four.value_id());
    REQUIRE(four.value_id() != two.value_id());

    bool was_dispatched = false;
    four.dispatch([&](int value) {
        was_dispatched = true;
        REQUIRE(value == 4);
    });
    REQUIRE(was_dispatched);
}
