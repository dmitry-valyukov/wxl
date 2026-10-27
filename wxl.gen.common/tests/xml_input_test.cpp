#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "xml_input.h"

import wxl.core;

namespace {

// A path as the errors spell it.
std::string utf8(std::filesystem::path const& path) {
    return std::string {wxl::core::unicode::to_utf8(path).chars()};
}

}  // namespace

TEST(xml_input, documentation_file_finds_members_by_id) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.documentation.xml";
    std::ofstream {file} << R"xml(<?xml version="1.0" encoding="utf-8"?>
<doc>
  <assembly><name>Sample</name></assembly>
  <members>
    <member name="T:Sample.Widget">
      <summary>A widget
        on two lines.</summary>
    </member>
    <member name="M:Sample.Widget.Resize(System.Double,System.Boolean@)">
      <summary>Resizes it.</summary>
      <param name="width">The new width.</param>
      <param name="changed">Whether it changed.</param>
      <returns>Nothing &amp; nobody.</returns>
      <deprecated type="deprecate">Use Scale.</deprecated>
    </member>
  </members>
</doc>
)xml";

    documentation_file members {file};
    std::filesystem::remove(file);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_FALSE(members.find("T:Sample.Gadget"));
    EXPECT_EQ(members.find("T:Sample.Widget")->summary, "A widget on two lines.");

    auto const method = members.find("M:Sample.Widget.Resize(System.Double,System.Boolean@)");
    ASSERT_TRUE(method);
    EXPECT_EQ(method->summary, "Resizes it.");
    ASSERT_EQ(method->params.size(), 2u);
    EXPECT_EQ(method->params[0].first, "width");
    EXPECT_EQ(method->params[1].second, "Whether it changed.");
    EXPECT_EQ(method->returns, "Nothing & nobody.");
    EXPECT_EQ(method->deprecated, "Use Scale.");
}

TEST(xml_input, documentation_file_parses_the_rest_by_slices) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.slices.xml";
    {
        std::ofstream out {file};
        out << "<doc><members>";
        for (int i = 0; i < 100; ++i) {
            out << "<member name=\"T:N.T" << i << "\"><summary>Type " << i << "</summary></member>";
        }
        out << "<member name=\"T:N.Empty\"/></members></doc>";
    }
    documentation_file members {file};
    std::filesystem::remove(file);
    ASSERT_EQ(members.size(), 101u);

    // A deadline already past still takes one member per call.
    std::size_t calls = 0;
    while (members.parse_some(std::chrono::steady_clock::time_point {})) {
        ++calls;
    }
    EXPECT_EQ(calls, 100u);
    EXPECT_EQ(members.find("T:N.T42")->summary, "Type 42");
    EXPECT_EQ(members.find("T:N.Empty")->summary, "");
}

TEST(xml_input, documentation_file_passes_over_what_find_parsed) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.eager.xml";
    {
        std::ofstream out {file};
        out << "<doc><members>";
        for (int i = 0; i < 100; ++i) {
            out << "<member name=\"T:N.T" << i << "\"><summary>Type " << i << "</summary></member>";
        }
        out << "</members></doc>";
    }
    documentation_file members {file};
    std::filesystem::remove(file);

    // Every tenth asked for first, the last among them: the slices parse only
    // the other ninety, one each, and the last call has nothing left to walk.
    for (int i = 9; i < 100; i += 10) {
        EXPECT_EQ(members.find(std::format("T:N.T{}", i))->summary, std::format("Type {}", i));
    }
    std::size_t calls = 1;
    while (members.parse_some(std::chrono::steady_clock::time_point {})) {
        ++calls;
    }
    EXPECT_EQ(calls, 90u);
}

// A member that does not parse is an error naming the file and the member, and
// it stays one: asked for again, it throws again.
TEST(xml_input, documentation_file_names_a_member_that_does_not_parse) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.broken.xml";
    std::ofstream {file} << "<doc><members><member name=\"T:N.Broken\"><summary>Open</member>"
                            "<member name=\"T:N.Fine\"><summary>Fine.</summary></member></members></doc>";
    documentation_file members {file};

    for (int ask = 0; ask < 2; ++ask) {
        try {
            members.find("T:N.Broken");
            ADD_FAILURE() << "no error";
        } catch (std::runtime_error const& error) {
            std::string_view const what = error.what();
            EXPECT_TRUE(what.contains(utf8(file))) << what;
            EXPECT_TRUE(what.contains("T:N.Broken")) << what;
        }
    }
    EXPECT_EQ(members.find("T:N.Fine")->summary, "Fine.");
    std::filesystem::remove(file);
}

namespace {

// What opening `text` as a documentation file throws. Opening it at all is a
// failure: a broken member list must not pass for a shorter one.
std::string open_error(std::filesystem::path const& file, std::string_view text) {
    std::ofstream {file} << text;
    std::string what;
    try {
        documentation_file const members {file};
        ADD_FAILURE() << "opened, with " << members.size() << " members";
    } catch (std::runtime_error const& error) {
        what = error.what();
    }
    std::filesystem::remove(file);
    return what;
}

// The error names the file, the offset the last member in `text` starts at,
// and the reason.
void expect_broken_member(std::wstring_view name, std::string_view text, std::string_view reason) {
    auto const file = std::filesystem::temp_directory_path() / name;
    std::string const what = open_error(file, text);
    EXPECT_TRUE(what.contains(utf8(file))) << what;
    auto const offset = std::format("documentation member at offset {} ", text.rfind("<member"));
    EXPECT_TRUE(what.contains(offset)) << what;
    EXPECT_TRUE(what.contains(reason)) << what;
}

// "справка", spelled out so that the source's own encoding does not matter.
constexpr wchar_t cyrillic_name[] =
    L"wxl.gen.common.\u0441\u043F\u0440\u0430\u0432\u043A\u0430.xml";
constexpr std::string_view cyrillic_name_utf8 =
    "wxl.gen.common.\xD1\x81\xD0\xBF\xD1\x80\xD0\xB0\xD0\xB2\xD0\xBA\xD0\xB0.xml";

}  // namespace

TEST(xml_input, documentation_file_fails_on_a_start_tag_truncated) {
    expect_broken_member(L"wxl.gen.common.cut_tag.xml",
                         "<doc><members><member name=\"T:N.Fine\"><summary>Fine.</summary></member>"
                         "<member name=\"T:N.Cut",
                         "is truncated: no '>' closes its start tag");
}

TEST(xml_input, documentation_file_fails_on_a_member_truncated) {
    expect_broken_member(L"wxl.gen.common.cut_member.xml",
                         "<doc><members><member name=\"T:N.Fine\"><summary>Fine.</summary></member>"
                         "<member name=\"T:N.Cut\"><summary>Cut",
                         "is truncated: no </member> closes it");
}

TEST(xml_input, documentation_file_fails_on_a_name_left_open) {
    expect_broken_member(L"wxl.gen.common.open_name.xml",
                         "<doc><members><member name=\"T:N.Fine\"><summary>Fine.</summary></member>"
                         "<member name=\"T:N.Open><summary>Open.</summary></member>"
                         "</members></doc>",
                         "has no name: its start tag holds no quoted name attribute");
}

TEST(xml_input, documentation_file_fails_on_a_member_without_a_name) {
    expect_broken_member(L"wxl.gen.common.no_name.xml",
                         "<doc><members><member name=\"T:N.Fine\"><summary>Fine.</summary></member>"
                         "<member kind=\"T\"><summary>Nameless.</summary></member></members></doc>",
                         "has no name: its start tag holds no quoted name attribute");
}

// <members> and a <member> with no attributes at all are passed over, not
// taken for broken members.
TEST(xml_input, documentation_file_passes_over_a_member_without_attributes) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.bare.xml";
    std::ofstream {file} << "<doc><members><member><summary>Bare.</summary></member>"
                            "<member name=\"T:N.Named\"><summary>Named.</summary></member>"
                            "</members></doc>";
    documentation_file members {file};
    std::filesystem::remove(file);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_EQ(members.find("T:N.Named")->summary, "Named.");
}

// The file is named in UTF-8, whatever the code page: a Cyrillic name comes out
// as written -- not garbled, and not as a conversion error that loses this one.
TEST(xml_input, documentation_file_names_a_truncated_file_in_utf8) {
    auto const file = std::filesystem::temp_directory_path() / cyrillic_name;
    std::string const what =
        open_error(file, "<doc><members><member name=\"T:N.Cut\"><summary>Cut");
    EXPECT_TRUE(what.contains(cyrillic_name_utf8)) << what;
    EXPECT_TRUE(what.contains("is truncated")) << what;
}

TEST(xml_input, documentation_file_names_a_broken_member_in_utf8) {
    auto const file = std::filesystem::temp_directory_path() / cyrillic_name;
    std::ofstream {file} << "<doc><members><member name=\"T:N.Broken\"><summary>Open</member>"
                            "</members></doc>";
    documentation_file members {file};
    std::filesystem::remove(file);

    try {
        members.find("T:N.Broken");
        ADD_FAILURE() << "no error";
    } catch (std::runtime_error const& error) {
        std::string_view const what = error.what();
        EXPECT_TRUE(what.contains(cyrillic_name_utf8)) << what;
        EXPECT_TRUE(what.contains("T:N.Broken")) << what;
    }
}
