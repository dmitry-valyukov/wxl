#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "wxl.gen.h"

// Last of the headers: it imports std and wxl.core, and MSVC takes no standard
// header after that.
#include "test_directory.h"

import std;
import wxl.core;

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// The list the configure of this build wrote: the packages its restore resolved
// and the Windows SDK of its projection.
std::filesystem::path const restored_packages_file{u8"" WXL_RESTORED_PACKAGES};

// The generator's output for one test, in a directory of the test's own,
// emptied before and removed after.
class stale_files : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::path{wxl::core::test_directory().native()};
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    output out() const { return {dir_, 2}; }

    std::filesystem::path dir_;
};

}  // namespace

// A run takes away what an earlier run wrote and this one did not, by the names a
// listing of the directory hands out. A stale wrapper named in Cyrillic --
// "устаревший.h", spelled out so that the source's own encoding does not matter --
// goes, and so does a part of an earlier run with more parts; a file of the same
// kind that does not carry the generator's banner stays, and so does what the run
// wrote itself: its two parts and the schema's test beside them.
TEST_F(stale_files, one_named_in_cyrillic_is_removed) {
    auto const stale = dir_ / L"\u0443\u0441\u0442\u0430\u0440\u0435\u0432\u0448\u0438\u0439.h";
    auto const own = dir_ / L"\u0441\u0432\u043E\u0439.h";  // "свой.h"
    auto const stale_part = dir_ / L"parts" / L"wrapper-02.cpp";
    std::ofstream{stale} << gen::banner << "#pragma once\n";
    std::ofstream{own} << "#pragma once\n";
    std::filesystem::create_directories(stale_part.parent_path());
    std::ofstream{stale_part} << gen::banner;

    auto const profiles = resolve_profiles({profiles_dir / "base.winxl-profile"},
                                           load_restored_packages(restored_packages_file));
    auto const types = load_type_map(profiles_dir / "types.json");
    run(profiles, types, {}, out(), nullptr);

    EXPECT_FALSE(std::filesystem::exists(stale));
    EXPECT_FALSE(std::filesystem::exists(stale_part));
    EXPECT_TRUE(std::filesystem::exists(own));
    EXPECT_TRUE(std::filesystem::exists(dir_ / L"parts" / L"wrapper-00.cpp"));
    EXPECT_TRUE(std::filesystem::exists(dir_ / L"parts" / L"wrapper-01.cpp"));
    EXPECT_TRUE(std::filesystem::exists(dir_ / L"parts" / L"schema_surface.cpp"));
}
