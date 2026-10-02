#include <gtest/gtest.h>

#include "profile.h"

TEST(member_filter, allows) {
    EXPECT_TRUE(member_filter::all().allows("Width"));
    EXPECT_FALSE(member_filter::none().allows("Width"));
    EXPECT_TRUE(member_filter::allow({"Width"}).allows("Width"));
    EXPECT_FALSE(member_filter::allow({"Width"}).allows("Height"));
    EXPECT_FALSE(member_filter::deny({"Width"}).allows("Width"));
    EXPECT_TRUE(member_filter::deny({"Width"}).allows("Height"));
}

TEST(member_filter, all_and_none_absorb) {
    auto all = member_filter::all();
    all.merge(member_filter::allow({"Width"}));
    EXPECT_EQ(all, member_filter::all());

    auto some = member_filter::allow({"Width"});
    some.merge(member_filter::none());
    EXPECT_EQ(some, member_filter::allow({"Width"}));

    auto none = member_filter::none();
    none.merge(member_filter::deny({"Width"}));
    EXPECT_EQ(none, member_filter::deny({"Width"}));

    auto denied = member_filter::deny({"Width"});
    denied.merge(member_filter::all());
    EXPECT_EQ(denied, member_filter::all());
}

TEST(member_filter, allow_lists_unite) {
    auto filter = member_filter::allow({"Width"});
    filter.merge(member_filter::allow({"Height"}));
    EXPECT_EQ(filter, member_filter::allow({"Width", "Height"}));
}

TEST(member_filter, deny_lists_intersect) {
    auto filter = member_filter::deny({"Width", "Height"});
    filter.merge(member_filter::deny({"Height", "Margin"}));
    EXPECT_EQ(filter, member_filter::deny({"Height"}));
}

// Allow(A) + Deny(B) is everything except B, plus A: Deny(B \ A), whichever
// side came first.
TEST(member_filter, allow_and_deny_leave_the_difference_denied) {
    auto allow_first = member_filter::allow({"Width"});
    allow_first.merge(member_filter::deny({"Width", "Height"}));
    EXPECT_EQ(allow_first, member_filter::deny({"Height"}));

    auto deny_first = member_filter::deny({"Width", "Height"});
    deny_first.merge(member_filter::allow({"Width"}));
    EXPECT_EQ(deny_first, member_filter::deny({"Height"}));
}
