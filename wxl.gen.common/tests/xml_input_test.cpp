#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

#include "xml_input.h"

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
