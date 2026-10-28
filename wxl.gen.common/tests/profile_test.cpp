// windows.h for SetEnvironmentVariableW: the profiles read the variable from the
// process's own environment block, so that is where it is set.
#include "platform.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "profile.h"

import wxl.core;

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// An environment variable set for the life of the object -- removed, for a null
// value -- and the previous value back after.
class variable_override {
public:
    variable_override(wchar_t const* name, wchar_t const* value)
        : name_{name}, previous_{wxl::core::environment::variable(name)} {
        ::SetEnvironmentVariableW(name_, value);
    }
    variable_override(variable_override const&) = delete;
    variable_override& operator=(variable_override const&) = delete;
    ~variable_override() {
        ::SetEnvironmentVariableW(name_, previous_.empty() ? nullptr : previous_.c_str());
    }

private:
    wchar_t const* const name_;
    std::wstring const previous_;
};

// Two paths compared as paths, element by element, so that / and \ are one separator,
// and shown through to_utf8() when they differ: operator<< on a path goes through
// string(), which throws on a name the ANSI code page cannot write.
::testing::AssertionResult same_path(std::filesystem::path const& actual,
                                     std::filesystem::path const& expected) {
    if (actual == expected) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << wxl::core::unicode::to_utf8(actual).chars() << " vs "
                                         << wxl::core::unicode::to_utf8(expected).chars();
}

// A directory of the test's own under %TEMP%, empty.
std::filesystem::path scratch(std::wstring_view name) {
    auto const directory =
        std::filesystem::temp_directory_path() / L"wxl.gen.common.profile" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

// A NuGet.Config under `appdata` whose globalPackagesFolder is `value`, as bytes.
void write_nuget_config(std::filesystem::path const& appdata, std::string_view value) {
    std::filesystem::create_directories(appdata / L"NuGet");
    std::ofstream{appdata / L"NuGet" / L"NuGet.Config"}
        << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<configuration>\n  <config>\n"
           "    <add key=\"globalPackagesFolder\" value=\""
        << value << "\" />\n  </config>\n</configuration>\n";
}

}  // namespace

// A profile that asks for the Windows metadata on a machine without it still
// resolves -- without that metadata the walk stops at every Windows type, which
// a run reports rather than refuses -- and names the directory it looked in.
TEST(profile, missing_windows_metadata_is_named) {
    variable_override const nowhere{L"SystemRoot", L"M:\\no-such-windows"};
    auto const profiles = resolve_profiles({profiles_dir / "base.json"}, default_nuget_root());

    std::filesystem::path const expected =
        std::filesystem::path{L"M:\\no-such-windows"} / L"System32" / L"WinMetadata";
    EXPECT_TRUE(same_path(profiles.missing_windows_metadata, expected));
    for (auto&& file : profiles.metadata) {
        EXPECT_FALSE(file.native().starts_with(L"M:\\no-such-windows"))
            << wxl::core::unicode::to_utf8(file);
    }
}

TEST(profile, present_windows_metadata_is_read) {
    auto const profiles = resolve_profiles({profiles_dir / "base.json"}, default_nuget_root());

    EXPECT_TRUE(profiles.missing_windows_metadata.empty())
        << wxl::core::unicode::to_utf8(profiles.missing_windows_metadata);
    bool windows = false;
    for (auto&& file : profiles.metadata) {
        windows = windows || file.filename() == "Windows.Foundation.winmd";
    }
    EXPECT_TRUE(windows);
}

// A reference in Cyrillic -- "базовый", "папка/базовый.json", spelled out so that
// the source's own encoding does not matter -- names the file it spells, beside the
// profile that makes it.
TEST(profile, a_reference_in_cyrillic_names_the_file_it_spells) {
    std::filesystem::path const referrer{L"C:\\profiles\\full.json"};

    constexpr std::string_view basic = "\xD0\xB1\xD0\xB0\xD0\xB7\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9";
    EXPECT_TRUE(same_path(resolve_reference(basic, referrer),
                          L"C:\\profiles\\\u0431\u0430\u0437\u043E\u0432\u044B\u0439.json"));
    constexpr std::string_view in_folder =
        "\xD0\xBF\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0/"
        "\xD0\xB1\xD0\xB0\xD0\xB7\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9.json";
    EXPECT_TRUE(same_path(resolve_reference(in_folder, referrer),
                          L"C:\\profiles\\\u043F\u0430\u043F\u043A\u0430\\"
                          L"\u0431\u0430\u0437\u043E\u0432\u044B\u0439.json"));
}

// The packages folder NuGet.Config names, in Cyrillic -- "пакеты" -- is the folder
// the run reads, whatever the ANSI code page of the machine.
TEST(profile, the_packages_folder_nuget_config_names_in_cyrillic_is_read) {
    auto const appdata = scratch(L"nuget_config");
    auto const packages = appdata / L"\u043F\u0430\u043A\u0435\u0442\u044B";
    std::filesystem::create_directories(packages);
    write_nuget_config(appdata, wxl::core::unicode::to_utf8(packages).chars());

    variable_override const no_override{L"NUGET_PACKAGES", nullptr};
    variable_override const here{L"APPDATA", appdata.c_str()};

    EXPECT_TRUE(same_path(default_nuget_root(), packages));

    std::filesystem::remove_all(appdata);
}

// A value that is not UTF-8 is said, naming the file, rather than passed over for
// the default it exists to replace.
TEST(profile, a_packages_folder_that_is_not_utf8_is_said) {
    auto const appdata = scratch(L"nuget_config_broken");
    write_nuget_config(appdata, "C:\\packages\xFF");

    variable_override const no_override{L"NUGET_PACKAGES", nullptr};
    variable_override const here{L"APPDATA", appdata.c_str()};

    try {
        default_nuget_root();
        ADD_FAILURE() << "a NuGet.Config value that is not UTF-8 has to throw";
    } catch (std::runtime_error const& error) {
        auto const config = wxl::core::unicode::to_utf8(appdata / L"NuGet" / L"NuGet.Config");
        EXPECT_TRUE(std::string_view{error.what()}.starts_with(config.chars())) << error.what();
    }

    std::filesystem::remove_all(appdata);
}

// A profile that names itself nothing is named after its file, and a file named in
// Cyrillic -- "профиль", spelled out so that the source's own encoding does not
// matter -- gives that name in UTF-8, whatever the ANSI code page of the machine.
TEST(profile, takes_its_name_from_a_file_named_in_cyrillic) {
    auto const directory = scratch(L"name_from_file");
    auto const file = directory / L"\u043F\u0440\u043E\u0444\u0438\u043B\u044C.json";
    std::ofstream{file} << "{}";

    EXPECT_EQ(load_profile(file).name, "\xD0\xBF\xD1\x80\xD0\xBE\xD1\x84\xD0\xB8\xD0\xBB\xD1\x8C");

    std::filesystem::remove_all(directory);
}
