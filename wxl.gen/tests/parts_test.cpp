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
// still written and recorded, since the build compiles every part it asked for
// and the run removes whatever it wrote and did not record.
TEST_F(parts, are_all_written_whatever_the_count_of_classes) {
    auto const one = write(groups({2, 1}), 1);
    EXPECT_EQ(occurrences(one[0], "// body "), 3u);
    EXPECT_EQ(written_, (std::vector<std::filesystem::path>{std::filesystem::path{L"parts"} /
                                                            L"wrapper-00.cpp"}));

    auto const four = write(groups({2}), 4);
    EXPECT_EQ(occurrences(four[0], "// body "), 1u);
    EXPECT_EQ(occurrences(four[1], "// body "), 1u);
    for (std::size_t index : {2u, 3u}) {
        EXPECT_TRUE(four[index].starts_with(gen::banner));
        EXPECT_EQ(four[index].find("namespace wxl"), std::string::npos);
        EXPECT_EQ(four[index].find("#include"), std::string::npos);
    }
    ASSERT_EQ(written_.size(), 4u);
    for (std::size_t index = 0; index != 4; ++index) {
        EXPECT_EQ(written_[index],
                  std::filesystem::path{L"parts"} / std::format(L"wrapper-{:02}.cpp", index));
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

namespace {

// A class of the group the placement test writes: activated, so that it has a
// runtime class name, unless `activated` says otherwise.
gen::class_info wrapped(std::string_view name, bool activated = true) {
    gen::class_info info;
    info.name = std::string{name};
    info.metadata_name = std::format("N.{}", name);
    info.winrt_name = std::format("winrt::N::{}", name);
    info.base_name = "Object";
    info.construction =
        activated ? gen::construction_t::PublicActivation : gen::construction_t::None;
    return info;
}

}  // namespace

// The bodies of one group, A, B and C, written by the class writer and spread
// over two parts, so that the group spans both: each class's runtime class name
// comes ahead of its bodies and its try_as and is after them, and the explicit
// instantiation the group defines comes after every class of the group -- in
// the part of its last class, after that class's bodies.
TEST_F(parts, place_the_bodies_of_a_class_together_and_the_groups_own_after_them) {
    model m;
    gen::class_group group;
    group.name = "N";
    group.classes = {wrapped("A"), wrapped("B"), wrapped("C", false)};
    group.collections_defined = {"Collection<A>"};
    m.class_groups.push_back(std::move(group));
    m.group_of["N"] = "N";

    gen::emitted em{dir_};
    std::vector<gen::body_file> bodies;
    gen::write_classes(output{dir_, 2}, m, em, bodies);
    ASSERT_EQ(bodies.size(), 1u);
    ASSERT_EQ(bodies[0].classes.size(), 3u);
    gen::write_parts(output{dir_, 2}, bodies, em);

    // A and B in the first part, C in the second.
    auto const first = text_of(0);
    auto const second = text_of(1);
    for (std::string_view const name : {"A", "B"}) {
        auto const named = first.find(std::format("struct runtime_class_name_of<{}>", name));
        auto const constructor = first.find(std::format("\n{0}::{0}(Impl* impl)", name));
        auto const try_as =
            first.find(std::format("template {0} Object::try_as<{0}>() const;", name));
        auto const is = first.find(std::format("template bool Object::is<{}>() const;", name));
        ASSERT_NE(named, std::string::npos) << name;
        EXPECT_LT(named, constructor) << name;
        EXPECT_LT(constructor, try_as) << name;
        EXPECT_LT(try_as, is) << name;
        EXPECT_EQ(occurrences(first, std::format("Object::try_as<{0}>", name)), 1u) << name;
        EXPECT_EQ(second.find(std::format("Object::try_as<{0}>", name)), std::string::npos);
    }
    // A's bodies whole before B's begin.
    EXPECT_LT(first.find("template bool Object::is<A>() const;"),
              first.find("struct runtime_class_name_of<B>"));

    EXPECT_EQ(second.find("runtime_class_name_of"), std::string::npos);
    EXPECT_EQ(occurrences(second, "template C Object::try_as<C>() const;"), 1u);
    EXPECT_EQ(first.find("template class Collection<A>;"), std::string::npos);
    EXPECT_EQ(occurrences(second, "template class Collection<A>;"), 1u);
    EXPECT_LT(second.find("template bool Object::is<C>() const;"),
              second.find("template class Collection<A>;"));
}
