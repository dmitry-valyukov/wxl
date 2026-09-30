// Тесты сканера подсветки: код на входе, куски с видами на выходе. Языки
// взяты те, где у правила есть что проверить: границы, экранирование,
// удвоенные кавычки, префиксы, регистр.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.highlight;

using namespace wxl::highlight;

namespace {

// Куски строкой: K<...> ключевое слово, S<...> строка, C<...> комментарий,
// P<...> обычный текст. Тесты пишутся на ASCII, поэтому сужение честное.
std::string pieces(std::u16string_view tag, std::u16string_view code) {
    const language* found = find_language(tag);
    if (!found) return "no language";

    std::string out;
    scanner scan(*found, code);
    while (const std::optional<piece> next = scan.next()) {
        switch (next->kind) {
        case kind_t::plain: out += 'P'; break;
        case kind_t::keyword: out += 'K'; break;
        case kind_t::string: out += 'S'; break;
        case kind_t::comment: out += 'C'; break;
        }
        out += '<';
        for (const char16_t c : next->text) {
            EXPECT_LT(c, 128) << "test code must be ASCII";
            out += static_cast<char>(c);
        }
        out += '>';
    }
    return out;
}

TEST(highlight, tags_name_languages_case_insensitively) {
    ASSERT_NE(find_language(u"c#"), nullptr);
    EXPECT_EQ(find_language(u"c#"), find_language(u"cs"));
    EXPECT_EQ(find_language(u"c#"), find_language(u"CSharp"));
    EXPECT_EQ(find_language(u"c"), find_language(u"cpp"));
    EXPECT_NE(find_language(u"c"), find_language(u"c#"));
    EXPECT_EQ(find_language(u"code"), nullptr);
    EXPECT_EQ(find_language(u"1c"), nullptr);
    EXPECT_EQ(find_language(u""), nullptr);
}

TEST(highlight, every_language_has_its_rules) {
    EXPECT_EQ(languages().size(), 31u);
    for (const language& one : languages()) {
        EXPECT_FALSE(one.name.empty());
        EXPECT_FALSE(one.delimiters.empty()) << one.name.size();
        EXPECT_FALSE(one.keywords.empty()) << one.name.size();
        for (const keyword_set& set : one.keywords) {
            EXPECT_FALSE(set.words.empty());
            EXPECT_TRUE(std::ranges::is_sorted(set.words));
            std::size_t longest = 0;
            for (const std::u16string_view word : set.words) longest = std::max(longest, word.size());
            EXPECT_EQ(longest, set.longest);
        }
    }
}

TEST(highlight, empty_code_has_no_pieces) {
    scanner scan(*find_language(u"c#"), u"");
    EXPECT_FALSE(scan.next().has_value());
}

TEST(highlight, pieces_cover_the_code_in_order) {
    const std::u16string_view code = u"public static text \"qqq\" // c\nfor(;;)";
    std::u16string joined;
    scanner scan(*find_language(u"c#"), code);
    while (const std::optional<piece> next = scan.next()) joined += next->text;
    EXPECT_EQ(joined, code);
}

TEST(highlight, csharp_keywords_strings_and_comments) {
    EXPECT_EQ(pieces(u"c#", u"public static text \"qqq\" // c"),
              "K<public>P< >K<static>P< text >S<\"qqq\">P< >C<// c>");
    EXPECT_EQ(pieces(u"c#", u"/* a\nb */ x"), "C</* a\nb */>P< x>");
}

TEST(highlight, keyword_needs_a_word_boundary) {
    EXPECT_EQ(pieces(u"c#", u"format form for"), "P<format form >K<for>");
    EXPECT_EQ(pieces(u"c#", u"foreach"), "K<foreach>");
    EXPECT_EQ(pieces(u"c#", u"_int int_ int"), "P<_int int_ >K<int>");
}

TEST(highlight, a_keyword_inside_a_string_stays_a_string) {
    EXPECT_EQ(pieces(u"c#", u"\"if\" if"), "S<\"if\">P< >K<if>");
}

TEST(highlight, csharp_string_escapes_and_verbatim) {
    EXPECT_EQ(pieces(u"c#", u"\"a\\\"b\" x"), "S<\"a\\\"b\">P< x>");
    EXPECT_EQ(pieces(u"c#", u"@\"a\"\"b\" x"), "S<@\"a\"\"b\">P< x>");
    // Незакрытая строка кончается со строкой.
    EXPECT_EQ(pieces(u"c#", u"\"abc\nint"), "S<\"abc>P<\n>K<int>");
}

TEST(highlight, char_literal_is_one_char) {
    EXPECT_EQ(pieces(u"c#", u"'a' '\\n' '\\x41'"), "S<'a'>P< >S<'\\n'>P< >S<'\\x41'>");
    // Время жизни в Rust -- не строка: между кавычкой и следующей не один символ.
    EXPECT_EQ(pieces(u"rust", u"fn f<'a>(x: &'a str)"), "K<fn>P< f<'a>(x: &'a >K<str>P<)>");
}

TEST(highlight, rust_macro_takes_its_bang) {
    EXPECT_EQ(pieces(u"rust", u"assert!(x); let assert = 10;"),
              "K<assert!>P<(x); >K<let>P< assert = 10;>");
}

TEST(highlight, preprocessor_directive_after_hash) {
    EXPECT_EQ(pieces(u"c", u"#include <x>\n# define A"),
              "P<#>K<include>P< <x>\n# >K<define>P< A>");
    EXPECT_EQ(pieces(u"c", u"include"), "P<include>");
}

TEST(highlight, pascal_ignores_case_and_doubles_quotes) {
    EXPECT_EQ(pieces(u"pascal", u"BEGIN s := 'it''s'; { c } End"),
              "K<BEGIN>P< s := >S<'it''s'>P<; >C<{ c }>P< >K<End>");
    EXPECT_EQ(pieces(u"delphi", u"(* a *) // b"), "C<(* a *)>P< >C<// b>");
}

TEST(highlight, sql_and_basic) {
    EXPECT_EQ(pieces(u"sql", u"SELECT 'a''b' -- c"), "K<SELECT>P< >S<'a''b'>P< >C<-- c>");
    EXPECT_EQ(pieces(u"vb", u"Dim s = \"a\"\"b\" ' c"), "K<Dim>P< s = >S<\"a\"\"b\">P< >C<' c>");
}

TEST(highlight, python_docstring_is_a_comment_and_prefixes_are_strings) {
    EXPECT_EQ(pieces(u"python", u"def f():\n    \"\"\"doc\"\"\"\n    return r\"x\""),
              "K<def>P< f():\n    >C<\"\"\"doc\"\"\">P<\n    >K<return>P< >S<r\"x\">");
}

TEST(highlight, cpp_string_prefixes_are_strings) {
    EXPECT_EQ(pieces(u"cpp", u"a = u\"x\"; b = u'y'; c = u8\"z\"; d = U\"w\";"),
              "P<a = >S<u\"x\">P<; b = >S<u'y'>P<; c = >S<u8\"z\">P<; d = >S<U\"w\">P<;>");
    // Префикс -- начало слова: в середине идентификатора это не он.
    EXPECT_EQ(pieces(u"cpp", u"menu\"q\""), "P<menu>S<\"q\">");
    EXPECT_EQ(pieces(u"python", u"bar\"q\""), "P<bar>S<\"q\">");
}

TEST(highlight, lisp_form_head_after_paren) {
    EXPECT_EQ(pieces(u"lisp", u"(defun f () defun) ; c"),
              "P<(>K<defun>P< f () defun) >C<; c>");
}

TEST(highlight, ruby_block_comment_at_line_start) {
    EXPECT_EQ(pieces(u"ruby", u"x\n=begin\na\n=end\ny"), "P<x\n>C<=begin\na\n=end>P<\ny>");
    // Не в начале строки -- не комментарий; begin -- обычное ключевое слово.
    EXPECT_EQ(pieces(u"ruby", u"a =begin b"), "P<a =>K<begin>P< b>");
}

TEST(highlight, perl_pod_needs_a_word) {
    EXPECT_EQ(pieces(u"perl", u"=head1 T\n\ntext\n=cut\nprint"),
              "C<=head1 T\n\ntext\n=cut>P<\n>K<print>");
    EXPECT_EQ(pieces(u"perl", u"= 5"), "P<= 5>");
}

TEST(highlight, assembler_directives) {
    EXPECT_EQ(pieces(u"asm", u".model small\nmov ax, 1 ; c"),
              "P<.>K<model>P< >K<small>P<\n>K<mov>P< >K<ax>P<, 1 >C<; c>");
}

TEST(highlight, xsl_declaration_and_comment) {
    EXPECT_EQ(pieces(u"xml", u"<?xml version=\"1.0\"?><!-- c -->"),
              "P<<>K<?xml>P< >K<version>P<=>S<\"1.0\">P<?>>C<<!-- c -->>");
}

TEST(highlight, unterminated_block_comment_runs_to_the_end) {
    EXPECT_EQ(pieces(u"java", u"a /* b"), "P<a >C</* b>");
}

TEST(highlight, javascript_template_string_swallows_its_holes) {
    // Подстановку ${...} мы не разбираем: вся кавычка -- одна строка.
    EXPECT_EQ(pieces(u"js", u"const s = `a${b}c`; // note"),
              "K<const>P< s = >S<`a${b}c`>P<; >C<// note>");
}

TEST(highlight, typescript_knows_its_type_words) {
    EXPECT_EQ(pieces(u"ts", u"interface A { readonly x: number }"),
              "K<interface>P< A { >K<readonly>P< x: >K<number>P< }>");
}

TEST(highlight, go_raw_string_crosses_lines) {
    EXPECT_EQ(pieces(u"go", u"var s = `a\nb` // c"), "K<var>P< s = >S<`a\nb`>P< >C<// c>");
}

TEST(highlight, css_at_rules_and_properties) {
    EXPECT_EQ(pieces(u"css", u"@media all { color: red; /* c */ }"),
              "K<@media>P< all { >K<color>P<: red; >C</* c */>P< }>");
}

TEST(highlight, lua_long_brackets_beat_the_line_comment) {
    EXPECT_EQ(pieces(u"lua", u"--[[ c ]] x = [[s]] -- t"),
              "C<--[[ c ]]>P< x = >S<[[s]]>P< >C<-- t>");
}

TEST(highlight, cmake_commands_ignore_case) {
    EXPECT_EQ(pieces(u"cmake", u"SET(x 1) # c"), "K<SET>P<(x 1) >C<# c>");
    EXPECT_EQ(pieces(u"cmake", u"if(NOT DEFINED x)"), "K<if>P<(>K<NOT>P< >K<DEFINED>P< x)>");
}

TEST(highlight, shell_hash_opens_a_comment_only_after_a_space) {
    // ${имя#хвост} и $# -- не комментарии, иначе решётка съела бы строку.
    EXPECT_EQ(pieces(u"bash", u"echo ${x#y} # note"), "K<echo>P< ${x#y} >C<# note>");
    EXPECT_EQ(pieces(u"sh", u"echo $#"), "K<echo>P< $#>");
    EXPECT_EQ(pieces(u"bash", u"# note\necho"), "C<# note>P<\n>K<echo>");
}

TEST(highlight, json_has_only_literals_and_strings) {
    EXPECT_EQ(pieces(u"json", u"{\"a\": true, \"b\": null}"),
              "P<{>S<\"a\">P<: >K<true>P<, >S<\"b\">P<: >K<null>P<}>");
}

}  // namespace
