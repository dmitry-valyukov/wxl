#include <gtest/gtest.h>

#include "profile.h"

TEST(member_filter, allows) {
    EXPECT_TRUE(MemberFilter::all().allows("Width"));
    EXPECT_FALSE(MemberFilter::none().allows("Width"));
    EXPECT_TRUE(MemberFilter::allow({"Width"}).allows("Width"));
    EXPECT_FALSE(MemberFilter::allow({"Width"}).allows("Height"));
    EXPECT_FALSE(MemberFilter::deny({"Width"}).allows("Width"));
    EXPECT_TRUE(MemberFilter::deny({"Width"}).allows("Height"));
}

TEST(member_filter, all_and_none_absorb) {
    auto all = MemberFilter::all();
    all.merge(MemberFilter::allow({"Width"}));
    EXPECT_EQ(all, MemberFilter::all());

    auto some = MemberFilter::allow({"Width"});
    some.merge(MemberFilter::none());
    EXPECT_EQ(some, MemberFilter::allow({"Width"}));

    auto none = MemberFilter::none();
    none.merge(MemberFilter::deny({"Width"}));
    EXPECT_EQ(none, MemberFilter::deny({"Width"}));

    auto denied = MemberFilter::deny({"Width"});
    denied.merge(MemberFilter::all());
    EXPECT_EQ(denied, MemberFilter::all());
}

TEST(member_filter, allow_lists_unite) {
    auto filter = MemberFilter::allow({"Width"});
    filter.merge(MemberFilter::allow({"Height"}));
    EXPECT_EQ(filter, MemberFilter::allow({"Width", "Height"}));
}

TEST(member_filter, deny_lists_intersect) {
    auto filter = MemberFilter::deny({"Width", "Height"});
    filter.merge(MemberFilter::deny({"Height", "Margin"}));
    EXPECT_EQ(filter, MemberFilter::deny({"Height"}));
}

// Allow(A) + Deny(B) is everything except B, plus A: Deny(B \ A), whichever
// side came first.
TEST(member_filter, allow_and_deny_leave_the_difference_denied) {
    auto allow_first = MemberFilter::allow({"Width"});
    allow_first.merge(MemberFilter::deny({"Width", "Height"}));
    EXPECT_EQ(allow_first, MemberFilter::deny({"Height"}));

    auto deny_first = MemberFilter::deny({"Width", "Height"});
    deny_first.merge(MemberFilter::allow({"Width"}));
    EXPECT_EQ(deny_first, MemberFilter::deny({"Height"}));
}
