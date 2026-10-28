#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "profile.h"

namespace {

// A version the test spells correctly; one that does not parse stops the test that
// wrote it.
nuget_version v(std::string_view text) {
    auto const version = nuget_version::parse(text);
    if (!version) {
        throw std::invalid_argument(std::string{text} + " is not a version");
    }
    return *version;
}

}  // namespace

TEST(nuget_version, parses_what_nuget_writes) {
    for (std::string_view const text :
         {"1", "1.2", "1.2.3", "1.2.3.4", "2.5.4-experimental", "2.0.5-experimental2",
          "10.0.26100.4654", "2.0.250303.1", "1.0.0-rc.1+build.5", "1.0.0+20261028",
          "1.0.0-x-y.z--1", "01.002.0003", "2147483647.0", "1.0.0-0", "1.0.0-rc.0",
          "1.0.0-0a", "1.0.0+001", "1.0.0-rc.99999999999999999999"}) {
        EXPECT_TRUE(nuget_version::parse(text)) << text;
    }
}

TEST(nuget_version, refuses_what_is_not_a_version) {
    for (std::string_view const text :
         {"", "1.2.3.4.5", "1..2", ".1", "1.", "a", "1.a", "1.0-", "1.0-a..b", "1.0-a.",
          "1.0+", "1.0+a..b", "1.0.0-a_b", " 1.0", "1.0 ", "-1.0", "[1.0]", "1.0,2.0",
          "1.0.0-\xC3\xA9", "99999999999999999999999.0", "2147483648.0", "1.2147483648",
          "1.0.0-rc.01", "1.0.0-00", "1.0.0-007"}) {
        EXPECT_FALSE(nuget_version::parse(text)) << text;
    }
}

TEST(nuget_version, compares_the_numeric_parts_as_numbers) {
    EXPECT_GT(v("1.10.0"), v("1.9.0"));
    EXPECT_GT(v("2.0.250303.1"), v("2.0.250303"));
    EXPECT_GT(v("10.0.26100.4654"), v("10.0.26100.0"));
    EXPECT_LT(v("2.5.1"), v("2.6.4"));
    // A part not written is 0, and a leading zero changes nothing.
    EXPECT_EQ(v("1"), v("1.0.0.0"));
    EXPECT_EQ(v("1.0"), v("1.0.0"));
    EXPECT_EQ(v("01.002.0003"), v("1.2.3"));
}

TEST(nuget_version, puts_a_release_above_its_prereleases) {
    EXPECT_GT(v("2.5.4"), v("2.5.4-experimental"));
    EXPECT_GT(v("2.5.4-experimental"), v("2.5.3"));
    EXPECT_GT(v("2.5.4-experimental"), v("2.5.1"));
    EXPECT_GT(v("2.6.4"), v("2.5.4-experimental"));
    EXPECT_LT(v("1.0.0.1-rc"), v("1.0.0.1"));
    EXPECT_GT(v("1.0.0.1-rc"), v("1.0.0"));
}

// The order of the SemVer 2.0 specification, which NuGet keeps.
TEST(nuget_version, orders_prerelease_labels_identifier_by_identifier) {
    std::vector<nuget_version> const ascending{
        v("1.0.0-alpha"), v("1.0.0-alpha.1"), v("1.0.0-alpha.beta"), v("1.0.0-beta"),
        v("1.0.0-beta.2"), v("1.0.0-beta.11"), v("1.0.0-rc.1"), v("1.0.0")};
    for (std::size_t at = 1; at != ascending.size(); ++at) {
        EXPECT_LT(ascending[at - 1], ascending[at]) << at;
        EXPECT_GT(ascending[at], ascending[at - 1]) << at;
    }
}

TEST(nuget_version, compares_numeric_identifiers_as_numbers_and_below_text) {
    EXPECT_GT(v("1.0.0-rc.10"), v("1.0.0-rc.2"));
    EXPECT_GT(v("1.0.0-rc.2147483647"), v("1.0.0-rc.3"));
    EXPECT_LT(v("1.0.0-1"), v("1.0.0-a"));
    EXPECT_LT(v("1.0.0-999"), v("1.0.0-0a"));
    // Int32.TryParse takes a '-' as the sign: the identifier -1 is the number -1.
    EXPECT_LT(v("1.0.0--1"), v("1.0.0-0"));
}

// An identifier is a number only as far as Int32 goes; a longer run of digits is
// compared as text, as NuGet's CompareRelease does.
TEST(nuget_version, compares_digits_beyond_int32_as_text) {
    EXPECT_LT(v("1.0.0-rc.3"), v("1.0.0-rc.2147483648"));
    EXPECT_GT(v("1.0.0-rc.2147483648"), v("1.0.0-rc.10000000000"));
    EXPECT_LT(v("1.0.0-rc.123456789012345678901234567890"), v("1.0.0-rc.99999999999999999999"));
}

// Text is compared as text: "experimental10" is below "experimental2", as NuGet
// orders the two.
TEST(nuget_version, compares_other_identifiers_as_text_ignoring_case) {
    EXPECT_EQ(v("1.0.0-RC.1"), v("1.0.0-rc.1"));
    EXPECT_EQ(v("2.5.4-Experimental"), v("2.5.4-experimental"));
    EXPECT_LT(v("2.0.5-experimental10"), v("2.0.5-experimental2"));
    EXPECT_LT(v("2.0.5-experimental"), v("2.0.5-experimental2"));
    EXPECT_LT(v("1.0.0-A"), v("1.0.0-b"));
    EXPECT_LT(v("1.0.0-a"), v("1.0.0-B"));
    EXPECT_LT(v("1.0.0--"), v("1.0.0-0a"));
}

TEST(nuget_version, ignores_build_metadata) {
    EXPECT_EQ(v("1.0.0+a"), v("1.0.0+b"));
    EXPECT_EQ(v("1.0.0+a"), v("1.0.0"));
    EXPECT_EQ(v("1.0.0-rc+x.1"), v("1.0.0-rc"));
    EXPECT_LT(v("1.0.0-rc+zzz"), v("1.0.0+aaa"));
}
