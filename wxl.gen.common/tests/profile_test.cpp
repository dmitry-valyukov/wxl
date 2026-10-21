#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include "profile.h"

import wxl.core;

namespace {

std::filesystem::path const profiles_dir{WXL_GEN_PROFILES_DIR};

// SystemRoot for the life of the object, the previous value back after.
class system_root {
public:
    explicit system_root(char const* value) {
        if (char const* previous = std::getenv("SystemRoot")) {
            previous_ = previous;
        }
        _putenv_s("SystemRoot", value);
    }
    ~system_root() { _putenv_s("SystemRoot", previous_.c_str()); }

private:
    std::string previous_;
};

}  // namespace

// A profile that asks for the Windows metadata on a machine without it still
// resolves -- without that metadata the walk stops at every Windows type, which
// a run reports rather than refuses -- and names the directory it looked in.
TEST(profile, missing_windows_metadata_is_named) {
    system_root const nowhere{"M:\\no-such-windows"};
    auto const profiles = resolve_profiles({profiles_dir / "base.json"}, default_nuget_root());

    EXPECT_EQ(profiles.missing_windows_metadata,
              std::filesystem::path{"M:\\no-such-windows"} / "System32" / "WinMetadata");
    for (auto&& file : profiles.metadata) {
        EXPECT_FALSE(file.string().starts_with("M:\\no-such-windows")) << file;
    }
}

TEST(profile, present_windows_metadata_is_read) {
    auto const profiles = resolve_profiles({profiles_dir / "base.json"}, default_nuget_root());

    EXPECT_TRUE(profiles.missing_windows_metadata.empty()) << profiles.missing_windows_metadata;
    bool windows = false;
    for (auto&& file : profiles.metadata) {
        windows = windows || file.filename() == "Windows.Foundation.winmd";
    }
    EXPECT_TRUE(windows);
}

// A file system error as a person reads it: the path as written, then the system's own
// words for the code -- all of it UTF-8, whatever the ANSI code page of the machine.
TEST(error_message, a_file_system_error_is_its_path_and_the_systems_words) {
    // "справка", spelled out so that the source's own encoding does not matter.
    std::filesystem::path const file{L"C:\\wxl\\\u0441\u043F\u0440\u0430\u0432\u043A\u0430.xml"};
    constexpr int access_denied = 5;  // ERROR_ACCESS_DENIED
    std::filesystem::filesystem_error const error{"exists", file,
                                                  std::error_code{access_denied, std::system_category()}};

    std::string const message = error_message(error);

    EXPECT_TRUE(wxl::core::unicode::is_valid_utf8(message)) << message;
    std::string_view const name = "C:\\wxl\\\xD1\x81\xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0.xml: ";
    ASSERT_TRUE(message.starts_with(name)) << message;
    EXPECT_EQ(message.substr(name.size()),
              wxl::core::environment::error_explanation(access_denied).chars());
    EXPECT_GT(message.size(), name.size()) << message;
}

// Any other error is its own text, and what of it is not UTF-8 comes out marked, not lost.
TEST(error_message, other_text_keeps_what_is_not_utf8_marked) {
    EXPECT_EQ(error_message(std::runtime_error{"cannot read \xC2\xA0" "file \xFF here"}),
              "cannot read \xC2\xA0" "file \xEF\xBF\xBD here");
}
