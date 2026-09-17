// Голден-тесты BB-токенизатора: BB-код на входе, каноническая сериализация
// дерева семейства на выходе — тем же html::serialized, которым проверяется
// wxl.html.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.bb;
import wxl.core;

using namespace wxl;

namespace {

// Дерево как UTF-8, чтобы упавший голден печатался читаемо.
std::string parsed(std::wstring_view input) {
    const html::document doc = bb::parse(input);
    const std::wstring wide = html::serialized(doc.root());
    return std::string(wxl::core::unicode::checked(std::wstring_view(wide)).value().to_utf8().chars());
}

TEST(bb, bare_text_stays_bare) {
    EXPECT_EQ(parsed(L"Привет, мир"), "Привет, мир");
}

TEST(bb, inline_formatting) {
    EXPECT_EQ(parsed(L"[b]ж[/b][i]к[/i][u]п[/u][s]з[/s][sub]н[/sub][sup]в[/sup]"),
              "<b>ж</b><i>к</i><u>п</u><s>з</s><sub>н</sub><sup>в</sup>");
}

TEST(bb, tags_are_case_insensitive) {
    EXPECT_EQ(parsed(L"[B]ж[/B]"), "<b>ж</b>");
}

TEST(bb, newline_is_a_line_break) {
    EXPECT_EQ(parsed(L"строка\nвторая"), "строка<br>вторая");
    EXPECT_EQ(parsed(L"строка\r\nвторая"), "строка<br>вторая");
    EXPECT_EQ(parsed(L"абзац\n\nследующий"), "абзац<br><br>следующий");
}

TEST(bb, unknown_tag_is_literal_text) {
    // Форумы показывают [такое] как есть — и мы обязаны совпасть.
    EXPECT_EQ(parsed(L"a [хех] b [unknown=1] c"),
              "a [хех] b [unknown=1] c");
}

TEST(bb, misnested_formatting_recovers) {
    // Восстановление — общее у семейства: тот же tree_builder.
    EXPECT_EQ(parsed(L"[b]first[i]second[/b]third[/i]"),
              "<b>first<i>second</i></b><i>third</i>");
}

TEST(bb, url_with_target) {
    EXPECT_EQ(parsed(L"[url=https://example.invalid]пример[/url]"),
              "<a href=\"https://example.invalid\">пример</a>");
}

TEST(bb, url_bare_target_is_also_text) {
    EXPECT_EQ(parsed(L"[url]https://example.invalid[/url]"),
              "<a href=\"https://example.invalid\">https://example.invalid</a>");
}

TEST(bb, image_source_is_the_content) {
    EXPECT_EQ(parsed(L"[img]https://example.invalid/x.png[/img]"),
              "<img src=\"https://example.invalid/x.png\">");
}

TEST(bb, quote_with_author) {
    EXPECT_EQ(parsed(L"[quote=Вася]цитата[/quote]"),
              "<blockquote><b>Вася:</b><br>цитата</blockquote>");
}

TEST(bb, quote_swallows_framing_newlines) {
    EXPECT_EQ(parsed(L"[quote]\nцитата\n[/quote]\nпосле"),
              "<blockquote>цитата</blockquote>после");
}

TEST(bb, code_is_verbatim) {
    // Теги внутри кода — не разметка; переводы строк сохраняются как <br>
    // силами pre-режима построителя.
    EXPECT_EQ(parsed(L"[code]if (a[b]) {\n  x();\n}[/code]"),
              "<pre>if (a[b]) {<br>  x();<br>}</pre>");
}

TEST(bb, lists_plain_and_ordered) {
    EXPECT_EQ(parsed(L"[list]\n[*]раз\n[*]два\n[/list]"),
              "<ul><li>раз</li><li>два</li></ul>");
    EXPECT_EQ(parsed(L"[list=1][*]раз[*]два[/list]"),
              "<ol><li>раз</li><li>два</li></ol>");
}

TEST(bb, color_size_face_become_font) {
    EXPECT_EQ(parsed(L"[color=red]а[/color][size=5]б[/size][font=Georgia]в[/font]"),
              "<font color=\"red\">а</font><font size=\"5\">б</font>"
              "<font face=\"Georgia\">в</font>");
}

TEST(bb, quoted_attribute_value) {
    EXPECT_EQ(parsed(L"[quote=\"Пётр I\"]верно[/quote]"),
              "<blockquote><b>Пётр I:</b><br>верно</blockquote>");
}

TEST(bb, ampersand_is_just_a_character) {
    EXPECT_EQ(parsed(L"Tom & Jerry &amp;"), "Tom &amp; Jerry &amp;amp;");
}

TEST(bb, unclosed_tags_do_not_leak) {
    // Кусок самодостаточен: finish() закрывает всё открытое.
    EXPECT_EQ(parsed(L"[b]жирный до конца"), "<b>жирный до конца</b>");
}

TEST(bb, code_without_close_runs_to_the_end) {
    EXPECT_EQ(parsed(L"[code]оборванный"), "<pre>оборванный</pre>");
}

}  // namespace
