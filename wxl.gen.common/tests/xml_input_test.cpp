#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "xml_input.h"

TEST(xml_input, documentation_members_by_id) {
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

    auto const members = documentation_members(file);
    std::filesystem::remove(file);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_EQ(members.at("T:Sample.Widget").summary, "A widget on two lines.");

    auto const& method = members.at("M:Sample.Widget.Resize(System.Double,System.Boolean@)");
    EXPECT_EQ(method.summary, "Resizes it.");
    ASSERT_EQ(method.params.size(), 2u);
    EXPECT_EQ(method.params[0].first, "width");
    EXPECT_EQ(method.params[1].second, "Whether it changed.");
    EXPECT_EQ(method.returns, "Nothing & nobody.");
    EXPECT_EQ(method.deprecated, "Use Scale.");
}
