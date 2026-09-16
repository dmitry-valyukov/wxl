#include <gtest/gtest.h>

#include <string_view>

import wxl.logging;

using wxl::logging::severity;
using wxl::logging::severity_range;

namespace {

TEST(severity, the_order_is_worst_first) {
    EXPECT_LT(severity::fatal, severity::error);
    EXPECT_LT(severity::error, severity::warn);
    EXPECT_LT(severity::warn, severity::info);
    EXPECT_LT(severity::info, severity::trace);
    EXPECT_LT(severity::trace, severity::debug);
}

TEST(severity, a_filter_passes_itself_and_everything_worse) {
    EXPECT_TRUE(wxl::logging::is_enabled(severity::fatal, severity::info));
    EXPECT_TRUE(wxl::logging::is_enabled(severity::info, severity::info));
    EXPECT_FALSE(wxl::logging::is_enabled(severity::trace, severity::info));
    EXPECT_FALSE(wxl::logging::is_enabled(severity::debug, severity::info));

    // The quietest setting still lets the thing that kills the program through.
    EXPECT_TRUE(wxl::logging::is_enabled(severity::fatal, severity::fatal));
    EXPECT_FALSE(wxl::logging::is_enabled(severity::error, severity::fatal));
}

TEST(severity, the_names_are_ascii_and_uppercase) {
    EXPECT_EQ(wxl::logging::severity_name(severity::fatal), "FATAL");
    EXPECT_EQ(wxl::logging::severity_name(severity::error), "ERROR");
    EXPECT_EQ(wxl::logging::severity_name(severity::warn), "WARN");
    EXPECT_EQ(wxl::logging::severity_name(severity::info), "INFO");
    EXPECT_EQ(wxl::logging::severity_name(severity::trace), "TRACE");
    EXPECT_EQ(wxl::logging::severity_name(severity::debug), "DEBUG");
}

TEST(severity, a_name_is_known_while_compiling) {
    static_assert(wxl::logging::severity_name(severity::warn) == std::string_view("WARN"));
}

TEST(severity, a_band_holds_its_ends) {
    constexpr severity_range down_to_info{.from = severity::fatal, .to = severity::info};

    EXPECT_TRUE(down_to_info.contains(severity::fatal));
    EXPECT_TRUE(down_to_info.contains(severity::info));
    EXPECT_FALSE(down_to_info.contains(severity::trace));

    constexpr severity_range only_the_noise{.from = severity::trace, .to = severity::debug};

    EXPECT_FALSE(only_the_noise.contains(severity::info));
    EXPECT_TRUE(only_the_noise.contains(severity::trace));
    EXPECT_TRUE(only_the_noise.contains(severity::debug));
}

TEST(severity, the_default_band_is_everything) {
    constexpr severity_range everything;

    EXPECT_TRUE(everything.contains(severity::fatal));
    EXPECT_TRUE(everything.contains(severity::debug));
}

TEST(severity, this_build_compiles_every_level) {
    static_assert(wxl::logging::is_compiled(severity::debug),
                  "the floor is meant to be at debug unless somebody moved it deliberately");

    EXPECT_TRUE(wxl::logging::is_compiled(severity::fatal));
}

}  // namespace
