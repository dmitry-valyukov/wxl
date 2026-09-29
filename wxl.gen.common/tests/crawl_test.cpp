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
    use_type_map(load_type_map(profiles_dir / "types.json"));
    auto const profiles = resolve_profiles({profiles_dir / "rich.json"}, default_nuget_root());

    std::vector<std::string> files;
    for (auto&& file : profiles.metadata) {
        files.push_back(file.string());
    }
    std::deque<winmd::reader::cache> caches;
    auto const walk = [&] { return crawl(profiles, caches.emplace_back(files)); };

    auto const first = walk();
    for (int run = 1; run < 8; ++run) {
        auto const next = walk();
        ASSERT_EQ(names_of(first.ordered), names_of(next.ordered)) << "walk " << run;
        ASSERT_EQ(names_of(first.boundary), names_of(next.boundary)) << "walk " << run;
    }
}
