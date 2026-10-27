#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "xml_input.h"

import wxl.core;

namespace {

// A path as the errors spell it.
std::string utf8(std::filesystem::path const& path) {
    return std::string {wxl::core::unicode::to_utf8(path).chars()};
}

// What the images of the descriptions in these files are relative to.
constexpr char image_root[] = "https://reference.example/api/";

// A description of text alone, as the reader gives one without markup.
doc_text plain(std::string_view text) {
    return {{.text = std::string {text}}};
}

std::string_view kind_name(doc_run::kind_t kind) {
    switch (kind) {
        case doc_run::kind_t::text: return "text";
        case doc_run::kind_t::code: return "code";
        case doc_run::kind_t::bold: return "bold";
        case doc_run::kind_t::italic: return "italic";
        case doc_run::kind_t::superscript: return "sup";
        case doc_run::kind_t::subscript: return "sub";
        case doc_run::kind_t::line_break: return "br";
        case doc_run::kind_t::image: return "img";
        case doc_run::kind_t::definitions: return "definitions";
        case doc_run::kind_t::definition: return "definition";
        case doc_run::kind_t::table: return "table";
        case doc_run::kind_t::row: return "row";
        case doc_run::kind_t::header_cell: return "th";
        case doc_run::kind_t::cell: return "td";
    }
    return "?";
}

// The pieces of a description spelled out: text in quotes, an element by its
// kind, with its text in parentheses, its address in angle brackets and what it
// holds in square ones.
std::string shape(doc_text const& text) {
    std::string spelled;
    for (auto&& run : text) {
        if (!spelled.empty()) {
            spelled += ' ';
        }
        if (run.kind == doc_run::kind_t::text) {
            spelled += std::format("\"{}\"", run.text);
            continue;
        }
        spelled += kind_name(run.kind);
        if (!run.text.empty()) {
            spelled += std::format("({})", run.text);
        }
        if (!run.url.empty()) {
            spelled += std::format("<{}>", run.url);
        }
        if (!run.runs.empty()) {
            spelled += std::format("[{}]", shape(run.runs));
        }
    }
    return spelled;
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

    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    ASSERT_EQ(members.size(), 2u);
    EXPECT_FALSE(members.find("T:Sample.Gadget"));
    EXPECT_EQ(members.find("T:Sample.Widget")->summary, plain("A widget on two lines."));

    auto const method = members.find("M:Sample.Widget.Resize(System.Double,System.Boolean@)");
    ASSERT_TRUE(method);
    EXPECT_EQ(method->summary, plain("Resizes it."));
    ASSERT_EQ(method->params.size(), 2u);
    EXPECT_EQ(method->params[0].first, "width");
    EXPECT_EQ(method->params[1].second, plain("Whether it changed."));
    EXPECT_EQ(method->returns, plain("Nothing & nobody."));
    EXPECT_EQ(method->deprecated, plain("Use Scale."));
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
    documentation_file members {file, image_root};
    std::filesystem::remove(file);
    ASSERT_EQ(members.size(), 101u);

    // A deadline already past still takes one member per call.
    std::size_t calls = 0;
    while (members.parse_some(std::chrono::steady_clock::time_point {})) {
        ++calls;
    }
    EXPECT_EQ(calls, 100u);
    EXPECT_EQ(members.find("T:N.T42")->summary, plain("Type 42"));
    EXPECT_TRUE(members.find("T:N.Empty")->summary.empty());
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
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    // Every tenth asked for first, the last among them: the slices parse only
    // the other ninety, one each, and the last call has nothing left to walk.
    for (int i = 9; i < 100; i += 10) {
        EXPECT_EQ(members.find(std::format("T:N.T{}", i))->summary, plain(std::format("Type {}", i)));
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
    documentation_file members {file, image_root};

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
    EXPECT_EQ(members.find("T:N.Fine")->summary, plain("Fine."));
    std::filesystem::remove(file);
}

namespace {

// What opening `text` as a documentation file throws. Opening it at all is a
// failure: a broken member list must not pass for a shorter one.
std::string open_error(std::filesystem::path const& file, std::string_view text) {
    std::ofstream {file} << text;
    std::string what;
    try {
        documentation_file const members {file, image_root};
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
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    ASSERT_EQ(members.size(), 1u);
    EXPECT_EQ(members.find("T:N.Named")->summary, plain("Named."));
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
    documentation_file members {file, image_root};
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

// Each kind of markup the descriptions carry, read into its pieces from the
// text the XML decodes to. An image's address is the root the file's images are
// relative to and the path to the page without its "./"; its align goes. Angle
// brackets around a name that is no tag of the markup are text, and so is an
// underscore inside a word, and italic around a superscript, whose end tag
// Doxygen lets no emphasis reach across.
TEST(xml_input, documentation_file_reads_the_markup_of_a_description) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.markup.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="F:N.Symbol.Accept">
  <summary>E8FB &lt;img alt="Accept icon" src="./n.controls/images/e8fb.png" align="top" /&gt;</summary>
</member>
<member name="T:N.CubicEase">
  <summary>f (t) = t&lt;sup&gt;3&lt;/sup&gt; and x&lt;sub&gt;i&lt;/sub&gt;.</summary>
</member>
<member name="T:N.PowerEase"><summary>_t&lt;sup&gt;p&lt;/sup&gt;_ or _t_&lt;sup&gt;p&lt;/sup&gt;</summary></member>
<member name="T:N.Break"><summary>One line&lt;br/&gt;and
  the next.</summary></member>
<member name="P:N.Logo"><returns>Schemes:&lt;termdeflist&gt;
  &lt;termdef&gt;&lt;name&gt;ms-appx:///&lt;/name&gt;A path in the package.&lt;/termdef&gt;
  &lt;termdef&gt;&lt;name&gt;ms-appdata:///local/&lt;/name&gt;A file.&lt;/termdef&gt;
&lt;/termdeflist&gt;</returns></member>
<member name="P:N.Level"><returns>Values: &lt;table&gt;
   &lt;tr&gt;&lt;th&gt;Value&lt;/th&gt;&lt;th&gt;Meaning&lt;/th&gt;&lt;/tr&gt;
   &lt;tr&gt;&lt;td&gt;**0**&lt;/td&gt;&lt;td&gt;No list.&lt;/td&gt;&lt;/tr&gt;
&lt;/table&gt; And so on.</returns></member>
<member name="T:N.Emphasis"><summary>**Bold** and _EaseIn_, but not a_b_c.</summary></member>
<member name="T:N.Angles">
  <summary>IVector&lt;String&gt; and &lt;xxx:yyy&gt;, [,&lt;name&gt;[=&lt;value&gt;].</summary>
</member>
<member name="M:N.Generic`1"><summary>Generic.</summary><typeparam name="T">The type.</typeparam></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(shape(members.find("F:N.Symbol.Accept")->summary),
              R"("E8FB " img(Accept icon)<https://reference.example/api/n.controls/images/e8fb.png>)");
    EXPECT_EQ(shape(members.find("T:N.CubicEase")->summary), R"("f (t) = t" sup["3"] " and x" sub["i"] ".")");
    EXPECT_EQ(shape(members.find("T:N.PowerEase")->summary), R"("_t" sup["p"] "_ or " italic(_)["t"] sup["p"])");
    EXPECT_EQ(shape(members.find("T:N.Break")->summary), R"("One line" br "and the next.")");
    EXPECT_EQ(shape(members.find("P:N.Logo")->returns),
              R"("Schemes:" definitions[definition(ms-appx:///)["A path in the package."] )"
              R"(definition(ms-appdata:///local/)["A file."]])");
    EXPECT_EQ(shape(members.find("P:N.Level")->returns),
              R"("Values:" table[row[th["Value"] th["Meaning"]] row[td[bold(**)["0"]] td["No list."]]] "And so on.")");
    EXPECT_EQ(shape(members.find("T:N.Emphasis")->summary),
              R"(bold(**)["Bold"] " and " italic(_)["EaseIn"] ", but not a_b_c.")");
    EXPECT_EQ(members.find("T:N.Angles")->summary, plain("IVector<String> and <xxx:yyy>, [,<name>[=<value>]."));
    EXPECT_EQ(members.find("M:N.Generic`1")->summary, plain("Generic."));
}

// The remnant of a Markdown image that lost its address -- "!caption", a line
// break and the caption again -- goes whole, after <br/> and after the two
// spaces of a Markdown line break alike. A "!" with no caption repeated after
// the break is text, and the break a line break.
TEST(xml_input, documentation_file_drops_the_remnant_of_an_image) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.remnant.xml";
    std::ofstream {file, std::ios::binary}
        << "<doc><members>\r\n"
           "<member name=\"P:N.Html\"><summary>Gets them.\r\n\r\n"
           "!A menu with accelerators&lt;br/&gt;\r\nA menu with accelerators</summary></member>\r\n"
           "<member name=\"P:N.Markdown\"><summary>Gets it.\r\n\r\n"
           "!A menu with accelerators  \r\nA menu with accelerators</summary></member>\r\n"
           "<member name=\"P:N.Exclaimed\"><summary>Sighs !Wow&lt;br/&gt;Nope.</summary></member>\r\n"
           "</members></doc>\r\n";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(members.find("P:N.Html")->summary, plain("Gets them."));
    EXPECT_EQ(members.find("P:N.Markdown")->summary, plain("Gets it."));
    EXPECT_EQ(shape(members.find("P:N.Exclaimed")->summary), R"("Sighs !Wow" br "Nope.")");
}

// What the reader would lose stops it, naming the file, the member, the line and
// the element: an element inside a part of a member, and one no member's
// documentation has. Asked for again, the member throws again; the others read
// as ever.
TEST(xml_input, documentation_file_names_an_element_it_would_lose) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.lost.xml";
    std::ofstream {file} << "<doc><members>\n"
                            "<member name=\"T:N.Linked\">\n"
                            "<summary>Use\n"
                            "<see cref=\"T:N.Other\"/> instead.</summary></member>\n"
                            "<member name=\"T:N.Remarked\"><summary>Fine.</summary>\n"
                            "<remarks>More.</remarks></member>\n"
                            "<member name=\"T:N.Fine\"><summary>Fine.</summary></member>\n"
                            "</members></doc>\n";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    auto const error = [&members](std::string_view id) {
        try {
            members.find(id);
        } catch (std::runtime_error const& thrown) {
            return std::string {thrown.what()};
        }
        return std::string {};
    };
    for (int ask = 0; ask < 2; ++ask) {
        for (auto&& [id, what] : {std::pair {"T:N.Linked", "T:N.Linked: line 4: <see> inside <summary>"},
                                  std::pair {"T:N.Remarked", "T:N.Remarked: line 6: <remarks>"}}) {
            std::string const thrown = error(id);
            EXPECT_TRUE(thrown.starts_with(utf8(file))) << thrown;
            EXPECT_TRUE(thrown.contains(what)) << thrown;
        }
    }
    EXPECT_EQ(members.find("T:N.Fine")->summary, plain("Fine."));
}

// Markup that does not parse is text as written: a tag left open or closing
// nothing, a list its file cut short, an image whose address is neither ./path
// nor absolute, a tag with an attribute the form does not carry, a table inside
// a superscript or with text outside its cells, a caption repeated with more on
// its line. Tags left open where the text ends, with nothing of them left, go
// with the white space before them.
TEST(xml_input, documentation_file_keeps_markup_that_does_not_parse_as_text) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.unparsed.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="T:N.Open"><summary>f (t) = t&lt;sup&gt;3.</summary></member>
<member name="T:N.Stray"><summary>3&lt;/sup&gt; and &lt;sup/&gt;</summary></member>
<member name="P:N.Policy"><returns>A value that says so. &lt;table&gt; </returns></member>
<member name="M:N.Tile(System.String)">
  <param name="logo">Schemes: &lt;termdeflist&gt;&lt;termdef&gt;&lt;name&gt;ms-appx:///&lt;/name&gt;A path.</param>
</member>
<member name="T:N.Picture"><summary>See &lt;img src="../up.png" alt="Up"/&gt; and &lt;img src=x&gt;.</summary></member>
<member name="T:N.Scripted"><summary>&lt;sup&gt;&lt;table&gt;&lt;/table&gt;&lt;/sup&gt;</summary></member>
<member name="T:N.Loose">
  <summary>&lt;table&gt;&lt;tr&gt;x&lt;td&gt;y&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;</summary>
</member>
<member name="P:N.Cut"><returns>Values: &lt;table&gt;  &lt;tr&gt;
</returns></member>
<member name="T:N.Attributed"><summary>Not &lt;img src="./x.png" class="wide"/&gt;, nor
  &lt;table&gt;&lt;tr&gt;&lt;td colspan="2"&gt;x&lt;/td&gt;&lt;/tr&gt;&lt;/table&gt;.</summary></member>
<member name="T:N.Captioned"><summary>Gets them.
!A menu&lt;br/&gt;A menu and more</summary></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(members.find("T:N.Open")->summary, plain("f (t) = t<sup>3."));
    EXPECT_EQ(members.find("T:N.Stray")->summary, plain("3</sup> and <sup/>"));
    EXPECT_EQ(members.find("P:N.Policy")->returns, plain("A value that says so."));
    EXPECT_EQ(members.find("M:N.Tile(System.String)")->params[0].second,
              plain("Schemes: <termdeflist><termdef><name>ms-appx:///</name>A path."));
    EXPECT_EQ(members.find("T:N.Picture")->summary, plain(R"(See <img src="../up.png" alt="Up"/> and <img src=x>.)"));
    EXPECT_EQ(shape(members.find("T:N.Scripted")->summary), R"(sup["<table></table>"])");
    EXPECT_EQ(members.find("T:N.Loose")->summary, plain("<table><tr>x<td>y</td></tr></table>"));
    EXPECT_EQ(members.find("P:N.Cut")->returns, plain("Values:"));
    EXPECT_EQ(members.find("T:N.Attributed")->summary,
              plain(R"(Not <img src="./x.png" class="wide"/>, nor <table><tr><td colspan="2">x</td></tr></table>.)"));
    EXPECT_EQ(shape(members.find("T:N.Captioned")->summary), R"("Gets them. !A menu" br "A menu and more")");
}

// Bold and italic are what Doxygen's Markdown makes of the text as written: a
// mark opens after white space or punctuation and before a letter, closes after
// a letter and before none, and pairs with the first such mark of its width; _
// and * alike. Not inside a code span or a quoted string, and not across an end
// tag; a backslash or an @ inside, which the writer escapes, ends none, and a
// quote ends no code span. A / before * keeps it from opening, but not before
// **. White space after a line break goes, and is none before a closing mark;
// white space after the mark stays.
TEST(xml_input, documentation_file_reads_bold_and_italic_as_doxygen_does) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.emphasis.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="T:N.Loose"><summary>Then 2 ** 3, ** false** and a*b*c, but *shape*, __bold__ and _a_b_.</summary></member>
<member name="T:N.Paired">
  <summary>If the region is supported, **true**. If this method returns **true**, the region is.</summary>
</member>
<member name="T:N.Kept"><summary>Not in `a*b*c` nor "a *b* c", nor and/*or*, but /**x**.</summary></member>
<member name="T:N.Escaped">
  <summary>Mail *someone@contoso.com*, *C:\Windows*, **@param**, **\c** and _\\server\share_.</summary>
</member>
<member name="T:N.Ticked"><summary>Use `'a'` or _b_, not `rock' _n_ roll`.</summary></member>
<member name="T:N.Lines"><summary>See _a&lt;br/&gt; _ b and _c&lt;br/&gt;_ d.</summary></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(shape(members.find("T:N.Loose")->summary),
              R"("Then 2 ** 3, ** false** and a*b*c, but " italic(*)["shape"] ", " bold(__)["bold"] " and " )"
              R"(italic(_)["a_b"] ".")");
    EXPECT_EQ(shape(members.find("T:N.Paired")->summary),
              R"("If the region is supported, " bold(**)["true"] ". If this method returns " bold(**)["true"] )"
              R"(", the region is.")");
    EXPECT_EQ(shape(members.find("T:N.Kept")->summary),
              R"("Not in " code(`a*b*c`) " nor "a *b* c", nor and/*or*, but /" bold(**)["x"] ".")");
    EXPECT_EQ(shape(members.find("T:N.Escaped")->summary),
              R"("Mail " italic(*)["someone@contoso.com"] ", " italic(*)["C:\Windows"] ", " bold(**)["@param"] ", " )"
              R"(bold(**)["\c"] " and " italic(_)["\\server\share"] ".")");
    EXPECT_EQ(shape(members.find("T:N.Ticked")->summary),
              R"("Use " code(`'a'`) " or " italic(_)["b"] ", not " code(`rock' _n_ roll`) ".")");
    EXPECT_EQ(shape(members.find("T:N.Lines")->summary),
              R"("See " italic(_)["a" br] " b and " italic(_)["c" br] " d.")");
}

// A code span is a piece of its own, as written, its backticks and white space
// folded: nothing in it is markup. Three backticks that nothing closes are text.
// Emphasis around a span with a backslash or an @ before a letter does not
// close, as Doxygen's search for the closing mark ends there -- unless one of
// ( { [ < \ @ stands before it, which Doxygen passes over.
TEST(xml_input, documentation_file_reads_a_code_span_as_written) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.code.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="F:N.Denied">
  <summary>Without ``&lt;uap4:Capability   Name="offlineMapsManagement" /&gt;`` declared.</summary>
</member>
<member name="T:N.Fence"><summary>Three ``` alone.</summary></member>
<member name="T:N.Commanded"><summary>*Use `@param` here*, but *use `x` here*.</summary></member>
<member name="T:N.Mailed"><summary>*Mail `a@b` here*.</summary></member>
<member name="T:N.Passed"><summary>*Use `(\c)` here*, _and `\\d` there_.</summary></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(shape(members.find("F:N.Denied")->summary),
              R"("Without " code(``<uap4:Capability Name="offlineMapsManagement" />``) " declared.")");
    EXPECT_EQ(members.find("T:N.Fence")->summary, plain("Three ``` alone."));
    EXPECT_EQ(shape(members.find("T:N.Commanded")->summary),
              R"("*Use " code(`@param`) " here*, but " italic(*)["use " code(`x`) " here"] ".")");
    EXPECT_EQ(shape(members.find("T:N.Mailed")->summary), R"("*Mail " code(`a@b`) " here*.")");
    EXPECT_EQ(shape(members.find("T:N.Passed")->summary),
              R"(italic(*)["Use " code(`(\c)`) " here"] ", " italic(_)["and " code(`\\d`) " there"] ".")");
}

// HTML character references the decoding of the XML leaves in the text -- the
// named ones the descriptions hold, &#N; and &#xN; -- are the characters they
// stand for, once, in the text, in a code span and in an image's attributes.
// Such a character is no part of the markup's tags: a quote one stands for ends
// no value, a "<" opens no tag and a "!" no remnant of an image. To Markdown it
// is a character like any other, and a quote opens a quoted string. A reference
// to no character, to a control character but the white space the text folds,
// or by a name not known, stays as written.
TEST(xml_input, documentation_file_reads_character_references_as_characters) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.references.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="P:N.DeviceId">
  <summary>Plug-&amp;amp;-Play for Windows&amp;nbsp;10:&amp;#9;&amp;#65;&amp;#x42;&amp;#X43;, `a&amp;amp;b`,
    &amp;amp;amp; once.</summary>
</member>
<member name="F:N.Symbol.Accept">
  <summary>E10B &lt;img alt="Icon &amp;quot;Accept&amp;quot;" src="./a.png?x=1&amp;amp;y=2" /&gt;</summary>
</member>
<member name="T:N.Kept">
  <summary>&amp;lt; &amp;foo; &amp;#xD800; &amp;#1114112; &amp;#; &amp;#0; &amp;#x1B; &amp;#127;
    &amp;amp;gt and &amp;</summary>
</member>
<member name="T:N.Tags">
  <summary>&amp;#60;sup&amp;#62;3&amp;#60;/sup&amp;#62; and &lt;br&amp;#62; and
    &lt;img alt="a&amp;quot; src="./x.png"/&gt;</summary>
</member>
<member name="T:N.Quoted"><summary>Not &amp;quot;a **b** c&amp;quot; nor &amp;#33;A&lt;br/&gt;A</summary></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(shape(members.find("P:N.DeviceId")->summary),
              "\"Plug-&-Play for Windows\xC2\xA0" "10: ABC, \" code(`a&b`) \", &amp; once.\"");
    EXPECT_EQ(shape(members.find("F:N.Symbol.Accept")->summary),
              R"("E10B " img(Icon "Accept")<https://reference.example/api/a.png?x=1&y=2>)");
    EXPECT_EQ(members.find("T:N.Kept")->summary,
              plain("&lt; &foo; &#xD800; &#1114112; &#; &#0; &#x1B; &#127; &gt and &"));
    EXPECT_EQ(members.find("T:N.Tags")->summary, plain(R"(<sup>3</sup> and <br> and <img alt="a" src="./x.png"/>)"));
    EXPECT_EQ(shape(members.find("T:N.Quoted")->summary), R"("Not "a **b** c" nor !A" br "A")");
}

// An image's attributes in any order and either quote, its source relative to
// its page or absolute, its alt text there or not; a line break as <br>, <br/>
// and <br />.
TEST(xml_input, documentation_file_reads_images_and_line_breaks_in_any_spelling) {
    auto const file = std::filesystem::temp_directory_path() / "wxl.gen.common.spellings.xml";
    std::ofstream {file} << R"xml(<doc><members>
<member name="T:N.Pictures">
  <summary>See &lt;img alt='Single' src="./single.png"&gt; and &lt;img src="https://example.com/y.png"/&gt;.</summary>
</member>
<member name="T:N.Breaks"><summary>One&lt;br&gt;two&lt;br /&gt;three&lt;br/&gt;four</summary></member>
</members></doc>
)xml";
    documentation_file members {file, image_root};
    std::filesystem::remove(file);

    EXPECT_EQ(shape(members.find("T:N.Pictures")->summary),
              R"("See " img(Single)<https://reference.example/api/single.png> " and " )"
              R"(img<https://example.com/y.png> ".")");
    EXPECT_EQ(shape(members.find("T:N.Breaks")->summary), R"("One" br "two" br "three" br "four")");
}
