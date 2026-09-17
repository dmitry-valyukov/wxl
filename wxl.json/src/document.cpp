module wxl.json;

import std;
import wxl.core;
import wxl.xml;

namespace wxl::json {
namespace {

// Черновики разбора берут память из STA-пула: стек значений растёт и
// опорожняется на каждом документе, и это ровно тот повтор, ради которого
// пул и заведён.
template <typename T>
using sta_vector = std::vector<T, core::sta_allocator<T>>;

using sta_string = std::basic_string<char, std::char_traits<char>, core::sta_allocator<char>>;

/// Место в документе так, как его считают люди: с единицы, колонка в
/// байтах. Считается только на поломке, поэтому счётчика строк в разборе
/// нет -- на удачном пути он был бы платой ни за что.
struct position {
    int line;
    int column;
};

position position_of(const char* const begin, const char* const where) noexcept {
    position at{1, 1};

    for (const char* scan = begin; scan < where; ++scan) {
        if (*scan == '\n') {
            ++at.line;
            at.column = 1;
        } else {
            ++at.column;
        }
    }

    return at;
}

constexpr bool is_space(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

/// Разбор одного документа: съедает буфер, оставляет дерево в арене.
///
/// Рекурсии здесь нет ни одной: вложенность держит явный стек рамок, и
/// документ из миллиона открытых скобок стоит миллиона записей в векторе, а
/// не миллиона кадров стека. Это не педантизм -- глубину документа выбирает
/// не читающий его.
class parser {
public:
    parser(xml::arena& arena, const std::string_view text, const std::string_view file_name) noexcept
        : arena_(arena), begin_(text.data()), at_(text.data()), end_(text.data() + text.size()),
          file_name_(file_name) {}

    const value& parse();

private:
    /// Открытый контейнер: чем он будет, где в values_ начались его дети и
    /// как его самого зовут. Место открывающей скобки -- чтобы сообщение о
    /// незакрытом контейнере показывало на начало, а не на конец файла.
    struct frame {
        value_type kind;
        std::size_t mark;
        core::u8_view name;
        const char* opened;
    };

    [[noreturn]] void fail(std::string_view reason, const char* where,
                           std::source_location rule = std::source_location::current()) const;

    void skip_space();
    void read_value();
    void close_frame();

    value read_scalar(core::u8_view name);
    core::u8_view read_field_name();
    core::u8_view read_string();
    double read_number();
    char32_t read_hex4();
    void expect_word(std::string_view word);

    /// Конец документа. Буфер завершён нулём, поэтому *at_ читается всегда
    /// и вне разбора нуля не бывает -- нуль внутри строки JSON запрещает
    /// сам.
    bool at_end() const noexcept { return at_ == end_; }

    xml::arena& arena_;

    const char* const begin_;
    const char* at_;
    const char* const end_;

    std::string_view file_name_;

    /// Значения, уже разобранные, но ещё не отданные родителю. Дети одного
    /// контейнера лежат подряд с frame::mark -- закрытие копирует этот
    /// хвост в арену одним куском.
    sta_vector<value> values_;
    sta_vector<frame> frames_;

    /// Строка, которую документ прерывает escape-последовательностью,
    /// собирается здесь, а потом переезжает в арену.
    sta_string assembled_;
};

void parser::fail(const std::string_view reason, const char* const where,
                  const std::source_location rule) const {
    const position at = position_of(begin_, where);

    throw parsing_exception(file_name_, at.line, at.column, reason, rule);
}

// Пробелы, а с ними и комментарии. Комментарии JSON не знает, и всё же они
// здесь всегда, без переключателя: у этого читателя ровно два вида пищи --
// ответ сервера, где комментария не бывает никогда, и наши собственные
// файлы настроек (wxl.gen/profiles), где комментарий стоит рядом с решением
// и объясняет его. Комментарий не может изменить смысл документа, он может
// только сделать читаемым тот, что иначе был бы отвергнут, -- а документ с
// «//» вне строки сломан и без нас. Переключатель поэтому стоял бы в каждом
// вызове одинаково и не поймал бы ни одной настоящей ошибки.
void parser::skip_space() {
    for (;;) {
        while (!at_end() && is_space(*at_)) ++at_;

        if (at_end() || *at_ != '/') return;

        const char* const opened = at_;

        if (at_ + 1 == end_) fail("после косой черты ожидался комментарий", opened);

        if (at_[1] == '/') {
            at_ += 2;
            while (!at_end() && *at_ != '\n') ++at_;
        } else if (at_[1] == '*') {
            at_ += 2;
            for (;;) {
                if (at_end()) fail("комментарий не закрыт", opened);
                if (*at_ == '*' && at_ + 1 != end_ && at_[1] == '/') {
                    at_ += 2;
                    break;
                }
                ++at_;
            }
        } else {
            fail("после косой черты ожидался комментарий", opened);
        }
    }
}

char32_t parser::read_hex4() {
    if (end_ - at_ < 4) fail("в escape \\u ожидались четыре шестнадцатеричные цифры", at_ - 2);

    char32_t code = 0;

    for (int digit = 0; digit != 4; ++digit) {
        const char c = *at_++;

        code <<= 4;

        if (c >= '0' && c <= '9') code |= static_cast<char32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') code |= static_cast<char32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') code |= static_cast<char32_t>(c - 'A' + 10);
        else fail("в escape \\u ожидались четыре шестнадцатеричные цифры", at_ - 1);
    }

    return code;
}

// Строка. Пока документ её не прерывает, это вид прямо в него -- ни байта
// не копируется. Первая же escape-последовательность переводит на медленный
// путь: она называет символы, которых в документе нет ни одним байтом, и
// такую строку приходится собрать в арене.
core::u8_view parser::read_string() {
    const char* const opened = at_;

    ++at_;  // за открывающей кавычкой

    const char* const from = at_;

    for (;;) {
        if (at_end()) fail("строка не закрыта", opened);

        const char c = *at_;

        if (c == '"') {
            const std::string_view whole(from, static_cast<std::size_t>(at_ - from));
            ++at_;
            return core::unicode::assume_valid(whole);
        }

        if (c == '\\') break;

        if (static_cast<unsigned char>(c) < 0x20)
            fail("в строке управляющий символ; он пишется escape-последовательностью", at_);

        ++at_;
    }

    assembled_.assign(from, static_cast<std::size_t>(at_ - from));

    for (;;) {
        if (at_end()) fail("строка не закрыта", opened);

        const char c = *at_;

        if (c == '"') {
            ++at_;
            return core::unicode::assume_valid(arena_.copy(std::string_view(assembled_)));
        }

        if (static_cast<unsigned char>(c) < 0x20)
            fail("в строке управляющий символ; он пишется escape-последовательностью", at_);

        if (c != '\\') {
            assembled_ += c;
            ++at_;
            continue;
        }

        const char* const escape = at_;

        ++at_;
        if (at_end()) fail("строка не закрыта", opened);

        switch (const char what = *at_++) {
        case '"':
        case '\\':
        case '/': assembled_ += what; break;
        case 'b': assembled_ += '\b'; break;
        case 'f': assembled_ += '\f'; break;
        case 'n': assembled_ += '\n'; break;
        case 'r': assembled_ += '\r'; break;
        case 't': assembled_ += '\t'; break;
        case 'u': {
            char32_t code = read_hex4();

            // Суррогатная пара: 😀 -- это один символ, и только
            // вдвоём эти половины что-то значат. Вторая половина берётся
            // лишь тогда, когда она и правда вторая половина, иначе
            // документ терял бы следующую escape-последовательность.
            if (core::unicode::is_high_surrogate(code) && end_ - at_ >= 6 && at_[0] == '\\' && at_[1] == 'u') {
                const char* const pair = at_;

                at_ += 2;

                const char32_t low = read_hex4();

                if (core::unicode::is_low_surrogate(low))
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                else
                    at_ = pair;
            }

            // Половина пары, оставшаяся одна, в UTF-8 не записывается
            // вовсе. Отвергать из-за неё весь документ не за что -- это
            // испорченный символ, а не испорченная грамматика, -- и она
            // становится U+FFFD, ровно как в wxl::core::unicode::repaired.
            if (core::unicode::is_surrogate(code)) code = core::unicode::replacement_character;

            core::unicode::append_utf8(assembled_, code);
            break;
        }
        default: fail("неизвестная escape-последовательность", escape);
        }
    }
}

// Число. Границы находит грамматика JSON, а читает найденное wxl::core --
// то есть std::from_chars, у которого нет локали и потому нет разницы между
// машиной, настроенной на русский, и любой другой.
double parser::read_number() {
    const char* const from = at_;

    if (!at_end() && *at_ == '-') ++at_;

    if (at_end() || !is_digit(*at_)) fail("число оборвано", from);

    // Ведущего нуля JSON не допускает: 007 -- это не число, а опечатка, и
    // прочесть его семёркой значит скрыть её.
    if (*at_ == '0') ++at_;
    else while (!at_end() && is_digit(*at_)) ++at_;

    if (!at_end() && *at_ == '.') {
        ++at_;
        if (at_end() || !is_digit(*at_)) fail("после точки ожидались цифры", from);
        while (!at_end() && is_digit(*at_)) ++at_;
    }

    if (!at_end() && (*at_ == 'e' || *at_ == 'E')) {
        ++at_;
        if (!at_end() && (*at_ == '+' || *at_ == '-')) ++at_;
        if (at_end() || !is_digit(*at_)) fail("после показателя степени ожидались цифры", from);
        while (!at_end() && is_digit(*at_)) ++at_;
    }

    const std::string_view text(from, static_cast<std::size_t>(at_ - from));
    double number = 0;

    if (!wxl::core::try_parse(text, number)) fail("число не помещается в double", from);

    return number;
}

void parser::expect_word(const std::string_view word) {
    if (static_cast<std::size_t>(end_ - at_) < word.size() ||
        std::string_view(at_, word.size()) != word)
        fail(std::format("ожидалось {}", word), at_);

    at_ += word.size();
}

value parser::read_scalar(const core::u8_view name) {
    const char c = at_end() ? '\0' : *at_;

    if (c == '"') return value::of_string(read_string(), name);
    if (c == 't') return expect_word("true"), value::of_boolean(true, name);
    if (c == 'f') return expect_word("false"), value::of_boolean(false, name);
    if (c == 'n') return expect_word("null"), value::of_null(name);
    if (c == '-' || is_digit(c)) return value::of_number(read_number(), name);

    fail(at_end() ? "документ кончился там, где ожидалось значение" : "ожидалось значение", at_);
}

core::u8_view parser::read_field_name() {
    if (at_end() || *at_ != '"') fail("ожидалось имя поля в кавычках", at_);

    const core::u8_view name = read_string();

    skip_space();

    if (at_end() || *at_ != ':') fail("после имени поля ожидалось двоеточие", at_);

    ++at_;

    return name;
}

// Закрытый контейнер: его дети лежат в values_ подряд с метки рамки, и они
// одним куском переезжают в арену. На их месте остаётся одно значение --
// сам контейнер, готовый лечь в своего родителя.
void parser::close_frame() {
    const frame top = frames_.back();
    frames_.pop_back();

    const std::span<const value> children(values_.data() + top.mark, values_.size() - top.mark);
    const std::span<const value> kept = arena_.copy(children);

    values_.resize(top.mark);
    values_.push_back(value::of_items(top.kind, kept, top.name));
}

// Одно значение вместе со всем, что в нём вложено. Внешний цикл открывает
// значения, внутренний закрывает всё, что этим значением закончилось;
// возврат -- когда закрылся последний контейнер, то есть когда значение
// готово целиком.
void parser::read_value() {
    core::u8_view pending_name;

    for (;;) {
        skip_space();

        const char c = at_end() ? '\0' : *at_;

        if (c == '{' || c == '[') {
            const bool object = c == '{';
            const char closer = object ? '}' : ']';

            frames_.push_back(frame{object ? value_type::object : value_type::array, values_.size(),
                                    pending_name, at_});
            pending_name = {};
            ++at_;

            skip_space();

            if (!at_end() && *at_ == closer) {
                ++at_;
                close_frame();
            } else {
                if (object) pending_name = read_field_name();
                continue;
            }
        } else {
            values_.push_back(read_scalar(pending_name));
            pending_name = {};
        }

        // Значение готово. Дальше либо запятая -- и тогда читать следующее,
        // либо закрывающая скобка -- и тогда закрыть контейнер и повторить
        // тот же вопрос уже про него.
        for (;;) {
            if (frames_.empty()) return;

            skip_space();

            const frame& top = frames_.back();
            const char closer = top.kind == value_type::object ? '}' : ']';

            if (at_end()) fail("контейнер не закрыт", top.opened);

            if (*at_ == ',') {
                ++at_;

                if (top.kind == value_type::object) {
                    skip_space();
                    pending_name = read_field_name();
                }
                break;
            }

            if (*at_ != closer)
                fail(std::format("ожидалась запятая или «{}»", closer), at_);

            ++at_;
            close_frame();
        }
    }
}

const value& parser::parse() {
    skip_space();

    if (at_end()) fail("документ пуст", at_);

    read_value();

    skip_space();

    if (!at_end()) fail("после значения документ не кончился", at_);

    // Корень переезжает в арену: черновой вектор уйдёт вместе с разбором, а
    // дерево должно пережить его -- ровно столько, сколько живёт document.
    return *arena_.create<value>(values_.back());
}

/// Поток целиком, одним буфером. Кусками по мегабайту: страница сообщений
/// это сотни килобайт, и читать её десятками тысяч мелких дочитываний
/// незачем.
std::string read_all(std::istream& source) {
    constexpr std::streamsize chunk = 1 << 20;

    std::string text;

    for (;;) {
        const std::size_t was = text.size();

        text.resize(was + static_cast<std::size_t>(chunk));
        source.read(text.data() + was, chunk);

        const std::streamsize got = source.gcount();

        text.resize(was + static_cast<std::size_t>(got));

        if (source.eof()) return text;

        if (source.bad() || (source.fail() && got != chunk))
            throw exception(std::format("поток оборвался на байте {}", text.size()));
    }
}

}  // namespace

const value& document::load(std::string&& source, const std::string_view file_name) {
    document_ = std::move(source);
    file_name_ = file_name;

    return parse_source();
}

const value& document::load(std::istream& source, const std::string_view file_name) {
    return load(read_all(source), file_name);
}

const value& document::load_file(const std::filesystem::path& path) {
    // Имя файла Windows -- просто последовательность 16-битных чисел, и
    // непарный суррогат в ней ничем не запрещён. В сообщение об ошибке оно
    // попадает почищенным, а не как есть.
    std::string name(core::unicode::repaired(path.wstring()).to_utf8().chars());

    std::ifstream file(path, std::ios::binary);

    if (!file) throw exception(std::format("{}: файл не открывается", name));

    return load(file, name);
}

const value& document::parse_source() {
    // Прошлое дерево умирает целиком и здесь: арена отдаёт блоки одним
    // движением, и ни одного деструктора при этом не зовётся. Пустой арены не
    // бывает, поэтому старая именно умирает, а не опустошается, и новая
    // родится ниже -- вместе с разбором, которому она и нужна.
    arena_.reset();
    root_ = nullptr;

    std::string_view text(document_);

    // Метка порядка байтов. Сервер её не шлёт, а файл, сохранённый чужим
    // редактором, шлёт, и в UTF-8 она не значит ничего, кроме «это UTF-8».
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);

    if (const core::nullable<std::size_t> broken = core::unicode::find_invalid_utf8(text)) {
        const position at = position_of(text.data(), text.data() + *broken);

        throw parsing_exception(file_name_, at.line, at.column, "документ не в UTF-8",
                                std::source_location::current());
    }

    arena_ = std::make_unique<xml::arena>();

    parser reader(*arena_, text, file_name_);

    root_ = &reader.parse();

    return *root_;
}

}  // namespace wxl::json
