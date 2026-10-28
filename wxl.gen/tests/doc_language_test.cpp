#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "documentation.h"

#include "wxl.gen.h"

// Last of the headers: it imports std and wxl.core, and MSVC takes no standard
// header after that.
#include "test_directory.h"

import std;
import wxl.core;

// doc_language.txt: written by a run that documents the wrappers, absent after
// one that does not. The documentation site takes its language from the file
// wherever it finds one, so a file an earlier run left behind would build the
// site in a language nobody asked for this time.

namespace {

// u8, so that the path reads the directory as UTF-8 and not in the ANSI code page.
std::filesystem::path const profiles_dir{u8"" WXL_GEN_PROFILES_DIR};

// The list the configure of this build wrote: the packages its restore resolved
// and the Windows SDK of its projection.
std::filesystem::path const restored_packages_file{u8"" WXL_RESTORED_PACKAGES};

// The generator's output for one test, in a directory of the test's own,
// emptied before and removed after.
class doc_language : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::path{wxl::core::test_directory().native()};
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    output out() const { return {dir_}; }
    std::filesystem::path file() const { return dir_ / "doc_language.txt"; }

    static model documented_in(documentation_language const* language) {
        model m;
        m.doc_language = language;
        return m;
    }

    std::filesystem::path dir_;
};

std::string text_of(std::filesystem::path const& path) {
    std::ifstream in{path};
    return {std::istreambuf_iterator<char>{in}, {}};
}

}  // namespace

TEST_F(doc_language, names_the_code_and_doxygens_language_under_the_banner) {
    documentation_language const* const traditional = find_documentation_language("zh-hant");
    ASSERT_NE(traditional, nullptr);

    gen::emitted em{dir_};
    gen::write_doc_language(out(), documented_in(traditional), em);

    EXPECT_EQ(text_of(file()), std::string{gen::banner} + "zh-hant Chinese-Traditional\n");
    ASSERT_EQ(em.files().size(), 1u);
    EXPECT_EQ(em.files().front().native(), L"doc_language.txt");
}

// A whole run, as main() makes it: the language reaches the model and the
// writer from the run's own argument, and the next run without documentation
// takes the file away.
TEST_F(doc_language, follows_the_run_that_writes_the_wrappers) {
    auto const profiles = resolve_profiles({profiles_dir / "base.winxl-profile"},
                                           load_restored_packages(restored_packages_file));
    auto const types = load_type_map(profiles_dir / "types.json");

    documentation_language const* const russian = find_documentation_language("ru");
    ASSERT_NE(russian, nullptr);

    run(profiles, types, {}, out(), russian);
    EXPECT_EQ(text_of(file()), std::string{gen::banner} + "ru Russian\n");

    run(profiles, types, {}, out(), nullptr);
    EXPECT_FALSE(std::filesystem::exists(file()));
}
