// wxl::MarkupBlock и общий сборщик семейства: обход дерева wxl.html,
// говорящий его содержимое примитивами FormattedBlock. Весь обход — только
// публичный словарь: абзацы, стили, ссылки, картинки; дерево не переживает
// вызова, ровно как велит правило вставки спецификации.
//
// Без рекурсии: дерево глубоко настолько, насколько захотел вход, и обход
// несёт свой стек — как и сами парсеры.

// Проекция первой, и с ней все стандартные заголовки, которые ей нужны:
// собственные заголовки wxl несут импорт wxl.core, а стандартный заголовок
// после импорта MSVC уже не принимает.
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Text.h>

#include <filesystem>

#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.Media.impl.h"
#include "generated/brushes.h"
#include "impl/conversions.h"

// Импорт последним — его несёт markup_build.h.
#include "markup_build.h"

namespace wxl {

namespace {

namespace media = winrt::Microsoft::UI::Xaml::Media;

template <typename T>
using pooled_vector = std::vector<T, core::sta_allocator<T>>;

// Вид тегов живёт в HtmlTheme (MarkupBlock.h) и приезжает на каждый блок;
// константой остаётся одна таблица — семантика HTML, а не вид: наследная
// шкала 1..7 у <font size>, множителями базового кегля.
constexpr double kFontScale[] = {0.63, 0.82, 1.0, 1.13, 1.5, 2.0, 3.0};

// ---- маленькие разборы значений атрибутов ----

std::optional<double> parse_number(std::u16string_view text) {
    if (text.empty()) return std::nullopt;
    double value = 0;
    for (const char16_t c : text) {
        if (c < u'0' || c > u'9') return std::nullopt;
        value = value * 10 + (c - u'0');
    }
    return value;
}

// <font size>: 1..7, или +n/-n от умолчания 3.
std::optional<double> parse_font_scale(std::u16string_view text) {
    if (text.empty()) return std::nullopt;
    const bool relative = text.front() == u'+' || text.front() == u'-';
    const bool negative = text.front() == u'-';
    if (relative) text.remove_prefix(1);
    const std::optional<double> number = parse_number(text);
    if (!number) return std::nullopt;
    const int given = static_cast<int>(*number);
    const int value = std::clamp(relative ? 3 + (negative ? -given : given) : given, 1, 7);
    return kFontScale[value - 1];
}

struct named_color {
    std::u16string_view name;
    std::uint32_t rgb;
};

// Полный набор CSS-имён цвета — самая дешёвая грамматика: одна таблица
// (design.md, «Атрибуты»: принято).
constexpr named_color kNamedColors[] = {
    {u"aliceblue", 0xF0F8FF}, {u"antiquewhite", 0xFAEBD7}, {u"aqua", 0x00FFFF},
    {u"aquamarine", 0x7FFFD4}, {u"azure", 0xF0FFFF}, {u"beige", 0xF5F5DC},
    {u"bisque", 0xFFE4C4}, {u"black", 0x000000}, {u"blanchedalmond", 0xFFEBCD},
    {u"blue", 0x0000FF}, {u"blueviolet", 0x8A2BE2}, {u"brown", 0xA52A2A},
    {u"burlywood", 0xDEB887}, {u"cadetblue", 0x5F9EA0}, {u"chartreuse", 0x7FFF00},
    {u"chocolate", 0xD2691E}, {u"coral", 0xFF7F50}, {u"cornflowerblue", 0x6495ED},
    {u"cornsilk", 0xFFF8DC}, {u"crimson", 0xDC143C}, {u"cyan", 0x00FFFF},
    {u"darkblue", 0x00008B}, {u"darkcyan", 0x008B8B}, {u"darkgoldenrod", 0xB8860B},
    {u"darkgray", 0xA9A9A9}, {u"darkgreen", 0x006400}, {u"darkgrey", 0xA9A9A9},
    {u"darkkhaki", 0xBDB76B}, {u"darkmagenta", 0x8B008B}, {u"darkolivegreen", 0x556B2F},
    {u"darkorange", 0xFF8C00}, {u"darkorchid", 0x9932CC}, {u"darkred", 0x8B0000},
    {u"darksalmon", 0xE9967A}, {u"darkseagreen", 0x8FBC8F}, {u"darkslateblue", 0x483D8B},
    {u"darkslategray", 0x2F4F4F}, {u"darkslategrey", 0x2F4F4F}, {u"darkturquoise", 0x00CED1},
    {u"darkviolet", 0x9400D3}, {u"deeppink", 0xFF1493}, {u"deepskyblue", 0x00BFFF},
    {u"dimgray", 0x696969}, {u"dimgrey", 0x696969}, {u"dodgerblue", 0x1E90FF},
    {u"firebrick", 0xB22222}, {u"floralwhite", 0xFFFAF0}, {u"forestgreen", 0x228B22},
    {u"fuchsia", 0xFF00FF}, {u"gainsboro", 0xDCDCDC}, {u"ghostwhite", 0xF8F8FF},
    {u"gold", 0xFFD700}, {u"goldenrod", 0xDAA520}, {u"gray", 0x808080},
    {u"green", 0x008000}, {u"greenyellow", 0xADFF2F}, {u"grey", 0x808080},
    {u"honeydew", 0xF0FFF0}, {u"hotpink", 0xFF69B4}, {u"indianred", 0xCD5C5C},
    {u"indigo", 0x4B0082}, {u"ivory", 0xFFFFF0}, {u"khaki", 0xF0E68C},
    {u"lavender", 0xE6E6FA}, {u"lavenderblush", 0xFFF0F5}, {u"lawngreen", 0x7CFC00},
    {u"lemonchiffon", 0xFFFACD}, {u"lightblue", 0xADD8E6}, {u"lightcoral", 0xF08080},
    {u"lightcyan", 0xE0FFFF}, {u"lightgoldenrodyellow", 0xFAFAD2}, {u"lightgray", 0xD3D3D3},
    {u"lightgreen", 0x90EE90}, {u"lightgrey", 0xD3D3D3}, {u"lightpink", 0xFFB6C1},
    {u"lightsalmon", 0xFFA07A}, {u"lightseagreen", 0x20B2AA}, {u"lightskyblue", 0x87CEFA},
    {u"lightslategray", 0x778899}, {u"lightslategrey", 0x778899}, {u"lightsteelblue", 0xB0C4DE},
    {u"lightyellow", 0xFFFFE0}, {u"lime", 0x00FF00}, {u"limegreen", 0x32CD32},
    {u"linen", 0xFAF0E6}, {u"magenta", 0xFF00FF}, {u"maroon", 0x800000},
    {u"mediumaquamarine", 0x66CDAA}, {u"mediumblue", 0x0000CD}, {u"mediumorchid", 0xBA55D3},
    {u"mediumpurple", 0x9370DB}, {u"mediumseagreen", 0x3CB371}, {u"mediumslateblue", 0x7B68EE},
    {u"mediumspringgreen", 0x00FA9A}, {u"mediumturquoise", 0x48D1CC}, {u"mediumvioletred", 0xC71585},
    {u"midnightblue", 0x191970}, {u"mintcream", 0xF5FFFA}, {u"mistyrose", 0xFFE4E1},
    {u"moccasin", 0xFFE4B5}, {u"navajowhite", 0xFFDEAD}, {u"navy", 0x000080},
    {u"oldlace", 0xFDF5E6}, {u"olive", 0x808000}, {u"olivedrab", 0x6B8E23},
    {u"orange", 0xFFA500}, {u"orangered", 0xFF4500}, {u"orchid", 0xDA70D6},
    {u"palegoldenrod", 0xEEE8AA}, {u"palegreen", 0x98FB98}, {u"paleturquoise", 0xAFEEEE},
    {u"palevioletred", 0xDB7093}, {u"papayawhip", 0xFFEFD5}, {u"peachpuff", 0xFFDAB9},
    {u"peru", 0xCD853F}, {u"pink", 0xFFC0CB}, {u"plum", 0xDDA0DD},
    {u"powderblue", 0xB0E0E6}, {u"purple", 0x800080}, {u"rebeccapurple", 0x663399},
    {u"red", 0xFF0000}, {u"rosybrown", 0xBC8F8F}, {u"royalblue", 0x4169E1},
    {u"saddlebrown", 0x8B4513}, {u"salmon", 0xFA8072}, {u"sandybrown", 0xF4A460},
    {u"seagreen", 0x2E8B57}, {u"seashell", 0xFFF5EE}, {u"sienna", 0xA0522D},
    {u"silver", 0xC0C0C0}, {u"skyblue", 0x87CEEB}, {u"slateblue", 0x6A5ACD},
    {u"slategray", 0x708090}, {u"slategrey", 0x708090}, {u"snow", 0xFFFAFA},
    {u"springgreen", 0x00FF7F}, {u"steelblue", 0x4682B4}, {u"tan", 0xD2B48C},
    {u"teal", 0x008080}, {u"thistle", 0xD8BFD8}, {u"tomato", 0xFF6347},
    {u"turquoise", 0x40E0D0}, {u"violet", 0xEE82EE}, {u"wheat", 0xF5DEB3},
    {u"white", 0xFFFFFF}, {u"whitesmoke", 0xF5F5F5}, {u"yellow", 0xFFFF00},
    {u"yellowgreen", 0x9ACD32},
};

std::optional<int> hex_digit(char16_t c) {
    if (c >= u'0' && c <= u'9') return c - u'0';
    if (c >= u'a' && c <= u'f') return c - u'a' + 10;
    if (c >= u'A' && c <= u'F') return c - u'A' + 10;
    return std::nullopt;
}

std::optional<Color> parse_color(std::u16string_view text) {
    if (text.empty()) return std::nullopt;

    if (text.front() == u'#') {
        text.remove_prefix(1);
        std::uint32_t rgb = 0;
        if (text.size() == 3) {
            for (const char16_t c : text) {
                const std::optional<int> digit = hex_digit(c);
                if (!digit) return std::nullopt;
                rgb = rgb << 8 | static_cast<std::uint32_t>(*digit * 17);
            }
        } else if (text.size() == 6) {
            for (const char16_t c : text) {
                const std::optional<int> digit = hex_digit(c);
                if (!digit) return std::nullopt;
                rgb = rgb << 4 | static_cast<std::uint32_t>(*digit);
            }
        } else {
            return std::nullopt;
        }
        return Color{255, static_cast<std::uint8_t>(rgb >> 16),
                     static_cast<std::uint8_t>(rgb >> 8), static_cast<std::uint8_t>(rgb)};
    }

    // Имена, без учёта регистра ASCII: <font color=Navy> встречается.
    core::sta_u16string lower{text};
    for (char16_t& c : lower) {
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c + 32);
    }
    for (const named_color& candidate : kNamedColors) {
        if (candidate.name == lower) {
            return Color{255, static_cast<std::uint8_t>(candidate.rgb >> 16),
                         static_cast<std::uint8_t>(candidate.rgb >> 8),
                         static_cast<std::uint8_t>(candidate.rgb)};
        }
    }
    return std::nullopt;
}

// Первое семейство из списка через запятую, без кавычек и пробелов.
std::u16string_view first_face(std::u16string_view faces) {
    const std::size_t comma = faces.find(u',');
    if (comma != std::u16string_view::npos) faces = faces.substr(0, comma);
    while (!faces.empty() && (faces.front() == u' ' || faces.front() == u'"' ||
                              faces.front() == u'\''))
        faces.remove_prefix(1);
    while (!faces.empty() &&
           (faces.back() == u' ' || faces.back() == u'"' || faces.back() == u'\''))
        faces.remove_suffix(1);
    return faces;
}

// ---- обход ----

using html::attr_t;
using html::tag_t;

// Плоский текст поддерева: что <sub>/<sup> отдают appendScript, которому
// разметка внутри ни к чему.
core::sta_u16string flatten(const html::node& element) {
    struct level {
        const html::node* element;
        html::node_list::const_iterator child;
    };

    core::sta_u16string out;
    pooled_vector<level> stack;
    stack.push_back({&element, element.children().begin()});
    while (!stack.empty()) {
        level& top = stack.back();
        if (top.child == top.element->children().end()) {
            stack.pop_back();
            continue;
        }
        const html::node& child = *top.child;
        ++top.child;
        if (child.is_text()) {
            out += child.value();
        } else if (!child.children().empty()) {
            stack.push_back({&child, child.children().begin()});
        }
    }
    return out;
}

// Состояние подгонки одной таблицы, общее для обработчика её полосы:
// колонки по содержимому, пока влезают, и по долям их естественной ширины,
// когда нет, — так укладывает таблицу браузер. Ширина полосы приходит от
// блока (appendWideElement), поэтому в режиме без переноса таблица просто
// остаётся естественной ширины.
class table_fit : public core::sta_refcounted {
public:
    // `extra` — что ячейка добавляет к ширине своего текста: отступы и линия.
    table_fit(Grid grid, Border frame, int columns, double font_size, double extra) noexcept
        : grid_(std::move(grid)), frame_(std::move(frame)), columns_(columns),
          font_size_(font_size), extra_(extra) {}

    // `word` — самое длинное слово ячейки, из которого считается её
    // min-content: RichTextBlock, измеренный нулевой шириной, отвечает
    // нулём, а не словом, так что слово меряется отдельно.
    void add(Border const& box, int column, int span, core::sta_u16string word, bool bold) {
        boxes_.push_back(box);
        columns_of_.push_back(column);
        spans_.push_back(span);
        words_.push_back(std::move(word));
        bold_.push_back(bold);
        narrow_.push_back(-1);
    }

    // Раскладка колонок по ширине полосы — автоматическая раскладка
    // таблицы браузера в её основе: у каждой колонки две ширины, самой
    // широкой ячейки без ограничения (max-content) и самого длинного
    // неразрывного слова (min-content, ячейка измеряется нулевой шириной).
    // Влезает по max — колонки auto; иначе излишек над суммой min делится
    // между колонками пропорционально их запасу max − min, и ни одна не
    // становится уже своего слова: узкая колонка не ломает слова по буквам
    // ради широкой.
    void apply(double available) {
        if (!(available > 0) || columns_ <= 0) return;

        const std::size_t count = static_cast<std::size_t>(columns_);
        pooled_vector<double> widest(count, 0.0);
        pooled_vector<double> narrowest(count, 0.0);
        constexpr float infinite = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < boxes_.size(); ++i) {
            if (spans_[i] != 1) continue;  // объединённые не считаются
            auto native = Object::Impl::as<winrt::Microsoft::UI::Xaml::UIElement>(boxes_[i]);
            const std::size_t column = static_cast<std::size_t>(columns_of_[i]);
            native.Measure({infinite, infinite});
            widest[column] = std::max(widest[column], static_cast<double>(native.DesiredSize().Width));

            if (narrow_[i] < 0) {
                // Слово не меняется — меряется один раз, пробным TextBlock
                // того же кегля.
                winrt::Microsoft::UI::Xaml::Controls::TextBlock probe;
                probe.Text(impl::to_winrt(hstring_param{words_[i]}));
                probe.FontSize(font_size_);
                if (bold_[i]) probe.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
                probe.Measure({infinite, infinite});
                narrow_[i] = probe.DesiredSize().Width + extra_;
            }
            narrowest[column] = std::max(narrowest[column], narrow_[i]);
        }
        double max_total = 0;
        double min_total = 0;
        for (std::size_t c = 0; c < count; ++c) {
            narrowest[c] = std::min(narrowest[c], widest[c]);
            max_total += widest[c];
            min_total += narrowest[c];
        }

        const double inner = available - 1;  // левая линия рамки
        core::sta_u16string spec;
        if (max_total <= inner) {
            for (std::size_t c = 0; c < count; ++c) spec += c ? u",auto" : u"auto";
        } else {
            const double spare = std::max(0.0, inner - min_total);
            const double room = max_total - min_total;
            for (std::size_t c = 0; c < count; ++c) {
                double width = narrowest[c];
                if (room > 0) width += spare * (widest[c] - narrowest[c]) / room;
                if (c) spec += u',';
                append_digits(spec, std::max(1, static_cast<int>(std::floor(width))));
            }
        }
        if (spec == last_) return;
        last_ = spec;
        grid_.columnDefinitions(spec);
    }

    static void append_digits(core::sta_u16string& out, int number) {
        char16_t digits[12];
        int count = 0;
        do {
            digits[count++] = static_cast<char16_t>(u'0' + number % 10);
            number /= 10;
        } while (number != 0);
        while (count-- > 0) out += digits[count];
    }

private:
    Grid grid_;
    Border frame_;
    int columns_;
    double font_size_;
    double extra_;
    pooled_vector<Border> boxes_;
    pooled_vector<int> columns_of_;
    pooled_vector<int> spans_;
    pooled_vector<core::sta_u16string> words_;
    pooled_vector<bool> bold_;
    pooled_vector<double> narrow_;  // min-content ячейки; −1, пока не измерен
    core::sta_u16string last_;
};

class builder {
public:
    // Примитивы говорятся `target` — самому блоку или вложенному блоку ячейки
    // таблицы; настройки, реестр стилей и слив — у корневого `root`.
    builder(FormattedBlock const& target, MarkupBlock const& root, MarkupBlock::Impl const& impl,
            int depth = 0) noexcept
        : block_(target), root_(root), impl_(impl), theme_(impl.theme_),
          base_size_(target.fontSize()), depth_(depth) {}

    void run(const html::node& root) {
        stack_.push_back({&root, root.children().begin()});
        while (!stack_.empty()) {
            const std::size_t here = stack_.size() - 1;
            level_t& top = stack_[here];
            if (top.child == top.element->children().end()) {
                exit_element(top);
                stack_.pop_back();
                continue;
            }
            const html::node& child = *top.child;
            ++top.child;

            // Строчное содержимое после закрывшегося блочного соседа
            // начинает свежий абзац — иначе хвост `<div>a</div> b`
            // продолжил бы абзац блока, слипшись с последним словом.
            const bool inline_child = child.is_text() || !is_block(child.tag());
            if (inline_child && top.after_block) block_.appendParagraph();

            visit(child);

            // `top` мог протухнуть: visit() растил стек; кадр всё там же.
            stack_[here].after_block = !inline_child;
        }
    }

private:
    struct level_t {
        const html::node* element;
        html::node_list::const_iterator child;
        std::uint8_t style_pops = 0;
        bool pops_link = false;

        bool leaves_list = false;
        bool after_block = false;
        bool ends_fold = false;
    };

    // Таблица в таблице в таблице: вложенные блоки ячеек строятся
    // рекурсией, и глубже этого таблица идёт плоскими блоками.
    static constexpr int kMaxTableDepth = 8;

    void visit(const html::node& element) {
        if (element.is_text()) {
            emit_text(element.value());
            return;
        }

        // Бездетные — без своего уровня стека.
        switch (element.tag()) {
        case tag_t::br:
            block_.appendLineBreak();
            return;
        case tag_t::img:
            emit_image(element);
            return;
        case tag_t::sub:
            block_.appendScript(flatten(element), theme_.scriptScale, theme_.subscriptDrop);
            return;
        case tag_t::sup:
            block_.appendScript(flatten(element), theme_.scriptScale, theme_.superscriptDrop);
            return;
        case tag_t::hr:
            emit_rule();
            return;
        case tag_t::summary:
            // Заголовок раскрывашки уже прочитан при входе в <details>.
            return;
        case tag_t::table:
            if (depth_ < kMaxTableDepth) {
                emit_table(element);
                return;
            }
            break;
        case tag_t::pre:
            // Как таблица: своим элементом, со своим блоком внутри. Глубже
            // предела — плоско, обычными абзацами (enter_element).
            if (depth_ < kMaxTableDepth) {
                emit_code(element);
                return;
            }
            break;
        default:
            break;
        }

        stack_.push_back(enter_element(element));
    }

    level_t enter_element(const html::node& element) {
        level_t entered{&element, element.children().begin()};

        // Именованный стиль элемента — поверх вида самого тега, чтобы
        // перекрывал его. Незнакомое имя игнорируется по замыслу (в
        // style чужого HTML лежит настоящий CSS), но записывается: своё
        // имя с опечаткой выглядит ровно так же.
        const HtmlStyle* named = nullptr;
        HtmlStyle coded;
        if (const zstring_view name = element.attribute_str(attr_t::style); !name.empty()) {
            named = impl_.findStyle(name);
            if (!named && code_style(name, coded)) named = &coded;
            if (!named) report(HtmlErrorKind::UnknownStyle, name);
        }

        switch (element.tag()) {
        case tag_t::p:
            open_block(entered, {.margin = theme_.paragraphMargin}, named);
            break;
        case tag_t::div:
            open_block(entered, {}, named);
            break;
        case tag_t::h1:
        case tag_t::h2:
        case tag_t::h3: {
            const int rank = static_cast<int>(element.tag()) - static_cast<int>(tag_t::h1);
            push(entered, {.fontSize = base_size_ * theme_.headingScale[rank], .bold = true});
            open_block(entered, {.margin = theme_.headingMargin}, named);
            break;
        }
        case tag_t::blockquote:
            push(entered, {.color = quote_color(quote_level())});
            open_block(entered, {.margin = theme_.quoteMargin}, named);
            break;
        case tag_t::details:
            open_details(entered, element, named);
            break;
        case tag_t::table:
        case tag_t::tr:
        case tag_t::td:
            // Сюда таблица попадает только глубже kMaxTableDepth: плоско.
            open_block(entered, {}, named);
            break;
        case tag_t::th:
            push(entered, {.bold = true});
            open_block(entered, {}, named);
            break;
        case tag_t::ul:
            lists_.push_back({-1, 0});
            entered.leaves_list = true;
            break;
        case tag_t::ol:
            lists_.push_back({1, numbering(element)});
            entered.leaves_list = true;
            break;
        case tag_t::li:
            open_list_item(entered, named);
            break;
        case tag_t::pre:
            // Пробелы и структура строк приезжают из парсера готовыми
            // (сохранённые пробелы, узлы <br> на переводы); остался вид.
            push(entered, {.fontFamily = theme_.monospace});
            open_block(entered, {.margin = theme_.preMargin}, named);
            break;
        case tag_t::b:
            push(entered, {.bold = true}, named);
            break;
        case tag_t::i:
            push(entered, {.italic = true}, named);
            break;
        case tag_t::u:
            push(entered, {.underline = true}, named);
            break;
        case tag_t::s:
            push(entered, {.strikethrough = true}, named);
            break;
        case tag_t::code:
            push(entered, {.fontFamily = theme_.monospace}, named);
            break;
        case tag_t::span:
            // Своего вида нет: только именованный стиль, ради которого он и
            // существует.
            if (named) push(entered, named->text);
            break;
        case tag_t::font:
            push(entered, font_style(element), named);
            break;
        case tag_t::a:
            block_.pushLink(element.attribute_str(attr_t::href));
            entered.pops_link = true;
            if (named) push(entered, named->text);
            break;
        default:
            break;
        }
        return entered;
    }

    void exit_element(const level_t& left) {
        for (std::uint8_t i = 0; i < left.style_pops; ++i) block_.popStyle();
        if (left.pops_link) block_.popLink();
        if (left.leaves_list) lists_.pop_back();
        if (left.ends_fold) block_.endFold();
    }

    // Линейка: полоса в толщину линии во всю ширину текста, своим абзацем.
    void emit_rule() {
        block_.appendParagraph({.margin = theme_.ruleMargin});
        Border rule{};
        rule.height(theme_.ruleThickness);
        rule.background(line_brush(theme_.ruleColor));
        // Своей ширины у линейки нет: она во всю строку и без переноса.
        block_.appendWideElement(rule, WideWidth::Always);
    }

    // Прячет по имени из шаблона часть контрола: область содержимого у
    // Expander, которого у переключателя складки нет, а место под неё
    // шаблон всё равно держит (MinHeight темы). Обход без рекурсии.
    static void collapse_template_part(winrt::Microsoft::UI::Xaml::DependencyObject const& root,
                                       hstring_param const& name) {
        namespace xaml = winrt::Microsoft::UI::Xaml;
        pooled_vector<xaml::DependencyObject> pending;
        pending.push_back(root);
        while (!pending.empty()) {
            const xaml::DependencyObject current = pending.back();
            pending.pop_back();
            if (const auto element = current.try_as<xaml::FrameworkElement>()) {
                if (element.Name() == impl::to_winrt(name)) {
                    element.Visibility(xaml::Visibility::Collapsed);
                    return;
                }
            }
            const std::int32_t count = xaml::Media::VisualTreeHelper::GetChildrenCount(current);
            for (std::int32_t i = 0; i < count; ++i)
                pending.push_back(xaml::Media::VisualTreeHelper::GetChild(current, i));
        }
    }

    // Кисть линий: цвет из темы блока, а при пустом поле — разделитель темы
    // окна, свой у светлой и у тёмной.
    static Brush line_brush(std::optional<Color> const& color) {
        if (color) return SolidColorBrush{*color};
        return dsl::BrushPaths::DividerStrokeColorDefault;
    }

    // Кисти подложки кода: фон и кромка карточки. Пустое поле — ресурсы
    // карточки самой темы, вторичный фон (на светлой теме он и есть лёгкий
    // серый) и обычная кромка.
    static Brush code_ground(std::optional<Color> const& color) {
        if (color) return SolidColorBrush{*color};
        return dsl::CardBackgroundFillColorBrushes::Secondary;
    }

    static Brush code_edge(std::optional<Color> const& color) {
        if (color) return SolidColorBrush{*color};
        return dsl::CardBrushes::StrokeColorDefault;
    }

    // Код на своей подложке: карточка во всю ширину текста, а внутри —
    // вложенный блок с самим кодом.
    //
    // Вложенный, потому что подложить фон под несколько абзацев
    // RichTextBlock нечем: своего фона у абзаца нет, а элемент в строке —
    // единственное место, где в блоке бывает хоть какая-то подложка. Ровно
    // так же устроена ячейка таблицы, и плата та же: выделение не
    // пересекает границу карточки, внутри неё работая как везде.
    //
    // Вид — wxl::Card в масштабе абзаца: та же тень с подъёмом и та же
    // кромка, но скругление и подъём мельче, а фон на ступень серее.
    void emit_code(const html::node& element) {
        FormattedBlock content{};
        content.fontSize(base_size_);
        content.isTextSelectionEnabled(root_.isTextSelectionEnabled());
        content.onLink([box = impl_.onLink_](zstring_view target) { box->invoke(target); });
        content.onError([box = impl_.onError_](HtmlError const& error) { box->invoke(error); });
        // Моноширинный — на весь код разом: внутри карточки он один на всё,
        // и снимать его не с чего.
        content.pushStyle({.fontFamily = theme_.monospace});
        content.appendParagraph({});

        builder nested(content, root_, impl_, depth_ + 1);
        nested.run(element);

        Border card{};
        card.child(content);
        card.padding(theme_.prePadding);
        card.cornerRadius(CornerRadius{theme_.preRadius});
        card.background(code_ground(theme_.preBackground));
        card.borderBrush(code_edge(theme_.preBorderColor));
        card.borderThickness(Thickness{1});
        card.shadow(ThemeShadow{});
        card.translation({0.0f, 0.0f, static_cast<float>(theme_.preElevation)});

        // Полоса вокруг карточки: тень рисуется за её рамкой, а элемент в
        // строке текста обрезан своим местом в раскладке -- без просвета
        // вокруг от тени не видно ровно ничего. Просвет считается от
        // подъёма, потому что от него тень и зависит; сверху вдвое меньше,
        // потому что тень падает вниз.
        const double room = theme_.preElevation / 2;
        Grid strip{};
        strip.padding(Thickness{room, room / 2, room, room});
        strip.children().append(card);

        block_.appendParagraph({.margin = theme_.preMargin});
        block_.appendWideElement(strip, WideWidth::Always);
    }

    // Раскрывашка: заголовок — Expander без содержимого во всю ширину, а
    // содержимое — обычные абзацы этого же блока, спрятанные складкой
    // (beginFold/endFold). Ничего не вкладывается: пока показано, выделение
    // и поиск идут сквозь него.
    void open_details(level_t& entered, const html::node& element, const HtmlStyle* named) {
        open_block(entered, {.margin = theme_.detailsMargin}, named);

        core::sta_u16string title;
        for (const html::node& child : element.children()) {
            if (child.tag() == tag_t::summary) {
                title = flatten(child);
                break;
            }
        }
        if (title.empty()) title = u"Подробности";

        Expander toggle{};
        toggle.header(title);
        toggle.isExpanded(false);
        toggle.padding(Thickness{0});
        // Содержимого у переключателя нет, а шаблон держит под него место;
        // прячется, как только шаблон построен.
        Object::Impl::as<winrt::Microsoft::UI::Xaml::FrameworkElement>(toggle).Loaded(
            [](winrt::Windows::Foundation::IInspectable const& sender, auto const&) {
                collapse_template_part(sender.as<winrt::Microsoft::UI::Xaml::DependencyObject>(),
                                       u"ExpanderContent");
            });
        block_.appendWideElement(toggle);
        block_.beginFold(toggle);
        entered.ends_fold = true;
        // Содержимое начинается с нового абзаца, а не в абзаце заголовка —
        // абзац заголовка складка не прячет.
        entered.after_block = true;
    }

    struct cell_ref {
        const html::node* node;
        int row;
        int column;
        int row_span;
        int column_span;
        bool header;
    };

    // Самое длинное слово содержимого ячейки — то, уже чего колонка не
    // сожмётся (min-content браузера, без разрыва слов).
    static core::sta_u16string longest_word(const html::node& cell) {
        const core::sta_u16string text = flatten(cell);
        std::u16string_view best;
        std::size_t start = std::u16string_view::npos;
        for (std::size_t i = 0; i <= text.size(); ++i) {
            const bool space = i == text.size() || text[i] == u' ' || text[i] == u'\t' ||
                               text[i] == u'\n' || text[i] == u'\r';
            if (!space) {
                if (start == std::u16string_view::npos) start = i;
                continue;
            }
            if (start != std::u16string_view::npos && i - start > best.size())
                best = std::u16string_view{text}.substr(start, i - start);
            start = std::u16string_view::npos;
        }
        return core::sta_u16string{best};
    }

    // Число из атрибута объединения: 1 по умолчанию, и всё, что не число
    // или меньше единицы, — тоже 1.
    static int span_of(const html::node& cell, attr_t name) {
        const std::optional<double> value = parse_number(cell.attribute_str(name));
        if (!value || *value < 1) return 1;
        return static_cast<int>(std::min(*value, 100.0));
    }

    // Таблица — Grid в рамке, ячейки — вложенные FormattedBlock: своих
    // таблиц у RichTextBlock нет, а строкам и колонкам нужен контейнер,
    // которого в абзаце не бывает. Плата: выделение и поиск не пересекают
    // границу таблицы. Ссылки и диагностика ячеек уходят в обработчики
    // корневого блока.
    void emit_table(const html::node& table) {
        // Раскладка с объединениями: у каждой колонки — первая свободная
        // строка, чтобы rowspan сверху сдвигал ячейки ниже вправо.
        pooled_vector<cell_ref> cells;
        pooled_vector<int> reserved;
        int rows = 0;
        for (const html::node& row : table.children()) {
            if (row.tag() != tag_t::tr) continue;
            int column = 0;
            for (const html::node& cell : row.children()) {
                if (cell.tag() != tag_t::td && cell.tag() != tag_t::th) continue;
                while (column < static_cast<int>(reserved.size()) && reserved[column] > rows)
                    ++column;
                const int column_span = span_of(cell, attr_t::colspan);
                const int row_span = span_of(cell, attr_t::rowspan);
                cells.push_back({&cell, rows, column, row_span, column_span, cell.tag() == tag_t::th});
                if (static_cast<int>(reserved.size()) < column + column_span)
                    reserved.resize(static_cast<std::size_t>(column + column_span), 0);
                for (int c = column; c < column + column_span; ++c) reserved[c] = rows + row_span;
                column += column_span;
            }
            ++rows;
        }
        if (cells.empty()) return;
        for (const int until : reserved) rows = std::max(rows, until);
        const int columns = static_cast<int>(reserved.size());

        Grid grid{};
        core::sta_u16string spec;
        for (int r = 0; r < rows; ++r) spec += r ? u",auto" : u"auto";
        grid.rowDefinitions(spec);
        spec.clear();
        for (int c = 0; c < columns; ++c) spec += c ? u",auto" : u"auto";
        grid.columnDefinitions(spec);

        // Рамка таблицы рисует левую и верхнюю линии, ячейки — правую и
        // нижнюю: каждая линия по одному разу.
        const Brush lines = line_brush(theme_.tableBorderColor);
        Border frame{};
        frame.child(grid);
        frame.borderThickness(Thickness{1, 1, 0, 0});
        frame.borderBrush(lines);
        frame.horizontalAlignment(HorizontalAlignment::Left);

        core::intrusive_ptr<table_fit> fit{
            new table_fit(grid, frame, columns, base_size_,
                          theme_.cellPadding.left + theme_.cellPadding.right + 1),
            /*add_ref=*/false};
        for (const cell_ref& cell : cells) {
            FormattedBlock content{};
            content.fontSize(base_size_);
            content.isTextSelectionEnabled(root_.isTextSelectionEnabled());
            content.onLink([box = impl_.onLink_](zstring_view target) { box->invoke(target); });
            content.onError([box = impl_.onError_](HtmlError const& error) { box->invoke(error); });
            if (cell.header) {
                content.pushStyle({.bold = true});
                content.appendParagraph({.alignment = TextAlignment::Center});
            }
            builder nested(content, root_, impl_, depth_ + 1);
            nested.run(*cell.node);

            Border box{};
            box.child(content);
            box.padding(theme_.cellPadding);
            box.borderThickness(Thickness{0, 0, 1, 1});
            box.borderBrush(lines);
            Grid::setRow(box, cell.row);
            Grid::setColumn(box, cell.column);
            if (cell.row_span > 1) Grid::setRowSpan(box, cell.row_span);
            if (cell.column_span > 1) Grid::setColumnSpan(box, cell.column_span);
            grid.children().append(box);
            fit->add(box, cell.column, cell.column_span, longest_word(*cell.node), cell.header);
        }

        // Полоса во всю ширину текста; таблица в ней — сколько просит, а
        // когда не влезает — вся полоса, колонками по долям. Подгонка — в
        // ответ на размер самой полосы, который ей даёт блок.
        Grid strip{};
        strip.children().append(frame);
        Object::Impl::as<winrt::Microsoft::UI::Xaml::FrameworkElement>(strip).SizeChanged(
            [fit](winrt::Windows::Foundation::IInspectable const& sender, auto const&) {
                // Полоса без заданной ширины — блок не переносит текст или
                // ещё не разложен: таблице никто не мешает, колонки auto.
                const auto strip = sender.as<winrt::Microsoft::UI::Xaml::FrameworkElement>();
                fit->apply(std::isnan(strip.Width()) ? std::numeric_limits<double>::infinity()
                                                     : strip.ActualWidth());
            });

        block_.appendParagraph({.margin = theme_.tableMargin});
        block_.appendWideElement(strip);
    }

    // Блочный элемент: его абзац, блочная половина именованного стиля
    // поверх вида тега, текстовая — надета до выхода.
    void open_block(level_t& entered, BlockStyle style, const HtmlStyle* named) {
        if (named) {
            if (named->block.margin) style.margin = named->block.margin;
            if (named->block.alignment) style.alignment = named->block.alignment;
            push(entered, named->text);
        }
        block_.appendParagraph(style);
    }

    void open_list_item(level_t& entered, const HtmlStyle* named) {
        const double depth = static_cast<double>(lists_.empty() ? 1 : lists_.size());
        open_block(entered, {.margin = Thickness{theme_.listIndent * depth, 0, 0, 2}}, named);

        core::sta_u16string marker;
        if (!lists_.empty() && lists_.back().next > 0) {
            append_marker(marker, lists_.back().next++, lists_.back().numbering);
            marker += u". ";
        } else {
            marker = u"• ";
        }
        block_.appendText(marker);
    }

    void push(level_t& entered, TextStyle const& style) {
        block_.pushStyle(style);
        ++entered.style_pops;
    }

    void push(level_t& entered, TextStyle const& style, const HtmlStyle* named) {
        push(entered, style);
        if (named) push(entered, named->text);
    }

    TextStyle font_style(const html::node& element) const {
        TextStyle style;
        if (const zstring_view color = element.attribute_str(attr_t::color); !color.empty()) {
            style.color = parse_color(color);
            if (!style.color) report(HtmlErrorKind::BadColor, color);
        }
        if (const zstring_view face = element.attribute_str(attr_t::face); !face.empty()) {
            const std::u16string_view family = first_face(face);
            if (!family.empty()) style.fontFamily = FontFamily{hstring{family}};
        }
        if (const zstring_view size = element.attribute_str(attr_t::size); !size.empty()) {
            if (const std::optional<double> scale = parse_font_scale(size)) {
                style.fontSize = base_size_ * *scale;
            } else {
                report(HtmlErrorKind::BadFontSize, size);
            }
        }
        return style;
    }

    // Целыми видами, никогда подстрокой: виды дерева несут терминатор в
    // [size], и разрез посередине его потерял бы — парсер ради этого уже
    // порезал строки <pre> на отдельные куски с узлами <br>.
    void emit_text(zstring_view text) { block_.appendText(text); }

    // В диагностический слив, когда он у блока есть. detail — вид в DOM,
    // действителен на время вызова: ровно то, что обещает HtmlError.
    void report(HtmlErrorKind kind, zstring_view detail) const {
        impl_.onError_->invoke(HtmlError{kind, detail});
    }

    // Сколько цитат уже открыто над этой. Считается по стеку, а не
    // счётчиком: стек глубиной в документ, а спрашивают это только на входе
    // в blockquote, то есть редко, — зато считать нечего при выходе, и
    // счётчик не разъедется с деревом.
    int quote_level() const {
        int level = 0;

        for (const level_t& open : stack_)
            if (open.element && !open.element->is_text() &&
                open.element->tag() == html::tag_t::blockquote)
                ++level;

        return level;
    }

    // Цвет текста цитаты на её уровне: заданный темой, ближайший заданный
    // над ним — или, если тема не сказала ничего, ресурс темы окна
    // TextFillColorSecondaryBrush, разрешённый на первой цитате постройки, и
    // лишь если тот не сплошная кисть, постоянный средне-серый.
    Color quote_color(int level) {
        for (int at = std::min(level, 2); at >= 0; --at)
            if (theme_.quoteColor[at]) return *theme_.quoteColor[at];

        if (!quoteColor_) {
            quoteColor_ = Color{255, 96, 96, 96};
            Brush const& themed = dsl::TextFillColorBrushes::Secondary;
            if (const auto solid =
                    Object::Impl::as<media::Brush>(themed).try_as<media::SolidColorBrush>()) {
                quoteColor_ = impl::from_winrt(solid.Color());
            }
        }

        return *quoteColor_;
    }

    void emit_image(const html::node& element) {
        const zstring_view source = element.attribute_str(attr_t::src);
        if (source.empty()) {
            report(HtmlErrorKind::MissingImage, element.attribute_str(attr_t::alt));
            return;
        }

        // Только локальные источники — решение спецификации. Схема может
        // называть хранилище самого приложения; удалённое отбрасывается с
        // записью: лента, полная http-картинок, должна сказать, почему не
        // показывает ни одной.
        zstring_view src = source;
        core::sta_u16string resolved;
        const std::size_t scheme = src.view().find(u"://");
        if (scheme != std::u16string_view::npos) {
            core::sta_u16string name{src.view().substr(0, scheme)};
            for (char16_t& c : name) {
                if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c + 32);
            }
            if (name != u"file" && name != u"ms-appx" && name != u"ms-appdata") {
                report(HtmlErrorKind::RemoteImage, src);
                return;
            }
        } else if (!impl_.baseDirectory_.empty()) {
            // Голый относительный путь — от каталога документа, когда блоку
            // его назвали. Склейка — basic_string: терминирована, как всякий
            // текстовый параметр.
            const std::filesystem::path path{src.wide()};
            if (path.is_relative()) {
                const std::wstring joined =
                    (std::filesystem::path{zstring_view{impl_.baseDirectory_}.wide()} / path)
                        .native();
                resolved.assign(reinterpret_cast<const char16_t*>(joined.data()), joined.size());
                src = resolved;
            }
        }

        Size size{static_cast<float>(theme_.imageSide), static_cast<float>(theme_.imageSide)};
        if (const zstring_view width = element.attribute_str(attr_t::width); !width.empty()) {
            if (const auto value = parse_number(width)) size.width = static_cast<float>(*value);
            else report(HtmlErrorKind::BadDimension, width);
        }
        if (const zstring_view height = element.attribute_str(attr_t::height); !height.empty()) {
            if (const auto value = parse_number(height)) size.height = static_cast<float>(*value);
            else report(HtmlErrorKind::BadDimension, height);
        }
        block_.appendImage(src, size, element.attribute_str(attr_t::alt));
    }

    // Вид нумерации <ol type>: буква HTML, а всё незнакомое — цифры.
    static char16_t numbering(const html::node& element) noexcept {
        const zstring_view type = element.attribute_str(attr_t::type);
        if (type.size() != 1) return u'1';
        const char16_t kind = type.front();
        return kind == u'a' || kind == u'A' || kind == u'i' || kind == u'I' ? kind : u'1';
    }

    // Номер пункта в виде его списка: 1, a (после z идёт aa, как в браузере),
    // i — римскими; за пределами римской записи снова цифры.
    static void append_marker(core::sta_u16string& marker, int number, char16_t numbering) {
        if ((numbering == u'a' || numbering == u'A') && number > 0) {
            char16_t letters[8];
            int count = 0;
            for (int rest = number; rest > 0 && count < 8; rest /= 26) {
                --rest;
                letters[count++] = static_cast<char16_t>(numbering + rest % 26);
            }
            while (count-- > 0) marker += letters[count];
            return;
        }
        if ((numbering == u'i' || numbering == u'I') && number > 0 && number < 4000) {
            struct step {
                int value;
                std::u16string_view upper;
                std::u16string_view lower;
            };
            constexpr step steps[] = {
                {1000, u"M", u"m"}, {900, u"CM", u"cm"}, {500, u"D", u"d"}, {400, u"CD", u"cd"},
                {100, u"C", u"c"},  {90, u"XC", u"xc"},  {50, u"u", u"l"},  {40, u"XL", u"xl"},
                {10, u"X", u"x"},   {9, u"IX", u"ix"},   {5, u"V", u"v"},   {4, u"IV", u"iv"},
                {1, u"I", u"i"},
            };
            for (const step& one : steps) {
                while (number >= one.value) {
                    marker += numbering == u'I' ? one.upper : one.lower;
                    number -= one.value;
                }
            }
            return;
        }
        table_fit::append_digits(marker, number);
    }

    // Цвет куска кода по имени стиля подсветки: из темы, а при пустом поле
    // — палитра по фактической теме элемента, светлой или тёмной (цвета
    // Visual Studio: синее ключевое слово с белого фона на чёрном не
    // читается). Реестр стилей смотрели раньше: он сильнее.
    bool code_style(std::u16string_view name, HtmlStyle& out) {
        const std::optional<Color>* themed = nullptr;
        std::uint32_t light = 0;
        std::uint32_t dark = 0;
        if (name == u"kw") {
            themed = &theme_.keywordColor;
            light = 0x0000FF;
            dark = 0x569CD6;
        } else if (name == u"str") {
            themed = &theme_.stringColor;
            light = 0x8B0000;
            dark = 0xD69D85;
        } else if (name == u"com") {
            themed = &theme_.commentColor;
            light = 0x008000;
            dark = 0x57A64A;
        } else {
            return false;
        }
        if (*themed) {
            out.text.color = **themed;
            return true;
        }
        const std::uint32_t rgb = dark_theme() ? dark : light;
        out.text.color = Color{255, static_cast<std::uint8_t>(rgb >> 16),
                               static_cast<std::uint8_t>(rgb >> 8), static_cast<std::uint8_t>(rgb)};
        return true;
    }

    // Фактическая тема блока, спрошенная один раз на постройку: тема окна
    // может смениться, но уже построенное хранит вид, которым строилось.
    bool dark_theme() {
        if (!dark_) {
            namespace xaml = winrt::Microsoft::UI::Xaml;
            dark_ = Object::Impl::as<xaml::FrameworkElement>(root_).ActualTheme() ==
                    xaml::ElementTheme::Dark;
        }
        return *dark_;
    }

    FormattedBlock const& block_;
    MarkupBlock const& root_;
    MarkupBlock::Impl const& impl_;
    HtmlTheme const& theme_;
    const double base_size_;
    const int depth_;
    std::optional<Color> quoteColor_;
    std::optional<bool> dark_;

    pooled_vector<level_t> stack_;

    // Открытый список: следующий номер у <ol> (и вид нумерации), -1 у <ul>.
    // Глубина — втяжка.
    struct list_state {
        int next;
        char16_t numbering;
    };
    pooled_vector<list_state> lists_;
};

// Восстановления парсера — в словаре слива.
HtmlErrorKind mapped(html::error_t code) noexcept {
    switch (code) {
    case html::error_t::unknown_tag: return HtmlErrorKind::UnknownTag;
    case html::error_t::unpaired_close: return HtmlErrorKind::UnpairedClose;
    case html::error_t::misnested_tags: return HtmlErrorKind::MisnestedTags;
    case html::error_t::bad_entity: return HtmlErrorKind::BadEntity;
    case html::error_t::misplaced_tag: return HtmlErrorKind::MisplacedTag;
    }
    return HtmlErrorKind::UnknownTag;
}

}  // namespace

void build_markup(MarkupBlock const& block, MarkupBlock::Impl& impl,
                  const html::document& parsed) {
    // Восстановления парсера первыми, в порядке входа; сборщик добавит свои
    // по ходу обхода.
    for (const html::parse_error& error : parsed.errors()) {
        impl.onError_->invoke(HtmlError{mapped(error.code), error.detail});
    }

    builder walk(block, block, impl);
    walk.run(parsed.root());
}

MarkupBlock::MarkupBlock(Impl* impl) noexcept : base_t(impl) {}

void MarkupBlock::theme(HtmlTheme const& value) const {
    static_cast<Impl*>(impl())->theme_ = value;
}

void MarkupBlock::baseDirectory(hstring_param const& directory) const {
    static_cast<Impl*>(impl())->baseDirectory_ = hstring{directory};
}

}  // namespace wxl
