// wxl::highlight -- подсветка синтаксиса: делит исходный текст на куски
// «ключевое слово», «строка», «комментарий» и «всё остальное» по таблице
// языка. Ни одного регулярного выражения: у wxl их нет, и здесь они не
// нужны -- у комментария есть открывающая и закрывающая последовательности,
// у строки кавычка и экранирование, у ключевого слова границы, и этого
// хватает на все языки форума.
//
// Комментарий бывает трёх видов, и все три -- те же две последовательности:
// до конца строки (пустой close), внутристрочный (open и close где угодно)
// и построчный, у которого открывающая и закрывающая строки целиком его
// же (line_start_only, как =begin в Ruby).
//
// Раскраску модуль не знает: он отдаёт куски и что они такое, а каким
// цветом рисовать ключевое слово, решает тот, кто рисует. Определения
// языков лежат в src/languages.cpp, сгенерированном tools/import-syntax.ps1
// из таблиц RsdnFormatter и наших собственных (tools/syntax); таблицы
// открыты, и язык, которого там нет, описывается теми же структурами на
// стороне вызывающего.

export module wxl.highlight;

import std;

export namespace wxl::highlight {

/// Чем является кусок кода -- ровно настолько, насколько это важно
/// раскраске.
enum class kind_t : std::uint8_t { plain, keyword, string, comment };

/// Кусок кода: вид в исходный текст и что это. Куски выходят по порядку и
/// покрывают вход без зазоров, так что склейка их текстов даёт вход.
struct piece {
    std::wstring_view text;
    kind_t kind;
};

/// Граница, у которой ключевое слово считается ключевым. Виды те же, что у
/// RsdnFormatter, где они заменили регулярные выражения границ: `word` --
/// обычное \\b, `dot` -- директивы ассемблера (.model), `hash_with_space`
/// -- препроцессор (# define), `open_paren` -- формы Лиспа ((defun),
/// `exclamation_and_word` -- макросы Rust (assert!, и знак входит в кусок).
enum class boundary_t : std::uint8_t {
    none,
    word,
    dot,
    dot_or_word,
    hash_with_space,
    at_or_word,
    dot_or_at_or_word,
    double_question,
    not_ampersand,
    word_or_double_question,
    exclamation_and_word,
    open_paren,
};

/// Правило комментария или строки: открыл -- закрыл.
struct delimited_rule {
    kind_t kind;
    std::wstring_view open;
    std::wstring_view close;     ///< пусто -- до конца строки
    std::wstring_view prefixes;  ///< один из них может стоять перед open: u"..." в Python
    wchar_t escape;              ///< символ экранирования внутри; 0 -- нет
    bool doubled_close;          ///< удвоенный close внутри -- не конец: 'it''s'
    bool multiline;              ///< может пересечь перевод строки; иначе кончается с ней
    bool line_start_only;        ///< open только в начале строки: =begin в Ruby
    bool single_char;            ///< внутри ровно один символ или одна escape-последовательность:
                                 ///< символьный литерал, иначе 'a в Rust съело бы строку
    bool after_space;            ///< open только в начале строки или после пробела: решётка
                                 ///< shell, где ${имя#хвост} и $# -- не комментарий
};

/// Набор ключевых слов с одними границами. `words` отсортированы по
/// кодовым единицам -- сканер ищет двоичным поиском, -- а у языка без учёта
/// регистра лежат в нижнем.
struct keyword_set {
    boundary_t prefix;
    boundary_t postfix;
    std::span<const std::wstring_view> words;
    std::size_t longest;  ///< длина самого длинного слова: дальше сканер не смотрит
};

/// Язык: правила комментариев и строк в порядке старшинства, наборы
/// ключевых слов в порядке старшинства.
struct language {
    std::wstring_view name;
    bool case_insensitive;
    std::span<const delimited_rule> delimiters;
    std::span<const keyword_set> keywords;
};

/// Язык по имени тега кода: «c#», «cs» и «csharp» -- один C#, регистр
/// имени не важен. nullptr -- язык не известен, в том числе для самого
/// «code»: у кода без языка языка нет.
const language* find_language(std::wstring_view tag) noexcept;

/// Все встроенные языки.
std::span<const language> languages() noexcept;

/// Делит код на куски по одному, ничего не выделяя: куски -- виды в `code`
/// и живут, пока жив он.
///
/// Порядок старшинства в каждой точке -- комментарий или строка, потом
/// ключевое слово, потом обычный текст: ключевое слово внутри строки
/// строкой и остаётся.
class scanner {
public:
    scanner(const language& language, std::wstring_view code) noexcept
        : language_(&language), code_(code) {}

    /// Следующий кусок; пусто, когда код кончился.
    std::optional<piece> next() noexcept;

private:
    struct found {
        std::size_t end;
        kind_t kind;
    };

    std::optional<found> probe(std::size_t at) const noexcept;
    std::optional<found> delimited_at(std::size_t at) const noexcept;
    std::optional<found> keyword_at(std::size_t at) const noexcept;
    std::size_t delimited_end(const delimited_rule& rule, std::size_t at) const noexcept;

    const language* language_;
    std::wstring_view code_;
    std::size_t pos_ = 0;

    // Проба, сделанная при поиске конца обычного куска: следующий вызов
    // начнётся ровно с неё, и считать заново нечего.
    std::size_t probed_at_ = std::wstring_view::npos;
    std::optional<found> probed_;
};

}  // namespace wxl::highlight
