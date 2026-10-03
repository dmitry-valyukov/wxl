#include <gtest/gtest.h>

#include "wxl.gen.h"

TEST(group_namespaces, mutually_dependent_namespaces_share_the_shortest_name) {
    auto const group_of = gen::group_namespaces({
        {"Microsoft.UI.Xaml.Controls", {"Microsoft.UI.Xaml.Controls.Primitives"}},
        {"Microsoft.UI.Xaml.Controls.Primitives", {"Microsoft.UI.Xaml.Controls"}},
        {"Microsoft.UI.Xaml.Media", {"Microsoft.UI.Xaml"}},
    });

    EXPECT_EQ(group_of.at("Microsoft.UI.Xaml.Controls"), "Microsoft.UI.Xaml.Controls");
    EXPECT_EQ(group_of.at("Microsoft.UI.Xaml.Controls.Primitives"), "Microsoft.UI.Xaml.Controls");
    // A one-way dependency does not merge: the base's namespace keeps its own
    // file and the dependent one includes it.
    EXPECT_EQ(group_of.at("Microsoft.UI.Xaml.Media"), "Microsoft.UI.Xaml.Media");
    EXPECT_EQ(group_of.at("Microsoft.UI.Xaml"), "Microsoft.UI.Xaml");
}

TEST(group_namespaces, a_cycle_through_a_third_namespace_is_one_group) {
    auto const group_of = gen::group_namespaces({
        {"A.Long", {"A.Mid"}},
        {"A.Mid", {"A.X"}},
        {"A.X", {"A.Long"}},
    });

    EXPECT_EQ(group_of.at("A.Long"), "A.X");
    EXPECT_EQ(group_of.at("A.Mid"), "A.X");
    EXPECT_EQ(group_of.at("A.X"), "A.X");
}

// Of two names of the same length the lesser one names the group.
TEST(group_namespaces, a_tie_in_length_goes_to_the_lesser_name) {
    auto const group_of = gen::group_namespaces({
        {"B.Y", {"A.Z"}},
        {"A.Z", {"B.Y"}},
    });

    EXPECT_EQ(group_of.at("B.Y"), "A.Z");
    EXPECT_EQ(group_of.at("A.Z"), "A.Z");
}
