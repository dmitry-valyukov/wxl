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
std::string pieces(std::wstring_view tag, std::wstring_view code) {
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
        for (const wchar_t c : next->text) {
            EXPECT_LT(c, 128) << "test code must be ASCII";
            out += static_cast<char>(c);
        }
        out += '>';
    }
    return out;
}

TEST(highlight, tags_name_languages_case_insensitively) {
    ASSERT_NE(find_language(L"c#"), nullptr);
    EXPECT_EQ(find_language(L"c#"), find_language(L"cs"));
    EXPECT_EQ(find_language(L"c#"), find_language(L"CSharp"));
    EXPECT_EQ(find_language(L"c"), find_language(L"cpp"));
    EXPECT_NE(find_language(L"c"), find_language(L"c#"));
    EXPECT_EQ(find_language(L"code"), nullptr);
    EXPECT_EQ(find_language(L"1c"), nullptr);
    EXPECT_EQ(find_language(L""), nullptr);
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
            for (const std::wstring_view word : set.words) longest = std::max(longest, word.size());
            EXPECT_EQ(longest, set.longest);
        }
    }
}

TEST(highlight, empty_code_has_no_pieces) {
    scanner scan(*find_language(L"c#"), L"");
    EXPECT_FALSE(scan.next().has_value());
}

TEST(highlight, pieces_cover_the_code_in_order) {
    const std::wstring_view code = L"public static text \"qqq\" // c\nfor(;;)";
    std::wstring joined;
    scanner scan(*find_language(L"c#"), code);
    while (const std::optional<piece> next = scan.next()) joined += next->text;
    EXPECT_EQ(joined, code);
}

TEST(highlight, csharp_keywords_strings_and_comments) {
    EXPECT_EQ(pieces(L"c#", L"public static text \"qqq\" // c"),
              "K<public>P< >K<static>P< text >S<\"qqq\">P< >C<// c>");
    EXPECT_EQ(pieces(L"c#", L"/* a\nb */ x"), "C</* a\nb */>P< x>");
}

TEST(highlight, keyword_needs_a_word_boundary) {
    EXPECT_EQ(pieces(L"c#", L"format form for"), "P<format form >K<for>");
    EXPECT_EQ(pieces(L"c#", L"foreach"), "K<foreach>");
    EXPECT_EQ(pieces(L"c#", L"_int int_ int"), "P<_int int_ >K<int>");
}

TEST(highlight, a_keyword_inside_a_string_stays_a_string) {
    EXPECT_EQ(pieces(L"c#", L"\"if\" if"), "S<\"if\">P< >K<if>");
}

TEST(highlight, csharp_string_escapes_and_verbatim) {
    EXPECT_EQ(pieces(L"c#", L"\"a\\\"b\" x"), "S<\"a\\\"b\">P< x>");
    EXPECT_EQ(pieces(L"c#", L"@\"a\"\"b\" x"), "S<@\"a\"\"b\">P< x>");
    // Незакрытая строка кончается со строкой.
    EXPECT_EQ(pieces(L"c#", L"\"abc\nint"), "S<\"abc>P<\n>K<int>");
}

TEST(highlight, char_literal_is_one_char) {
    EXPECT_EQ(pieces(L"c#", L"'a' '\\n' '\\x41'"), "S<'a'>P< >S<'\\n'>P< >S<'\\x41'>");
    // Время жизни в Rust -- не строка: между кавычкой и следующей не один символ.
    EXPECT_EQ(pieces(L"rust", L"fn f<'a>(x: &'a str)"), "K<fn>P< f<'a>(x: &'a >K<str>P<)>");
}

TEST(highlight, rust_macro_takes_its_bang) {
    EXPECT_EQ(pieces(L"rust", L"assert!(x); let assert = 10;"),
              "K<assert!>P<(x); >K<let>P< assert = 10;>");
}

TEST(highlight, preprocessor_directive_after_hash) {
    EXPECT_EQ(pieces(L"c", L"#include <x>\n# define A"),
              "P<#>K<include>P< <x>\n# >K<define>P< A>");
    EXPECT_EQ(pieces(L"c", L"include"), "P<include>");
}

TEST(highlight, pascal_ignores_case_and_doubles_quotes) {
    EXPECT_EQ(pieces(L"pascal", L"BEGIN s := 'it''s'; { c } End"),
              "K<BEGIN>P< s := >S<'it''s'>P<; >C<{ c }>P< >K<End>");
    EXPECT_EQ(pieces(L"delphi", L"(* a *) // b"), "C<(* a *)>P< >C<// b>");
}

TEST(highlight, sql_and_basic) {
    EXPECT_EQ(pieces(L"sql", L"SELECT 'a''b' -- c"), "K<SELECT>P< >S<'a''b'>P< >C<-- c>");
    EXPECT_EQ(pieces(L"vb", L"Dim s = \"a\"\"b\" ' c"), "K<Dim>P< s = >S<\"a\"\"b\">P< >C<' c>");
}

TEST(highlight, python_docstring_is_a_comment_and_prefixes_are_strings) {
    EXPECT_EQ(pieces(L"python", L"def f():\n    \"\"\"doc\"\"\"\n    return r\"x\""),
              "K<def>P< f():\n    >C<\"\"\"doc\"\"\">P<\n    >K<return>P< >S<r\"x\">");
}

TEST(highlight, lisp_form_head_after_paren) {
    EXPECT_EQ(pieces(L"lisp", L"(defun f () defun) ; c"),
              "P<(>K<defun>P< f () defun) >C<; c>");
}

TEST(highlight, ruby_block_comment_at_line_start) {
    EXPECT_EQ(pieces(L"ruby", L"x\n=begin\na\n=end\ny"), "P<x\n>C<=begin\na\n=end>P<\ny>");
    // Не в начале строки -- не комментарий; begin -- обычное ключевое слово.
    EXPECT_EQ(pieces(L"ruby", L"a =begin b"), "P<a =>K<begin>P< b>");
}

TEST(highlight, perl_pod_needs_a_word) {
    EXPECT_EQ(pieces(L"perl", L"=head1 T\n\ntext\n=cut\nprint"),
              "C<=head1 T\n\ntext\n=cut>P<\n>K<print>");
    EXPECT_EQ(pieces(L"perl", L"= 5"), "P<= 5>");
}

TEST(highlight, assembler_directives) {
    EXPECT_EQ(pieces(L"asm", L".model small\nmov ax, 1 ; c"),
              "P<.>K<model>P< >K<small>P<\n>K<mov>P< >K<ax>P<, 1 >C<; c>");
}

TEST(highlight, xsl_declaration_and_comment) {
    EXPECT_EQ(pieces(L"xml", L"<?xml version=\"1.0\"?><!-- c -->"),
              "P<<>K<?xml>P< >K<version>P<=>S<\"1.0\">P<?>>C<<!-- c -->>");
}

TEST(highlight, unterminated_block_comment_runs_to_the_end) {
    EXPECT_EQ(pieces(L"java", L"a /* b"), "P<a >C</* b>");
}

TEST(highlight, javascript_template_string_swallows_its_holes) {
    // Подстановку ${...} мы не разбираем: вся кавычка -- одна строка.
    EXPECT_EQ(pieces(L"js", L"const s = `a${b}c`; // note"),
              "K<const>P< s = >S<`a${b}c`>P<; >C<// note>");
}

TEST(highlight, typescript_knows_its_type_words) {
    EXPECT_EQ(pieces(L"ts", L"interface A { readonly x: number }"),
              "K<interface>P< A { >K<readonly>P< x: >K<number>P< }>");
}

TEST(highlight, go_raw_string_crosses_lines) {
    EXPECT_EQ(pieces(L"go", L"var s = `a\nb` // c"), "K<var>P< s = >S<`a\nb`>P< >C<// c>");
}

TEST(highlight, css_at_rules_and_properties) {
    EXPECT_EQ(pieces(L"css", L"@media all { color: red; /* c */ }"),
              "K<@media>P< all { >K<color>P<: red; >C</* c */>P< }>");
}

TEST(highlight, lua_long_brackets_beat_the_line_comment) {
    EXPECT_EQ(pieces(L"lua", L"--[[ c ]] x = [[s]] -- t"),
              "C<--[[ c ]]>P< x = >S<[[s]]>P< >C<-- t>");
}

TEST(highlight, cmake_commands_ignore_case) {
    EXPECT_EQ(pieces(L"cmake", L"SET(x 1) # c"), "K<SET>P<(x 1) >C<# c>");
    EXPECT_EQ(pieces(L"cmake", L"if(NOT DEFINED x)"), "K<if>P<(>K<NOT>P< >K<DEFINED>P< x)>");
}

TEST(highlight, shell_hash_opens_a_comment_only_after_a_space) {
    // ${имя#хвост} и $# -- не комментарии, иначе решётка съела бы строку.
    EXPECT_EQ(pieces(L"bash", L"echo ${x#y} # note"), "K<echo>P< ${x#y} >C<# note>");
    EXPECT_EQ(pieces(L"sh", L"echo $#"), "K<echo>P< $#>");
    EXPECT_EQ(pieces(L"bash", L"# note\necho"), "C<# note>P<\n>K<echo>");
}

TEST(highlight, json_has_only_literals_and_strings) {
    EXPECT_EQ(pieces(L"json", L"{\"a\": true, \"b\": null}"),
              "P<{>S<\"a\">P<: >K<true>P<, >S<\"b\">P<: >K<null>P<}>");
}

}  // namespace
