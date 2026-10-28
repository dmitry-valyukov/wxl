#include <gtest/gtest.h>

#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "crawl.h"

import wxl.core;

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// The list the configure of this build wrote: the packages its restore resolved
// and the Windows SDK of its projection.
std::filesystem::path const restored_packages_file{u8"" WXL_RESTORED_PACKAGES};

std::string utf8(std::filesystem::path const& path) {
    return std::string{wxl::core::unicode::to_utf8(path).chars()};
}

// What the walk stops with; a failure of the test when it does not stop.
std::string walk_error(profile_set const& profiles, type_map const& types) {
    std::vector<std::string> files;
    for (auto&& file : profiles.metadata) {
        files.push_back(utf8(file));
    }
    winmd::reader::cache const db{files};
    try {
        crawl(profiles, types, db);
    } catch (std::runtime_error const& error) {
        return error.what();
    }
    ADD_FAILURE() << "a type the metadata does not declare has to stop the walk";
    return {};
}

// A profile that reads Microsoft.UI.Xaml.winmd alone and lists `types`, written
// into a directory of the test's own.
std::filesystem::path write_profile(std::wstring_view test, std::string_view types) {
    auto const directory = std::filesystem::temp_directory_path() / L"wxl.gen.common.crawl" / test;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    auto const profile = directory / L"p.winxl-profile";
    std::ofstream{profile} << R"({"packages": [{"id": "Microsoft.WindowsAppSDK.WinUI",
                                                "metadata": ["Microsoft.UI.Xaml.winmd"]}],
                                 "types": )"
                           << types << "}";
    return profile;
}

template <typename Types>
std::vector<std::string> names_of(Types const& types) {
    std::vector<std::string> names;
    for (auto&& type : types) {
        names.push_back(std::format("{}.{}", type.TypeNamespace(), type.TypeName()));
    }
    return names;
}

}  // namespace

// winmd orders types by the addresses of their file's tables first, and the
// heap lays a cache's files out in a different order every time. Caches of the
// same files, all alive at once, stand for separate runs: their walks agree
// only if nothing in the walk orders types that way.
TEST(crawl, order_depends_on_the_metadata_alone) {
    auto const types = load_type_map(profiles_dir / "types.json");
    auto const profiles = resolve_profiles({profiles_dir / "rich.winxl-profile"},
                                           load_restored_packages(restored_packages_file));

    std::vector<std::string> files;
    for (auto&& file : profiles.metadata) {
        files.push_back(std::string{wxl::core::unicode::to_utf8(file).chars()});
    }
    std::deque<winmd::reader::cache> caches;
    auto const walk = [&] { return crawl(profiles, types, caches.emplace_back(files)); };

    auto const first = walk();
    for (int run = 1; run < 8; ++run) {
        auto const next = walk();
        ASSERT_EQ(names_of(first.ordered), names_of(next.ordered)) << "walk " << run;
        ASSERT_EQ(names_of(first.boundary), names_of(next.boundary)) << "walk " << run;
    }
}

// A type a profile lists and the metadata does not declare stops the walk -- as
// a profile that projects WrapPanel stops on a release whose WinUI has none: the
// error names the type, the profile that lists it and the Windows App SDK
// release of the restore.
TEST(crawl, a_type_the_metadata_does_not_declare_stops_the_walk) {
    auto const profile = write_profile(L"undeclared_type", R"({
        "Microsoft.UI.Xaml.Controls.Button": {},
        "Microsoft.UI.Xaml.Controls.NoSuchPanel": {},
        "Windows.UI.NoSuchColor": {}
    })");
    auto const restored = load_restored_packages(restored_packages_file);
    auto const message = walk_error(resolve_profiles({profile}, restored),
                                    load_type_map(profiles_dir / "types.json"));

    restored_package const* const release = restored.find("Microsoft.WindowsAppSDK");
    ASSERT_NE(release, nullptr);
    // A type of a package is the restore's to declare; one of Windows is the
    // Windows SDK's, which this profile does not read.
    std::vector<std::string> const parts{
        "Microsoft.WindowsAppSDK " + release->version, utf8(restored_packages_file),
        "Microsoft.UI.Xaml.Controls.NoSuchPanel, listed by " + utf8(profile) +
            ": no package the profiles name declares it",
        "Windows.UI.NoSuchColor, listed by " + utf8(profile) + ": a Windows type",
        "\"windowsMetadata\": true"};
    for (auto&& part : parts) {
        EXPECT_NE(message.find(part), std::string::npos) << part << " in: " << message;
    }
    EXPECT_EQ(message.find("Controls.Button"), std::string::npos) << message;

    std::filesystem::remove_all(profile.parent_path());
}

// An implicit root of the type map that the metadata does not declare stops the
// walk too, named as the type map's.
TEST(crawl, a_root_of_the_type_map_the_metadata_does_not_declare_stops_the_walk) {
    auto const profile =
        write_profile(L"undeclared_root", R"({"Microsoft.UI.Xaml.Controls.Button": {}})");
    auto types = load_type_map(profiles_dir / "types.json");
    types.implicit_roots.insert("Microsoft.UI.Xaml.NoSuchRoot");
    auto const message = walk_error(
        resolve_profiles({profile}, load_restored_packages(restored_packages_file)), types);

    EXPECT_NE(message.find("Microsoft.UI.Xaml.NoSuchRoot, a root of every run in the type map "
                           "(types.json)"),
              std::string::npos)
        << message;
    EXPECT_EQ(message.find("Controls.Button"), std::string::npos) << message;

    std::filesystem::remove_all(profile.parent_path());
}

// A call a profile replaces comes out of the merged profiles under the type and the
// method it was written for, with the function and the header that declares it.
TEST(profile, a_replaced_call_reaches_the_merged_profiles) {
    auto const profiles = resolve_profiles({profiles_dir / "full.winxl-profile"},
                                           load_restored_packages(restored_packages_file));

    auto const type = profiles.replaced_calls.find("Windows.Storage.Streams.RandomAccessStreamReference");
    ASSERT_NE(type, profiles.replaced_calls.end());
    auto const call = type->second.find("CreateFromUri");
    ASSERT_NE(call, type->second.end());
    EXPECT_EQ(call->second.function, "impl::stream_reference_from_uri");
    EXPECT_EQ(call->second.include, "../impl/winrt_helpers.h");
    EXPECT_EQ(type->second.size(), 1u);
}
