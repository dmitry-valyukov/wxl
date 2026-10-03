#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "profile.h"

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
