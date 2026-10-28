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
    auto const directory =
        std::filesystem::temp_directory_path() / L"wxl.gen.common.crawl" / L"undeclared_type";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    auto const profile = directory / L"p.winxl-profile";
    std::ofstream{profile} << R"({
        "packages": [{"id": "Microsoft.WindowsAppSDK.WinUI", "metadata": ["Microsoft.UI.Xaml.winmd"]}],
        "types": {"Microsoft.UI.Xaml.Controls.Button": {}, "Microsoft.UI.Xaml.Controls.NoSuchPanel": {}}
    })";

    auto const restored = load_restored_packages(restored_packages_file);
    auto const profiles = resolve_profiles({profile}, restored);
    auto const types = load_type_map(profiles_dir / "types.json");
    std::vector<std::string> files;
    for (auto&& file : profiles.metadata) {
        files.push_back(utf8(file));
    }
    winmd::reader::cache const db{files};

    std::string message;
    try {
        crawl(profiles, types, db);
        ADD_FAILURE() << "a type the metadata does not declare has to stop the walk";
    } catch (std::runtime_error const& error) {
        message = error.what();
    }
    restored_package const* const release = restored.find("Microsoft.WindowsAppSDK");
    ASSERT_NE(release, nullptr);
    std::vector<std::string> const parts{"Microsoft.UI.Xaml.Controls.NoSuchPanel, listed by ",
                                         utf8(profile), "Microsoft.WindowsAppSDK " + release->version,
                                         utf8(restored_packages_file)};
    for (auto&& part : parts) {
        EXPECT_NE(message.find(part), std::string::npos) << part << " in: " << message;
    }
    EXPECT_EQ(message.find("Controls.Button"), std::string::npos) << message;

    std::filesystem::remove_all(directory);
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
