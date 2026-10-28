#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "profile.h"

import wxl.core;

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// The list the configure of this build wrote: the packages its restore resolved
// and the Windows SDK of its projection.
std::filesystem::path const restored_packages_file{u8"" WXL_RESTORED_PACKAGES};

// Two paths compared as paths, element by element, so that / and \ are one separator,
// and shown through to_utf8() when they differ: operator<< on a path goes through
// string(), which throws on a name the ANSI code page cannot write.
::testing::AssertionResult same_path(std::filesystem::path const& actual,
                                     std::filesystem::path const& expected) {
    if (actual == expected) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << wxl::core::unicode::to_utf8(actual).chars() << " vs "
                                         << wxl::core::unicode::to_utf8(expected).chars();
}

std::string utf8(std::filesystem::path const& path) {
    return std::string{wxl::core::unicode::to_utf8(path).chars()};
}

// A path as a JSON string writes it: in UTF-8, with / for the separator, which
// needs no escape.
std::string json_path(std::filesystem::path const& path) {
    std::string text = utf8(path);
    std::ranges::replace(text, '\\', '/');
    return text;
}

// A directory of the test's own under %TEMP%, empty.
std::filesystem::path scratch(std::wstring_view name) {
    auto const directory =
        std::filesystem::temp_directory_path() / L"wxl.gen.common.profile" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void write(std::filesystem::path const& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary} << text;
}

// One package of a list of the restore, as JSON.
std::string listed(std::string_view id, std::string_view version,
                   std::filesystem::path const& directory) {
    return std::string{"{\"id\": \""} + std::string{id} + "\", \"version\": \"" +
           std::string{version} + "\", \"directory\": \"" + json_path(directory) + "\"}";
}

// A list of the restore in `directory` with these packages, and a Windows SDK
// of `sdk_version` whose one .winmd file is there too (empty: nothing here opens
// it).
std::filesystem::path write_list(std::filesystem::path const& directory,
                                 std::vector<std::string> const& packages,
                                 std::string_view sdk_version = "10.0.26100.0") {
    auto const winmd = directory / L"sdk" / L"Windows.Foundation.FoundationContract.winmd";
    write(winmd, "");
    std::string text = "{\n    \"packages\": [";
    for (std::size_t at = 0; at != packages.size(); ++at) {
        text += (at ? ",\n        " : "\n        ") + packages[at];
    }
    text += "\n    ],\n    \"windowsSdk\": {\"version\": \"" + std::string{sdk_version} +
            "\", \"metadata\": [\"" + json_path(winmd) + "\"]}\n}\n";
    auto const file = directory / L"restored-packages.json";
    write(file, text);
    return file;
}

// A package directory with one .winmd file under metadata/.
std::filesystem::path write_package(std::filesystem::path const& directory) {
    write(directory / L"metadata" / L"Test.winmd", "");
    return directory;
}

// What the call stops with; a failure of the test when it does not stop.
template <typename F>
std::string stops_with(F&& call) {
    try {
        call();
    } catch (std::runtime_error const& error) {
        return error.what();
    }
    ADD_FAILURE() << "the call had to stop with an error";
    return {};
}

// Whether `text` holds every one of `parts`, each named when it is missing.
::testing::AssertionResult says(std::string_view text, std::vector<std::string> const& parts) {
    for (auto&& part : parts) {
        if (text.find(part) == std::string_view::npos) {
            return ::testing::AssertionFailure() << "no \"" << part << "\" in: " << text;
        }
    }
    return ::testing::AssertionSuccess();
}

}  // namespace

// The Windows metadata a profile asks for is that of the Windows SDK the build
// projects, file for file as the list of the restore names it.
TEST(profile, the_windows_metadata_is_the_one_the_build_projects) {
    auto const restored = load_restored_packages(restored_packages_file);
    ASSERT_FALSE(restored.sdk.metadata.empty());
    auto const profiles = resolve_profiles({profiles_dir / "base.winxl-profile"}, restored);

    for (auto&& file : restored.sdk.metadata) {
        EXPECT_NE(std::ranges::find(profiles.metadata, file), profiles.metadata.end())
            << utf8(file);
    }
}

// A package is read from the directory the list names, whatever case the profile
// spells its id in, and in a folder named in Cyrillic -- "пакеты", spelled out so
// that the source's own encoding does not matter -- whatever the ANSI code page
// of the machine.
TEST(profile, reads_a_package_from_the_directory_the_restore_names) {
    auto const directory = scratch(L"package_directory");
    auto const package = write_package(directory / L"\u043F\u0430\u043A\u0435\u0442\u044B" /
                                       L"test.package" / L"1.2.0");
    auto const list = write_list(directory, {listed("Test.Package", "1.2.0", package)});
    write(directory / L"p.winxl-profile",
          R"({"packages": [{"id": "test.package", "minimumVersion": "1.2.0-beta"}]})");

    auto const profiles =
        resolve_profiles({directory / L"p.winxl-profile"}, load_restored_packages(list));
    ASSERT_EQ(profiles.metadata.size(), 1u);
    EXPECT_TRUE(same_path(profiles.metadata.front(), package / L"metadata" / L"Test.winmd"));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_release_below_the_minimum_names_the_profile_and_both_versions) {
    auto const directory = scratch(L"release_below");
    auto const list = write_list(
        directory, {listed("Microsoft.WindowsAppSDK", "2.5.4-experimental", directory)});
    auto const profile = directory / L"p.winxl-profile";
    write(profile, R"({"minimumWindowsAppSdk": "2.6.0"})");
    auto const restored = load_restored_packages(list);

    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({profile}, restored); }),
                     {utf8(profile), "Microsoft.WindowsAppSDK", "2.6.0", "2.5.4-experimental",
                      utf8(list)}));

    std::filesystem::remove_all(directory);
}

// The set needs the highest of its minimums: a profile that extends another
// raises the requirement, and one that names a lower release does not lower it.
TEST(profile, the_highest_minimum_of_the_set_is_the_one_checked) {
    auto const directory = scratch(L"highest_minimum");
    auto const base = directory / L"base.winxl-profile";
    write(base, R"({"minimumWindowsAppSdk": "2.5.4-experimental"})");
    auto const lower = directory / L"lower.winxl-profile";
    write(lower, R"({"extends": ["base"], "minimumWindowsAppSdk": "2.5.1"})");
    auto const higher = directory / L"higher.winxl-profile";
    write(higher, R"({"extends": ["base"], "minimumWindowsAppSdk": "2.6.0"})");

    auto const older = load_restored_packages(
        write_list(directory / L"older", {listed("Microsoft.WindowsAppSDK", "2.5.1", directory)}));
    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({lower}, older); }),
                     {utf8(base), "2.5.4-experimental", "2.5.1"}));

    auto const release = load_restored_packages(
        write_list(directory / L"release",
                   {listed("microsoft.windowsappsdk", "2.5.4-experimental", directory)}));
    EXPECT_NO_THROW(resolve_profiles({lower}, release));
    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({higher}, release); }),
                     {utf8(higher), "2.6.0", "2.5.4-experimental"}));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_minimum_release_the_restore_lacks_says_what_to_add) {
    auto const directory = scratch(L"release_missing");
    auto const restored = load_restored_packages(write_list(directory, {}));
    auto const profile = directory / L"p.winxl-profile";
    write(profile, R"({"minimumWindowsAppSdk": "2.5.4-experimental"})");

    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({profile}, restored); }),
                     {utf8(profile), "add a reference to Microsoft.WindowsAppSDK "
                                     "2.5.4-experimental or newer"}));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_package_the_restore_lacks_says_what_to_add) {
    auto const directory = scratch(L"package_missing");
    auto const restored = load_restored_packages(write_list(directory, {}));
    auto const profile = directory / L"p.winxl-profile";
    write(profile, R"({"packages": [{"id": "Test.Missing", "minimumVersion": "1.4.0"}]})");

    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({profile}, restored); }),
                     {utf8(profile), "Test.Missing",
                      "add a reference to Test.Missing 1.4.0 or newer"}));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_package_below_its_minimum_names_both_versions) {
    auto const directory = scratch(L"package_below");
    auto const package = write_package(directory / L"package");
    auto const restored =
        load_restored_packages(write_list(directory, {listed("Test.Package", "1.4.0", package)}));
    auto const profile = directory / L"p.winxl-profile";
    write(profile, R"({"packages": [{"id": "Test.Package", "minimumVersion": "1.5.0"}]})");

    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({profile}, restored); }),
                     {utf8(profile), "Test.Package", "1.5.0", "1.4.0"}));

    std::filesystem::remove_all(directory);
}

// A key that names a version to read stops the load, and the message names the key
// that says what a profile can say instead.
TEST(profile, a_key_that_names_a_version_to_read_names_its_replacement) {
    auto const directory = scratch(L"version_keys");
    auto const release = directory / L"release.winxl-profile";
    write(release, R"({"windowsAppSdk": "2.5.4-experimental"})");
    auto const version = directory / L"version.winxl-profile";
    write(version, R"({"packages": [{"id": "Microsoft.Graphics.Win2D", "version": "1.4.0"}]})");
    auto const overridden = directory / L"override.winxl-profile";
    write(overridden, R"({"packages": [{"id": "Microsoft.WindowsAppSDK.WinUI",
                                        "overrideVersion": "2.3.10-experimental"}]})");

    EXPECT_TRUE(says(stops_with([&] { load_profile(release); }),
                     {utf8(release), "'windowsAppSdk'", "'minimumWindowsAppSdk'"}));
    EXPECT_TRUE(says(stops_with([&] { load_profile(version); }),
                     {utf8(version), "Microsoft.Graphics.Win2D", "'version'", "'minimumVersion'"}));
    EXPECT_TRUE(says(stops_with([&] { load_profile(overridden); }),
                     {utf8(overridden), "Microsoft.WindowsAppSDK.WinUI", "'overrideVersion'",
                      "'minimumVersion'", "'minimumWindowsAppSdk'"}));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_minimum_that_is_not_a_version_is_refused) {
    auto const directory = scratch(L"not_a_version");
    auto const release = directory / L"release.winxl-profile";
    write(release, R"({"minimumWindowsAppSdk": "latest"})");
    auto const package = directory / L"package.winxl-profile";
    write(package,
          R"json({"packages": [{"id": "Test.Package", "minimumVersion": "[1.0,2.0)"}]})json");
    // A key that is there names a version, and an empty one names none.
    auto const empty_release = directory / L"empty_release.winxl-profile";
    write(empty_release, R"({"minimumWindowsAppSdk": ""})");
    auto const empty_package = directory / L"empty_package.winxl-profile";
    write(empty_package, R"({"packages": [{"id": "Test.Package", "minimumVersion": ""}]})");

    EXPECT_TRUE(says(stops_with([&] { load_profile(release); }), {utf8(release), "latest"}));
    EXPECT_TRUE(says(stops_with([&] { load_profile(package); }), {utf8(package), "[1.0,2.0)"}));
    EXPECT_TRUE(says(stops_with([&] { load_profile(empty_release); }),
                     {utf8(empty_release),
                      "'minimumWindowsAppSdk' is \"\", which is not a NuGet version"}));
    EXPECT_TRUE(says(stops_with([&] { load_profile(empty_package); }),
                     {utf8(empty_package),
                      "'minimumVersion' is \"\", which is not a NuGet version"}));

    std::filesystem::remove_all(directory);
}

// A list made by an earlier configure can name a file the Windows SDK no longer
// has; the run names it rather than walking without it.
TEST(profile, a_listed_windows_metadata_file_that_is_gone_is_named) {
    auto const directory = scratch(L"metadata_gone");
    auto const list = write_list(directory, {});
    auto const gone = directory / L"sdk" / L"Windows.Foundation.FoundationContract.winmd";
    std::filesystem::remove(gone);
    auto const profile = directory / L"p.winxl-profile";
    write(profile, R"({"windowsMetadata": true})");
    auto const restored = load_restored_packages(list);

    // The file as the list spells it, which is how the message names it.
    EXPECT_TRUE(says(stops_with([&] { resolve_profiles({profile}, restored); }),
                     {utf8(list), "10.0.26100.0", json_path(gone)}));

    std::filesystem::remove_all(directory);
}

TEST(profile, a_list_that_is_not_there_says_how_to_get_it) {
    auto const directory = scratch(L"no_list");
    auto const list = directory / L"restored-packages.json";

    EXPECT_TRUE(says(stops_with([&] { load_restored_packages(list); }),
                     {utf8(list), "<build>/wxl.packages/restored-packages.json", "configure"}));

    std::filesystem::remove_all(directory);
}

// The version of the Windows SDK names a folder of its reference, and is held to
// the form the configure writes: four numbers separated by dots.
TEST(profile, a_list_with_a_windows_sdk_version_of_another_form_is_refused) {
    auto const directory = scratch(L"sdk_version");
    EXPECT_NO_THROW(load_restored_packages(write_list(directory / L"good", {}, "10.0.26100.0")));
    int at = 0;
    for (std::string_view const version : {"", "10.0.26100", "10.0.26100.0.1", "10..26100.0",
                                           "v10.0.26100.0", "10.0.26100.0 ", "..."}) {
        auto const list = write_list(directory / std::to_wstring(at++), {}, version);
        EXPECT_TRUE(says(stops_with([&] { load_restored_packages(list); }),
                         {utf8(list), "'windowsSdk.version'"}))
            << version;
    }

    std::filesystem::remove_all(directory);
}

TEST(profile, a_list_that_names_a_package_twice_is_refused) {
    auto const directory = scratch(L"listed_twice");
    auto const list = write_list(directory, {listed("Test.Package", "1.0.0", directory),
                                             listed("test.package", "1.1.0", directory)});

    EXPECT_TRUE(says(stops_with([&] { load_restored_packages(list); }),
                     {utf8(list), "test.package", "twice"}));

    std::filesystem::remove_all(directory);
}

// A reference in Cyrillic -- "базовый", "папка/базовый.winxl-profile", spelled out
// so that the source's own encoding does not matter -- names the file it spells,
// beside the profile that makes it; a bare name is a .winxl-profile.
TEST(profile, a_reference_in_cyrillic_names_the_file_it_spells) {
    std::filesystem::path const referrer{L"C:\\profiles\\full.winxl-profile"};

    constexpr std::string_view basic = "\xD0\xB1\xD0\xB0\xD0\xB7\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9";
    EXPECT_TRUE(
        same_path(resolve_reference(basic, referrer),
                  L"C:\\profiles\\\u0431\u0430\u0437\u043E\u0432\u044B\u0439.winxl-profile"));
    constexpr std::string_view in_folder =
        "\xD0\xBF\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0/"
        "\xD0\xB1\xD0\xB0\xD0\xB7\xD0\xBE\xD0\xB2\xD1\x8B\xD0\xB9.winxl-profile";
    EXPECT_TRUE(same_path(resolve_reference(in_folder, referrer),
                          L"C:\\profiles\\\u043F\u0430\u043F\u043A\u0430\\"
                          L"\u0431\u0430\u0437\u043E\u0432\u044B\u0439.winxl-profile"));
}

// A profile that names itself nothing is named after its file, and a file named in
// Cyrillic -- "профиль", spelled out so that the source's own encoding does not
// matter -- gives that name in UTF-8, whatever the ANSI code page of the machine.
TEST(profile, takes_its_name_from_a_file_named_in_cyrillic) {
    auto const directory = scratch(L"name_from_file");
    auto const file = directory / L"\u043F\u0440\u043E\u0444\u0438\u043B\u044C.winxl-profile";
    std::ofstream{file} << "{}";

    EXPECT_EQ(load_profile(file).name, "\xD0\xBF\xD1\x80\xD0\xBE\xD1\x84\xD0\xB8\xD0\xBB\xD1\x8C");

    std::filesystem::remove_all(directory);
}
