#include <gtest/gtest.h>

#include <deque>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "crawl.h"

namespace {

std::filesystem::path const profiles_dir{WXL_GEN_PROFILES_DIR};

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
    auto const profiles = resolve_profiles({profiles_dir / "rich.json"}, default_nuget_root());

    std::vector<std::string> files;
    for (auto&& file : profiles.metadata) {
        files.push_back(file.string());
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

// A call a profile replaces comes out of the merged profiles under the type and the
// method it was written for, with the function and the header that declares it.
TEST(profile, a_replaced_call_reaches_the_merged_profiles) {
    auto const profiles = resolve_profiles({profiles_dir / "full.json"}, default_nuget_root());

    auto const type = profiles.replaced_calls.find("Windows.Storage.Streams.RandomAccessStreamReference");
    ASSERT_NE(type, profiles.replaced_calls.end());
    auto const call = type->second.find("CreateFromUri");
    ASSERT_NE(call, type->second.end());
    EXPECT_EQ(call->second.function, "impl::stream_reference_from_uri");
    EXPECT_EQ(call->second.include, "../impl/file_stream_reference.h");
    EXPECT_EQ(type->second.size(), 1u);
}
