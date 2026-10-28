#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "wxl.gen.h"

// Last of the headers: it imports std and wxl.core, and MSVC takes no standard
// header after that.
#include "test_directory.h"

import std;
import wxl.core;

// parts/wrapper-NN.cpp: the out-of-line bodies the writers hand over, spread over
// the number of translation units the build asks for. Written from made-up
// groups, so that what is checked is the spreading and nothing of the metadata.

namespace {

// The generator's output for one test, in a directory of the test's own,
// emptied before and removed after.
class parts : public ::testing::Test
{
protected:
    void SetUp() override {
        dir_ = std::filesystem::path{wxl::core::test_directory().native()};
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_ / L"parts");
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    // `counts` classes in each group, group G's class C carrying the body
    // "// body GgCc", and each group including its own private header and a
    // header of the library they all share.
    static std::vector<gen::body_file> groups(std::vector<std::size_t> const& counts) {
        std::vector<gen::body_file> bodies;
        for (std::size_t group = 0; group != counts.size(); ++group) {
            gen::body_file file{
                .name = std::format("Group{}", group),
                .includes = {std::format("Group{}.impl.h", group), "../impl/activator.h"},
                .classes = {}};
            for (std::size_t at = 0; at != counts[group]; ++at) {
                file.classes.push_back(std::format("\n// body G{}C{}\n", group, at));
            }
            bodies.push_back(std::move(file));
        }
        return bodies;
    }

    std::filesystem::path part(std::size_t index) const {
        return dir_ / L"parts" / std::format(L"wrapper-{:02}.cpp", index);
    }

    std::string text_of(std::size_t index) const {
        std::ifstream in{part(index)};
        return {std::istreambuf_iterator<char>{in}, {}};
    }

    std::vector<std::string> write(std::vector<gen::body_file> const& bodies, std::size_t count) {
        gen::emitted em{dir_};
        gen::write_parts(output{dir_, count}, bodies, em);
        written_ = em.files();
        std::vector<std::string> texts;
        for (std::size_t index = 0; index != count; ++index) {
            texts.push_back(text_of(index));
        }
        return texts;
    }

    std::filesystem::path dir_;
    std::vector<std::filesystem::path> written_;
};

std::size_t occurrences(std::string_view text, std::string_view what) {
    std::size_t count = 0;
    for (auto at = text.find(what); at != std::string_view::npos; at = text.find(what, at + 1)) {
        ++count;
    }
    return count;
}

}  // namespace

// Eleven classes in three groups over three parts: every body is in exactly one
// part, the parts hold four, four and three classes, and each part keeps the
// order the classes came in.
TEST_F(parts, hold_every_class_body_once_and_balance_within_one_class) {
    std::vector<std::size_t> const counts{5, 2, 4};
    auto const texts = write(groups(counts), 3);

    std::vector<std::size_t> held(texts.size());
    for (std::size_t group = 0; group != counts.size(); ++group) {
        for (std::size_t at = 0; at != counts[group]; ++at) {
            auto const body = std::format("// body G{}C{}\n", group, at);
            std::size_t total = 0;
            for (std::size_t index = 0; index != texts.size(); ++index) {
                auto const here = occurrences(texts[index], body);
                total += here;
                held[index] += here;
            }
            EXPECT_EQ(total, 1u) << body;
        }
    }
    EXPECT_EQ(held, (std::vector<std::size_t>{4, 4, 3}));

    // The second part: the last class of group 0, group 1, the first class of
    // group 2, in that order.
    auto const& second = texts[1];
    EXPECT_LT(second.find("// body G0C4"), second.find("// body G1C0"));
    EXPECT_LT(second.find("// body G1C0"), second.find("// body G1C1"));
    EXPECT_LT(second.find("// body G1C1"), second.find("// body G2C0"));
    EXPECT_EQ(occurrences(texts[0], "// body G0C"), 4u);

    ASSERT_EQ(written_.size(), 3u);
    EXPECT_EQ(written_[2], std::filesystem::path{L"parts"} / L"wrapper-02.cpp");
    EXPECT_FALSE(std::filesystem::exists(part(3)));
    for (auto&& text : texts) {
        EXPECT_TRUE(text.starts_with(gen::banner));
    }
}

// A part includes what its groups included, in their order and each header once,
// a file of the run from parts/ with "../" and a header of the library through its
// include directory; a group with no class in the part adds nothing.
TEST_F(parts, include_what_their_groups_included_once) {
    auto const texts = write(groups({1, 1, 1}), 2);

    EXPECT_EQ(texts[0].substr(gen::banner.size(),
                              texts[0].find("\nnamespace wxl {") - gen::banner.size()),
              "#include \"../Group0.impl.h\"\n"
              "#include <impl/activator.h>\n"
              "#include \"../Group1.impl.h\"\n");
    EXPECT_EQ(occurrences(texts[1], "#include"), 2u);
    EXPECT_NE(texts[1].find("#include \"../Group2.impl.h\"\n#include <impl/activator.h>\n"),
              std::string::npos);

    // Each group's bodies under its name.
    EXPECT_NE(texts[0].find("\n// Group0\n\n// body G0C0"), std::string::npos);
    EXPECT_NE(texts[0].find("\n// Group1\n\n// body G1C0"), std::string::npos);
}

// One part takes everything; more parts than classes leave the last ones empty,
// still written, since the build compiles every part it asked for.
TEST_F(parts, are_all_written_whatever_the_count_of_classes) {
    auto const one = write(groups({2, 1}), 1);
    EXPECT_EQ(occurrences(one[0], "// body "), 3u);

    auto const four = write(groups({2}), 4);
    EXPECT_EQ(occurrences(four[0], "// body "), 1u);
    EXPECT_EQ(occurrences(four[1], "// body "), 1u);
    for (std::size_t index : {2u, 3u}) {
        EXPECT_TRUE(four[index].starts_with(gen::banner));
        EXPECT_EQ(four[index].find("namespace wxl"), std::string::npos);
        EXPECT_EQ(four[index].find("#include"), std::string::npos);
    }
}

// The count is the build's, from 1 to max_parts: the names have two digits.
TEST_F(parts, refuse_a_count_out_of_range) {
    gen::emitted em{dir_};
    EXPECT_THROW(gen::write_parts(output{dir_, 0}, groups({1}), em), std::invalid_argument);
    EXPECT_THROW(gen::write_parts(output{dir_, max_parts + 1}, groups({1}), em),
                 std::invalid_argument);
    EXPECT_TRUE(em.files().empty());
}
