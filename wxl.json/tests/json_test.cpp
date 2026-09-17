// Тесты читателя JSON: документ на входе, дерево на выходе. Взято то, где
// у грамматики есть что проверить, -- границы чисел, escape-по­следова­тель­но­сти,
// суррогатные пары, пустые контейнеры, вложенность и все отказы.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.json;

using namespace wxl;

namespace {

/// Разбор с готовым документом: тесту нужен и корень, и объект, который им
/// владеет, а держать оба руками в каждом тесте незачем.
class parsed {
public:
    explicit parsed(std::string_view source, std::string_view file_name = {})
        : root_(document_.load(std::string(source), file_name)) {}

    const json::value& operator*() const noexcept { return root_; }
    const json::value* operator->() const noexcept { return &root_; }

private:
    json::document document_;
    const json::value& root_;
};

std::string_view chars(const json::value& value) { return value.as_string().chars(); }

TEST(json, scalar_at_the_root_is_a_document) {
    EXPECT_EQ(parsed("42")->as_number(), 42.0);
    EXPECT_EQ(chars(*parsed("\"текст\"")), "текст");
    EXPECT_TRUE(parsed("true")->as_bool());
    EXPECT_FALSE(parsed("false")->as_bool(true));
    EXPECT_TRUE(parsed("null")->is_null());
}

TEST(json, object_answers_by_the_name_of_its_field) {
    const parsed document(R"({"id": 7, "code": "cpp", "isRated": true})");

    EXPECT_TRUE(document->is_object());
    EXPECT_EQ(document->size(), 3u);
    EXPECT_EQ((*document)["id"].as_int(), 7);
    EXPECT_EQ(chars((*document)["code"]), "cpp");
    EXPECT_TRUE((*document)["isRated"].as_bool());
}

TEST(json, a_field_that_is_not_there_reads_as_null) {
    const parsed document(R"({"author": null})");

    EXPECT_TRUE((*document)["author"].is_null());
    EXPECT_TRUE((*document)["nobody"].is_null());

    // Ради этого поиск и отвечает значением, а не указателем: цепочка по
    // отсутствующему полю дописывается до конца и не падает.
    EXPECT_EQ(chars((*document)["author"]["displayName"]), "");
    EXPECT_EQ((*document)["nobody"]["deeper"]["deeper still"].as_int(-1), -1);

    // Кому разница важна -- у того find().
    EXPECT_NE(document->find("author"), nullptr);
    EXPECT_EQ(document->find("nobody"), nullptr);
}

TEST(json, array_keeps_the_order_of_the_document) {
    const parsed document("[10, 20, 30]");

    EXPECT_TRUE(document->is_array());
    ASSERT_EQ(document->size(), 3u);
    EXPECT_EQ((*document)[std::size_t{0}].as_int(), 10);
    EXPECT_EQ((*document)[std::size_t{2}].as_int(), 30);
    EXPECT_TRUE((*document)[std::size_t{3}].is_null());

    int sum = 0;
    for (const json::value& element : document->elements()) sum += static_cast<int>(element.as_int());
    EXPECT_EQ(sum, 60);
}

TEST(json, empty_containers_are_containers) {
    EXPECT_TRUE(parsed("{}")->is_object());
    EXPECT_EQ(parsed("{}")->size(), 0u);
    EXPECT_TRUE(parsed("[]")->is_array());
    EXPECT_EQ(parsed("[]")->size(), 0u);
    EXPECT_TRUE(parsed("[[], {}, []]")->elements()[1].is_object());
}

TEST(json, a_page_of_messages_reads_the_way_it_is_written) {
    const parsed document(R"({
        "items": [
            {"id": 1, "subject": "Тема", "author": {"id": 5, "displayName": "Кто-то"}},
            {"id": 2, "subject": "Ответ", "parentID": 1}
        ],
        "total": 2,
        "offset": 0
    })");

    const json::value& items = (*document)["items"];

    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(chars(items[std::size_t{0}]["author"]["displayName"]), "Кто-то");
    EXPECT_EQ(items[std::size_t{1}]["parentID"].as_int(), 1);
    EXPECT_EQ(items[std::size_t{0}]["parentID"].as_int(-1), -1);
    EXPECT_EQ((*document)["total"].as_int(), 2);
}

TEST(json, members_carry_their_names) {
    const parsed document(R"({"a": 1, "b": 2})");

    std::string names;
    for (const json::value& member : document->members()) {
        names += member.name().chars();
        names += '=';
        names += std::to_string(member.as_int());
        names += ';';
    }

    EXPECT_EQ(names, "a=1;b=2;");

    // У элемента массива имени нет, и это видно.
    EXPECT_EQ(parsed("[1]")->elements()[0].name().chars(), "");
}

TEST(json, the_first_of_two_fields_of_one_name_answers) {
    const parsed document(R"({"id": 1, "id": 2})");

    EXPECT_EQ((*document)["id"].as_int(), 1);
    EXPECT_EQ(document->size(), 2u);
}

TEST(json, numbers_come_back_as_they_were_written) {
    EXPECT_EQ(parsed("-17")->as_int(), -17);
    EXPECT_DOUBLE_EQ(parsed("0.5")->as_number(), 0.5);
    EXPECT_DOUBLE_EQ(parsed("1e3")->as_number(), 1000.0);
    EXPECT_DOUBLE_EQ(parsed("-2.5E-2")->as_number(), -0.025);
    EXPECT_EQ(parsed("0")->as_int(), 0);

    // Идентификатор сообщения -- целое, и оно обязано остаться собой.
    EXPECT_EQ(parsed("7654321")->as_int(), 7654321);
}

TEST(json, a_number_with_a_leading_zero_is_a_typo_and_not_a_number) {
    EXPECT_THROW(parsed("007"), json::parsing_exception);
    EXPECT_THROW(parsed("1."), json::parsing_exception);
    EXPECT_THROW(parsed(".5"), json::parsing_exception);
    EXPECT_THROW(parsed("1e"), json::parsing_exception);
    EXPECT_THROW(parsed("+1"), json::parsing_exception);
}

TEST(json, escapes_are_undone) {
    EXPECT_EQ(chars(*parsed(R"("a\tb\nc")")), "a\tb\nc");
    EXPECT_EQ(chars(*parsed(R"("\"кавычки\"")")), "\"кавычки\"");
    EXPECT_EQ(chars(*parsed(R"("\\")")), "\\");
    EXPECT_EQ(chars(*parsed(R"("\/")")), "/");
    EXPECT_EQ(chars(*parsed(R"("AB")")), "AB");

    // Кириллица через \u -- та же буква, что и написанная прямо.
    EXPECT_EQ(chars(*parsed(R"("П")")), "П");
}

TEST(json, a_surrogate_pair_is_one_character) {
    // U+1F600, смайл: в UTF-16 две половины, в UTF-8 четыре байта.
    EXPECT_EQ(chars(*parsed(R"("😀")")), "\U0001F600");
}

TEST(json, half_a_pair_left_alone_becomes_the_replacement_character) {
    // Испорчен символ, а не грамматика, -- документ читается, символ
    // становится U+FFFD, как в wxl::core::unicode::repaired.
    EXPECT_EQ(chars(*parsed(R"("\ud83d")")), "�");
    EXPECT_EQ(chars(*parsed(R"("\ude00")")), "�");

    // Верхняя половина, за которой идёт не нижняя, не съедает следующую
    // escape-последовательность.
    EXPECT_EQ(chars(*parsed(R"("\ud83dA")")), "�A");
}

TEST(json, a_broken_escape_is_refused) {
    EXPECT_THROW(parsed(R"("\q")"), json::parsing_exception);
    EXPECT_THROW(parsed(R"("\u12")"), json::parsing_exception);
    EXPECT_THROW(parsed(R"("\uZZZZ")"), json::parsing_exception);
}

TEST(json, a_raw_control_character_inside_a_string_is_refused) {
    EXPECT_THROW(parsed("\"строка\nвторая\""), json::parsing_exception);
}

TEST(json, comments_are_read_because_our_own_files_carry_them) {
    const parsed document(R"({
        // так помечен корневой тип
        "roots": ["Button"],   /* а так -- всё остальное */
        "depth": 2
    })");

    EXPECT_EQ((*document)["depth"].as_int(), 2);
    EXPECT_EQ(chars((*document)["roots"][std::size_t{0}]), "Button");

    EXPECT_THROW(parsed("[1] /* и всё же незакрытый комментарий -- поломка"),
                 json::parsing_exception);
    EXPECT_THROW(parsed("[1] / 2"), json::parsing_exception);
}

TEST(json, a_byte_order_mark_is_not_part_of_the_document) {
    EXPECT_EQ(parsed("\xEF\xBB\xBF{\"a\": 1}")->size(), 1u);
}

TEST(json, bytes_that_are_not_utf8_are_refused_whole) {
    EXPECT_THROW(parsed("{\"a\": \"\xC3\x28\"}"), json::parsing_exception);
}

TEST(json, what_the_document_gets_wrong_is_named_and_placed) {
    json::document document;

    try {
        document.load(std::string("{\n  \"a\": 1,\n  \"b\" 2\n}"), "profile.json");
        FAIL() << "документ разобрался";
    } catch (const json::parsing_exception& refused) {
        EXPECT_EQ(refused.file_name(), "profile.json");
        EXPECT_EQ(refused.line(), 3);
        EXPECT_EQ(refused.reason(), "после имени поля ожидалось двоеточие");

        // Сообщение читается редактором: имя, строка, колонка, причина.
        EXPECT_TRUE(std::string_view(refused.what()).starts_with("profile.json(3,"));
    }
}

TEST(json, an_unclosed_container_points_at_where_it_opened) {
    json::document document;

    try {
        document.load(std::string("[\n  1,\n  2\n"));
        FAIL() << "документ разобрался";
    } catch (const json::parsing_exception& refused) {
        EXPECT_EQ(refused.line(), 1);
        EXPECT_EQ(refused.reason(), "контейнер не закрыт");
    }
}

TEST(json, the_grammar_is_the_one_from_the_specification) {
    EXPECT_THROW(parsed(""), json::parsing_exception);
    EXPECT_THROW(parsed("   "), json::parsing_exception);
    EXPECT_THROW(parsed("[1,]"), json::parsing_exception);
    EXPECT_THROW(parsed("{\"a\": 1,}"), json::parsing_exception);
    EXPECT_THROW(parsed("{a: 1}"), json::parsing_exception);
    EXPECT_THROW(parsed("{'a': 1}"), json::parsing_exception);
    EXPECT_THROW(parsed("[1 2]"), json::parsing_exception);
    EXPECT_THROW(parsed("[1] [2]"), json::parsing_exception);
    EXPECT_THROW(parsed("nul"), json::parsing_exception);
    EXPECT_THROW(parsed("{\"a\"}"), json::parsing_exception);
}

TEST(json, nesting_is_bounded_by_memory_and_not_by_the_stack) {
    // Разбор не рекурсивен, и это единственный способ показать это: глубина,
    // на которой рекурсивный читатель кончился бы вместе со стеком потока.
    constexpr int deep = 20000;

    std::string source;
    source.reserve(static_cast<std::size_t>(deep) * 2);
    source.append(static_cast<std::size_t>(deep), '[');
    source.append(static_cast<std::size_t>(deep), ']');

    const parsed document(source);

    const json::value* at = &*document;
    int depth = 0;
    while (at->size() == 1) {
        at = &at->elements()[0];
        ++depth;
    }

    EXPECT_EQ(depth, deep - 1);
    EXPECT_TRUE(at->is_array());
}

TEST(json, a_document_object_reads_more_than_one_document) {
    json::document document;

    EXPECT_EQ(document.root(), nullptr);

    EXPECT_EQ(document.load(std::string(R"({"first": 1})"))["first"].as_int(), 1);
    EXPECT_EQ(document.load(std::string(R"({"second": 2})"))["second"].as_int(), 2);

    // Прошлое дерево умерло вместе с ареной; живо только последнее.
    ASSERT_NE(document.root(), nullptr);
    EXPECT_TRUE((*document.root())["first"].is_null());
    EXPECT_EQ(document.file_name(), "");
}

TEST(json, a_stream_is_read_to_its_end) {
    std::istringstream source(R"({"name": "Беседка", "year": 2026})");

    json::document document;
    const json::value& root = document.load(source, "поток");

    EXPECT_EQ(chars(root["name"]), "Беседка");
    EXPECT_EQ(root["year"].as_int(), 2026);
    EXPECT_EQ(document.file_name(), "поток");
}

TEST(json, a_file_that_is_not_there_is_reported_and_not_guessed) {
    json::document document;

    EXPECT_THROW(document.load_file("нет-такого-файла.json"), json::exception);
}

TEST(json, a_tree_can_be_built_without_a_document) {
    // Дерево -- обычные данные: тест и заглушка сети собирают такое же
    // руками, не выдумывая текста документа.
    const json::value fields[] = {
        json::value::of_number(12, u8"id"),
        json::value::of_string(u8"Беседка", u8"name"),
    };

    const json::value forum = json::value::of_items(json::value_type::object, fields);

    EXPECT_EQ(forum["id"].as_int(), 12);
    EXPECT_EQ(chars(forum["name"]), "Беседка");
}

}  // namespace
