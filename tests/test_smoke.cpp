#include <string_view>

#include "harness.hpp"
#include "kr8/version.hpp"

TEST(version_is_not_empty) {
    CHECK(!kr8::version().empty());
}

TEST(version_matches_compile_definition) {
    CHECK_EQ(kr8::version(), std::string_view(KR8_VERSION));
}

TEST(version_starts_with_a_digit) {
    CHECK(!kr8::version().empty());
    CHECK(kr8::version().front() >= '0');
    CHECK(kr8::version().front() <= '9');
}
