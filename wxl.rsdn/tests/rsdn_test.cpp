// Голден-тесты токенизатора RSDN: разметка на входе, каноническая
// сериализация дерева семейства на выходе.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.rsdn;
import wxl.core;

using namespace wxl;

namespace {

std::string parsed(std::wstring_view input) {
    const html::document doc = rsdn::parse(input);
    const std::wstring wide = html::serialized(doc.root());
    return std::string(wxl::core::unicode::checked(std::wstring_view(wide)).value().to_utf8().chars());
}

TEST(rsdn, inline_formatting) {
    EXPECT_EQ(parsed(L"[b]ж[/b][i]к[/i][sub]н[/sub][sup]в[/sup]"),
              "<b>ж</b><i>к</i><sub>н</sub><sup>в</sup>");
}

TEST(rsdn, tt_is_monospace) {
    EXPECT_EQ(parsed(L"a [tt]mono[/tt] b"), "a <code>mono</code> b");
}

TEST(rsdn, newline_is_a_line_break) {
    EXPECT_EQ(parsed(L"строка\nвторая"), "строка<br>вторая");
}

TEST(rsdn, prefixed_lines_fold_into_one_quote) {
    // Строки с префиксом «кто-то>» — цитата; префикс остаётся видимым,
    // как показывает его Janus.
    EXPECT_EQ(parsed(L"AVK>первая строка\nAVK>вторая строка\nмой ответ"),
              "<blockquote>AVK&gt;первая строка<br>AVK&gt;вторая строка</blockquote>"
              "мой ответ");
}

TEST(rsdn, arrows_count_as_depth) {
    // Две стрелки — цитата в цитате; одна после них — снова внешняя.
    EXPECT_EQ(parsed(L"В>>глубже\nВ>>ещё\n·>мельче\nответ"),
              "<blockquote><blockquote>В&gt;&gt;глубже<br>В&gt;&gt;ещё</blockquote>"
              "·&gt;мельче</blockquote>ответ");
}

TEST(rsdn, an_underscore_is_a_prefix_too) {
    // Префикс форум собирает из ника, а у ника вроде «kov_serg» заглавных
    // букв нет -- остаётся одно подчёркивание. Взято с живого форума:
    // rsdn.org/forum/cpp, ветка про короутины.
    EXPECT_EQ(parsed(L"_>чужие слова\nмой ответ"),
              "<blockquote>_&gt;чужие слова</blockquote>мой ответ");
    EXPECT_EQ(parsed(L"_>>глубже\nответ"),
              "<blockquote><blockquote>_&gt;&gt;глубже</blockquote></blockquote>ответ");
}

TEST(rsdn, lowercase_before_an_arrow_is_not_a_quote) {
    // «x>y» — сравнение; префикс цитаты пишется заглавными или кириллицей.
    EXPECT_EQ(parsed(L"x>y\nz"), "x&gt;y<br>z");
}

TEST(rsdn, empty_line_does_not_break_the_quote) {
    // Стрелка без имени перед ней — не префикс: так считает и сам форум.
    EXPECT_EQ(parsed(L">раз\n\n>два\nответ"), "&gt;раз<br><br>&gt;два<br>ответ");
    EXPECT_EQ(parsed(L"А>раз\n\nА>два\nответ"),
              "<blockquote>А&gt;раз<br><br>А&gt;два</blockquote>ответ");
}

TEST(rsdn, q_tag_is_a_quote) {
    EXPECT_EQ(parsed(L"[q]\nчужая мысль\n[/q]\nсвоя"),
              "<blockquote>чужая мысль</blockquote>своя");
}

TEST(rsdn, quote_names_its_author) {
    EXPECT_EQ(parsed(L"[q=Козьма]Зри в корень[/q]"),
              "<blockquote><b>Козьма:</b><br>Зри в корень</blockquote>");
    EXPECT_EQ(parsed(L"[quote=Автор]\nслова\n[/quote]"),
              "<blockquote><b>Автор:</b><br>слова</blockquote>");
}

TEST(rsdn, cut_is_a_details_with_its_title) {
    EXPECT_EQ(parsed(L"[cut=Спойлер]тайна[/cut]"),
              "<details><summary>Спойлер</summary>тайна</details>");
    EXPECT_EQ(parsed(L"[cut]\nтайна\n[/cut]"),
              "<details><summary>Скрытый текст</summary>тайна</details>");
}

TEST(rsdn, rule_and_table) {
    EXPECT_EQ(parsed(L"до\n[hr]\nпосле"), "до<hr>после");
    EXPECT_EQ(parsed(L"[t]\n[tr][th]а[/th][td]б[/td][/tr]\n[tr][td]в[/td][td]г[/td][/tr]\n[/t]"),
              "<table><tr><th>а</th><td>б</td></tr><tr><td>в</td><td>г</td></tr></table>");
    // Закрывающие у строк и ячеек не обязательны, как в HTML.
    EXPECT_EQ(parsed(L"[table][tr][td]а[td]б[tr][td]в[/table]"),
              "<table><tr><td>а</td><td>б</td></tr><tr><td>в</td></tr></table>");
}

TEST(rsdn, language_tags_become_pre) {
    EXPECT_EQ(parsed(L"[c#]var x = a[i];\nreturn x;[/c#]"),
              "<pre><span style=\"kw\">var</span> x = a[i];<br>"
              "<span style=\"kw\">return</span> x;</pre>");
    EXPECT_EQ(parsed(L"[code]plain[/code]"), "<pre>plain</pre>");
}

TEST(rsdn, code_is_highlighted_by_language) {
    EXPECT_EQ(parsed(L"[c#]if (x) return \"s\"; // c[/c#]"),
              "<pre><span style=\"kw\">if</span> (x) <span style=\"kw\">return</span> "
              "<span style=\"str\">&quot;s&quot;</span>; <span style=\"com\">// c</span></pre>");
    // Язык значением тега, регистр имени безразличен.
    EXPECT_EQ(parsed(L"[code=SQL]select 1[/code]"),
              "<pre><span style=\"kw\">select</span> 1</pre>");
    EXPECT_EQ(parsed(L"[CODE]if (x)[/code]"), "<pre>if (x)</pre>");
    // Языки форума, которых у RsdnFormatter не было, красит своя таблица.
    EXPECT_EQ(parsed(L"[js]if (x) // c[/js]"),
              "<pre><span style=\"kw\">if</span> (x) <span style=\"com\">// c</span></pre>");
    EXPECT_EQ(parsed(L"[go]func f() // c[/go]"),
              "<pre><span style=\"kw\">func</span> f() <span style=\"com\">// c</span></pre>");
    // Язык, которого подсветка не знает, — просто код.
    EXPECT_EQ(parsed(L"[1c]Если Истина Тогда[/1c]"), "<pre>Если Истина Тогда</pre>");
}

TEST(rsdn, site_languages_are_not_tags_of_their_own) {
    // Сайт красит lua внутри [code=lua] — и мы красим.
    EXPECT_EQ(parsed(L"[code=lua]local x = 1 -- c[/code]"),
              "<pre><span style=\"kw\">local</span> x = 1 "
              "<span style=\"com\">-- c</span></pre>");
    // А тега [lua] у форума нет, и буквальным он остаётся у обоих.
    EXPECT_EQ(parsed(L"[lua]local x[/lua]"), "[lua]local x[/lua]");
}

TEST(rsdn, url_email_and_image) {
    EXPECT_EQ(parsed(L"[url=https://rsdn.org]сайт[/url] и "
                     L"[email]info@example.invalid[/email]"),
              "<a href=\"https://rsdn.org\">сайт</a> и "
              "<a href=\"mailto:info@example.invalid\">info@example.invalid</a>");
    EXPECT_EQ(parsed(L"[img]https://example.invalid/x.png[/img]"),
              "<img src=\"https://example.invalid/x.png\">");
}

TEST(rsdn, image_size_word_is_accepted) {
    EXPECT_EQ(parsed(L"[img=small]https://example.invalid/x.png[/img]"
                     L"[img large]https://example.invalid/y.png[/img]"),
              "<img src=\"https://example.invalid/x.png\">"
              "<img src=\"https://example.invalid/y.png\">");
}

TEST(rsdn, addresses_in_text_are_links) {
    EXPECT_EQ(parsed(L"см. https://rsdn.org/forum, и www.example.invalid."),
              "см. <a href=\"https://rsdn.org/forum\">https://rsdn.org/forum</a>, и "
              "<a href=\"http://www.example.invalid\">www.example.invalid</a>.");
    // Внутри [url] адрес уже ссылка; почта — не ссылка вовсе.
    EXPECT_EQ(parsed(L"[url=https://a.invalid]https://a.invalid[/url] me@a.invalid"),
              "<a href=\"https://a.invalid\">https://a.invalid</a> me@a.invalid");
    // Скобка закрывает адрес, только если внутри её нечем открыть.
    EXPECT_EQ(parsed(L"(http://a.invalid/x) http://a.invalid/y(1)"),
              "(<a href=\"http://a.invalid/x\">http://a.invalid/x</a>) "
              "<a href=\"http://a.invalid/y(1)\">http://a.invalid/y(1)</a>");
}

TEST(rsdn, smiles_become_images) {
    EXPECT_EQ(parsed(L"да :) нет ::) и :up:"),
              "да <img src=\"smiles/smile.gif\" width=\"15\" height=\"15\" alt=\":)\"> "
              "нет ::) и <img src=\"smiles/sup.gif\" width=\"15\" height=\"15\" alt=\":up:\">");
    EXPECT_EQ(parsed(L":)))"),
              "<img src=\"smiles/lol.gif\" width=\"15\" height=\"15\" alt=\":)))\">");
}

TEST(rsdn, escaped_bracket_is_literal) {
    EXPECT_EQ(parsed(L"\\[b]не жирный\\[/b]"), "[b]не жирный[/b]");
}

TEST(rsdn, headings_clamp_to_three) {
    EXPECT_EQ(parsed(L"[h1]раз[/h1][h5]глубоко[/h5]"), "<h1>раз</h1><h3>глубоко</h3>");
}

TEST(rsdn, lists) {
    EXPECT_EQ(parsed(L"[list]\n[*]раз\n[*]два\n[/list]"),
              "<ul><li>раз</li><li>два</li></ul>");
}

TEST(rsdn, list_numbering_kinds) {
    EXPECT_EQ(parsed(L"[list=a]\n[*]раз\n[/list]"), "<ol type=\"a\"><li>раз</li></ol>");
    EXPECT_EQ(parsed(L"[list=I]\n[*]раз\n[/list]"), "<ol type=\"I\"><li>раз</li></ol>");
    EXPECT_EQ(parsed(L"[list=1]\n[*]раз\n[/list]"), "<ol><li>раз</li></ol>");
}

TEST(rsdn, tagline_is_a_quote) {
    EXPECT_EQ(parsed(L"[tagline]подпись[/tagline]"),
              "<blockquote>подпись</blockquote>");
}

TEST(rsdn, unknown_tag_is_literal_text) {
    EXPECT_EQ(parsed(L"a [такое] b"), "a [такое] b");
}

TEST(rsdn, misnesting_recovers_family_fashion) {
    EXPECT_EQ(parsed(L"[b]first[i]second[/b]third[/i]"),
              "<b>first<i>second</i></b><i>third</i>");
}

}  // namespace
