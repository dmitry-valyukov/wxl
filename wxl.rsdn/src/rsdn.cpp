// Токенизатор разметки RSDN над tree_builder семейства.

module wxl.rsdn;

import std;
import wxl.core;
import wxl.highlight;
import wxl.html;
import wxl.xml;

namespace wxl::rsdn {

namespace {

using html::attr_t;
using html::attribute_t;
using html::tag_t;
using html::tree_builder;

constexpr bool is_ascii_letter(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

constexpr bool is_ascii_digit(char16_t c) noexcept { return c >= u'0' && c <= u'9'; }

constexpr bool is_ws(char16_t c) noexcept {
    return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
}

constexpr bool is_newline(char16_t c) noexcept { return c == u'\n' || c == u'\r'; }

constexpr char16_t to_lower_ascii(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
}

// Символ слова -- чтобы адрес не начинался посреди слова, а смайл посреди
// адреса.
constexpr bool is_word(char16_t c) noexcept {
    return is_ascii_letter(c) || is_ascii_digit(c) || c == u'_' || c >= 0x80;
}

// Буква префикса цитаты, как её понимает RsdnFormatter: заглавная латиница
// (инициалы автора), кириллица в любом регистре, «·» безымянной цитаты и
// подчёркивание. Строчная латиница нарочно нет: «x>y» в тексте --
// сравнение, а не цитата.
//
// Подчёркивание здесь потому, что префикс форум собирает из ника, и у ника
// вроде «kov_serg» заглавных букв нет вовсе -- остаётся одно
// подчёркивание. «_>» и «_>>» на форуме встречаются постоянно, и без этого
// целая ветка ответов ему теряет цитаты.
constexpr bool is_prefix_letter(char16_t c) noexcept {
    return (c >= u'A' && c <= u'Z') || (c >= u'А' && c <= u'я') || c == u'Ё' || c == u'ё' ||
           c == u'·' || c == u'_';
}

bool one_of(std::u16string_view name, std::span<const std::u16string_view> names) noexcept {
    for (const std::u16string_view candidate : names) {
        if (name == candidate) return true;
    }
    return false;
}

// Теги кода без подсветки: сам [code] и языки, для которых у форума имя
// есть, а таблицы подсветки нет. Такой остался один -- 1С: ключевых слов
// для него нет ни у RsdnFormatter, ни у подсветки самого сайта.
bool is_plain_code_tag(std::u16string_view name) noexcept {
    constexpr std::u16string_view known[] = {u"code", u"1c"};
    return one_of(name, known);
}

// Языки, которые подсветка знает, а форум тегом -- нет: сайт красит их
// только внутри [code=lua]. Голое [lua] на форуме остаётся текстом, и у
// нас остаётся тоже -- незнакомый тег буквален.
bool is_language_without_tag(std::u16string_view name) noexcept {
    constexpr std::u16string_view known[] = {
        u"bash", u"sh",   u"shell", u"lua",      u"cmake",
        u"json", u"vbs",  u"vbscript", u"jscript", u"golang",
    };
    return one_of(name, known);
}

// Языковые теги кода: у RSDN каждый язык -- свой тег ([c#]...[/c#]), и все
// они для дерева один <pre>.
bool is_code_tag(std::u16string_view name) noexcept {
    return is_plain_code_tag(name) ||
           (!is_language_without_tag(name) && highlight::find_language(name) != nullptr);
}

// Смайлы -- картинки из smiles/ рядом с разметкой, размеры те, что рисует
// сам форум. Порядок важен: длинный код раньше короткого, иначе «:)))»
// стало бы улыбкой и двумя скобками. `not_after` -- символ, после которого
// код не смайл: «::)» на форуме так и экранируют.
struct smile_t {
    core::zstring_view code;
    std::u16string_view file;
    int width;
    int height;
    char16_t not_after;
};

constexpr smile_t kSmiles[] = {
    {u":-)))", u"lol.gif", 15, 15, u':'},
    {u":)))", u"lol.gif", 15, 15, u':'},
    {u":-))", u"biggrin.gif", 15, 15, u':'},
    {u":))", u"biggrin.gif", 15, 15, u':'},
    {u":-)", u"smile.gif", 15, 15, u':'},
    {u":)", u"smile.gif", 15, 15, u':'},
    {u";-)", u"wink.gif", 15, 15, u';'},
    {u";o)", u"wink.gif", 15, 15, u';'},
    {u";O)", u"wink.gif", 15, 15, u';'},
    {u";)", u"wink.gif", 15, 15, u';'},
    {u":-(", u"frown.gif", 15, 15, u':'},
    {u":(", u"frown.gif", 15, 15, u':'},
    {u":-/", u"smirk.gif", 15, 15, u':'},
    {u":-\\", u"smirk.gif", 15, 15, u':'},
    {u":???:", u"confused.gif", 15, 22, 0},
    {u":up:", u"sup.gif", 15, 15, 0},
    {u":down:", u"down.gif", 15, 15, 0},
    {u":super:", u"super.gif", 26, 28, 0},
    {u":shuffle:", u"shuffle.gif", 15, 20, 0},
    {u":crash:", u"crash.gif", 30, 26, 0},
    {u":maniac:", u"maniac.gif", 70, 25, 0},
    {u":user:", u"user.gif", 40, 20, 0},
    {u":wow:", u"wow.gif", 19, 19, 0},
    {u":beer:", u"beer.gif", 57, 16, 0},
    {u":team:", u"invasion.gif", 110, 107, 0},
    {u":no:", u"no.gif", 15, 15, 0},
    {u":nopont:", u"nopont.gif", 35, 35, 0},
    {u":xz:", u"xz.gif", 37, 15, 0},
    {u":facepalm:", u"facepalm.gif", 18, 18, 0},
    {u":sarcasm:", u"sarcasm.gif", 50, 38, 0},
};

constexpr std::u16string_view kSmileDirectory = u"smiles/";

// Имя стиля куска кода: то, что RsdnBlock и тема знают как kw/str/com.
core::zstring_view code_style_name(highlight::kind_t kind) noexcept {
    switch (kind) {
    case highlight::kind_t::keyword: return u"kw";
    case highlight::kind_t::string: return u"str";
    case highlight::kind_t::comment: return u"com";
    default: return {};
    }
}

bool starts_with_ascii_nocase(std::u16string_view text, std::size_t at,
                              std::u16string_view what) noexcept {
    if (text.size() - at < what.size()) return false;
    for (std::size_t i = 0; i < what.size(); ++i) {
        if (to_lower_ascii(text[at + i]) != what[i]) return false;
    }
    return true;
}

void append_number(core::sta_u16string& out, int number) {
    char16_t digits[12];
    int count = 0;
    do {
        digits[count++] = static_cast<char16_t>(u'0' + number % 10);
        number /= 10;
    } while (number != 0);
    while (count-- > 0) out += digits[count];
}

class parser {
public:
    parser(std::u16string_view input, tree_builder& out) : in_(input), out_(out) {}

    void run() {
        while (pos_ < in_.size()) {
            if (at_line_start_) line_start();
            if (pos_ >= in_.size()) break;

            const char16_t c = in_[pos_];
            if (c == u'\\' && pos_ + 1 < in_.size() && in_[pos_ + 1] == u'[') {
                // Экранированная скобка: «\[b]» -- буквально «[b]».
                flush_breaks();
                out_.character(u'[');
                pos_ += 2;
                continue;
            }
            if (c == u'[' && tag()) continue;
            if (c == u'\r') {
                ++pos_;
                if (pos_ >= in_.size() || in_[pos_] != u'\n') newline();
                continue;
            }
            if (c == u'\n') {
                newline();
                ++pos_;
                continue;
            }
            if (link_depth_ == 0 && (smile() || autolink())) continue;
            flush_breaks();
            out_.character(c);
            ++pos_;
        }
        while (quote_level_ > 0) {
            out_.close(tag_t::blockquote);
            --quote_level_;
        }
    }

private:
    // ---- строки и построчные цитаты ----

    void newline() {
        at_line_start_ = true;
        if (swallow_newline_) {
            swallow_newline_ = false;
            return;
        }
        ++pending_breaks_;
    }

    void flush_breaks() {
        swallow_newline_ = false;
        for (; pending_breaks_ > 0; --pending_breaks_) out_.line_break();
    }

    void block_edge() {
        pending_breaks_ = 0;
        swallow_newline_ = true;
    }

    // Начало строки: решает судьбу построчной цитаты. Префикс -- буквы и
    // одна или больше «>», и сколько стрелок -- такова глубина: «CCC>>>»
    // лежит в третьей вложенной цитате. Сама строка остаётся как есть,
    // вместе с префиксом, -- так показывает цитаты Janus.
    void line_start() {
        at_line_start_ = false;

        std::size_t at = pos_;
        while (at < in_.size() && is_prefix_letter(in_[at])) ++at;
        int level = 0;
        if (at > pos_) {
            while (at < in_.size() && in_[at] == u'>') {
                ++level;
                ++at;
            }
        }

        if (level > 0) {
            set_quote_level(level);
        } else if (quote_level_ > 0) {
            // Пустая строка внутри цитаты цитату не рвёт: пауза в чужой
            // речи -- ещё не своя речь.
            if (pos_ < in_.size() && is_newline(in_[pos_])) return;
            set_quote_level(0);
        }
    }

    void set_quote_level(int level) {
        if (level == quote_level_) return;
        block_edge();
        while (quote_level_ < level) {
            out_.open(tag_t::blockquote);
            ++quote_level_;
        }
        while (quote_level_ > level) {
            out_.close(tag_t::blockquote);
            --quote_level_;
        }
        block_edge();
    }

    // ---- смайлы и адреса в тексте ----

    bool smile() {
        for (const smile_t& candidate : kSmiles) {
            if (!starts_at(candidate.code)) continue;
            if (candidate.not_after != 0 && pos_ > 0 && in_[pos_ - 1] == candidate.not_after)
                continue;

            flush_breaks();
            value_.assign(kSmileDirectory);
            value_.append(candidate.file);
            width_.clear();
            append_number(width_, candidate.width);
            height_.clear();
            append_number(height_, candidate.height);
            attrs_.clear();
            attrs_.emplace_back(attr_t::src, out_.copy(value_));
            attrs_.emplace_back(attr_t::width, out_.copy(width_));
            attrs_.emplace_back(attr_t::height, out_.copy(height_));
            attrs_.emplace_back(attr_t::alt, candidate.code);
            out_.open(tag_t::img, out_.copy_attributes(attrs_));
            pos_ += candidate.code.size();
            return true;
        }
        return false;
    }

    // Адрес прямо в тексте -- ссылка: схема или «www.», не посреди слова,
    // до пробела или скобки разметки, без замыкающей пунктуации. Почтовые
    // адреса нарочно не трогаем: так делает и сам форум.
    bool autolink() {
        if (pos_ > 0 && is_word(in_[pos_ - 1])) return false;

        constexpr std::u16string_view schemes[] = {u"http://", u"https://", u"ftp://", u"www."};
        std::size_t head = 0;
        for (const std::u16string_view scheme : schemes) {
            if (starts_with_ascii_nocase(in_, pos_, scheme)) {
                head = scheme.size();
                break;
            }
        }
        if (head == 0) return false;

        std::size_t end = pos_ + head;
        while (end < in_.size() && !is_ws(in_[end]) && in_[end] != u'[' && in_[end] != u']' &&
               in_[end] != u'<' && in_[end] != u'>' && in_[end] != u'"')
            ++end;

        // Точка в конце предложения -- не часть адреса; скобка -- если ей
        // нечего закрывать внутри.
        bool has_open_paren = false;
        for (std::size_t i = pos_; i < end; ++i) has_open_paren |= in_[i] == u'(';
        while (end > pos_ + head) {
            const char16_t last = in_[end - 1];
            const bool punctuation = last == u'.' || last == u',' || last == u';' ||
                                     last == u':' || last == u'!' || last == u'?' ||
                                     last == u'\'' || (last == u')' && !has_open_paren);
            if (!punctuation) break;
            --end;
        }
        if (end == pos_ + head) return false;  // одна схема -- не адрес

        const std::u16string_view address = in_.substr(pos_, end - pos_);
        value_.clear();
        if (head == 4) value_.assign(u"http://");  // www. без схемы
        value_.append(address);

        flush_breaks();
        open_link(value_);
        for (const char16_t c : address) out_.character(c);
        out_.close(tag_t::a);
        pos_ = end;
        return true;
    }

    bool starts_at(std::u16string_view what) const noexcept {
        return in_.size() - pos_ >= what.size() && in_.compare(pos_, what.size(), what) == 0;
    }

    // ---- теги ----

    bool tag() {
        std::size_t i = pos_ + 1;
        bool closing = false;
        if (i < in_.size() && in_[i] == u'/') {
            closing = true;
            ++i;
        }

        name_.clear();
        if (!closing && i < in_.size() && in_[i] == u'*') {
            name_ = u"*";
            ++i;
        } else {
            while (i < in_.size() &&
                   (is_ascii_letter(in_[i]) || is_ascii_digit(in_[i]) || in_[i] == u'#' ||
                    in_[i] == u'+' || in_[i] == u'?')) {
                name_ += to_lower_ascii(in_[i]);
                ++i;
            }
        }
        if (name_.empty()) return false;

        // Значение -- через «=» или просто через пробел: [img=small] и
        // [img small] форум понимает одинаково.
        value_.clear();
        if (!closing) {
            while (i < in_.size() && in_[i] == u' ') ++i;
            if (i < in_.size() && in_[i] == u'=') {
                ++i;
                while (i < in_.size() && in_[i] == u' ') ++i;
            }
            const bool quoted = i < in_.size() && in_[i] == u'"';
            if (quoted) ++i;
            while (i < in_.size() && in_[i] != u']' && !is_newline(in_[i])) {
                value_ += in_[i];
                ++i;
            }
            if (quoted && !value_.empty() && value_.back() == u'"') value_.pop_back();
            while (!value_.empty() && value_.back() == u' ') value_.pop_back();
        }

        if (i >= in_.size() || in_[i] != u']' || i - pos_ > 256) return false;
        const std::size_t past = i + 1;

        return closing ? close_tag(past) : open_tag(past);
    }

    bool open_tag(std::size_t past) {
        if (name_ == u"b" || name_ == u"i" || name_ == u"u" || name_ == u"s" ||
            name_ == u"sub" || name_ == u"sup" || name_ == u"tt") {
            flush_breaks();
            out_.open(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == u"url") {
            flush_breaks();
            pos_ = past;
            if (!value_.empty()) {
                open_link(value_);
                ++link_depth_;
                return true;
            }
            const std::u16string_view target = capture(u"url");
            value_.assign(target);
            open_link(value_);
            for (const char16_t c : target) out_.character(c);
            out_.close(tag_t::a);
            return true;
        }
        if (name_ == u"email") {
            flush_breaks();
            pos_ = past;
            const std::u16string_view address = capture(u"email");
            value_.assign(u"mailto:");
            value_.append(address);
            open_link(value_);
            for (const char16_t c : address) out_.character(c);
            out_.close(tag_t::a);
            return true;
        }
        if (name_ == u"img") {
            // Значение -- small или large; дерево размеров картинки не
            // знает, а ошибочное значение форум тоже пропускает молча.
            flush_breaks();
            pos_ = past;
            const std::u16string_view source = capture(u"img");
            value_.assign(source);
            attrs_.clear();
            attrs_.emplace_back(attr_t::src, out_.copy(value_));
            out_.open(tag_t::img, out_.copy_attributes(attrs_));
            return true;
        }
        if (name_ == u"q" || name_ == u"quote" || name_ == u"tagline" ||
            name_ == u"moderator") {
            block_edge();
            out_.open(tag_t::blockquote);
            if (!value_.empty()) titled(value_);  // [quote=автор]: автор жирной строкой
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == u"cut") {
            // Спойлер -- раскрывающийся блок дерева; заголовок свой или
            // форумный «Скрытый текст».
            block_edge();
            out_.open(tag_t::details);
            out_.open(tag_t::summary);
            if (value_.empty()) value_.assign(u"Скрытый текст");
            for (const char16_t c : value_) out_.character(c);
            out_.close(tag_t::summary);
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == u"hr") {
            block_edge();
            out_.open(tag_t::hr);
            block_edge();
            pos_ = past;
            return true;
        }
        if (is_table_tag()) {
            block_edge();
            out_.open(table_tag());
            block_edge();
            pos_ = past;
            return true;
        }
        if (is_code_tag(name_)) {
            code(past);
            return true;
        }
        if (name_.size() == 2 && name_[0] == u'h' && name_[1] >= u'1' && name_[1] <= u'6') {
            block_edge();
            out_.open(heading(name_[1]));
            pos_ = past;
            return true;
        }
        if (name_ == u"list") {
            block_edge();
            const bool ordered = !value_.empty();
            lists_.push_back(ordered);
            if (ordered) {
                // Вид нумерации -- атрибут type, как у HTML; «1» -- умолчание.
                const char16_t kind = value_.size() == 1 ? value_[0] : u'1';
                if (kind == u'a' || kind == u'A' || kind == u'i' || kind == u'I') {
                    attrs_.clear();
                    attrs_.emplace_back(attr_t::type, out_.copy(value_));
                    out_.open(tag_t::ol, out_.copy_attributes(attrs_));
                } else {
                    out_.open(tag_t::ol);
                }
            } else {
                out_.open(tag_t::ul);
            }
            pos_ = past;
            return true;
        }
        if (name_ == u"*") {
            block_edge();
            out_.open(tag_t::li);
            pos_ = past;
            return true;
        }
        return false;
    }

    bool close_tag(std::size_t past) {
        if (name_ == u"b" || name_ == u"i" || name_ == u"u" || name_ == u"s" ||
            name_ == u"sub" || name_ == u"sup" || name_ == u"tt") {
            flush_breaks();
            out_.close(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == u"url" || name_ == u"email") {
            flush_breaks();
            out_.close(tag_t::a);
            if (link_depth_ > 0) --link_depth_;
            pos_ = past;
            return true;
        }
        if (name_ == u"q" || name_ == u"quote" || name_ == u"tagline" ||
            name_ == u"moderator") {
            out_.close(tag_t::blockquote);
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == u"cut") {
            out_.close(tag_t::details);
            block_edge();
            pos_ = past;
            return true;
        }
        if (is_table_tag()) {
            out_.close(table_tag());
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_.size() == 2 && name_[0] == u'h' && name_[1] >= u'1' && name_[1] <= u'6') {
            out_.close(heading(name_[1]));
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == u"list") {
            const bool ordered = !lists_.empty() && lists_.back();
            if (!lists_.empty()) lists_.pop_back();
            out_.close(ordered ? tag_t::ol : tag_t::ul);
            block_edge();
            pos_ = past;
            return true;
        }
        return false;
    }

    // Заголовок блока жирной строкой над содержимым: автор цитаты,
    // название спойлера.
    void titled(const core::sta_u16string& title) {
        out_.open(tag_t::b);
        for (const char16_t c : title) out_.character(c);
        out_.character(u':');
        out_.close(tag_t::b);
        out_.line_break();
    }

    // Блок кода: всё до закрывающего тега буквально, теги внутри -- не
    // разметка. Язык -- имя тега ([c#]) или значение ([code=c#]); когда он
    // известен подсветке, куски кода одеваются в <span style="kw|str|com">.
    void code(std::size_t past) {
        block_edge();
        out_.open(tag_t::pre);
        pos_ = past;

        close_name_.assign(name_);
        language_.assign(value_.empty() ? std::u16string_view{name_} : std::u16string_view{value_});
        const std::u16string_view body = capture(close_name_);

        if (const highlight::language* language = highlight::find_language(language_)) {
            highlight::scanner scan(*language, body);
            while (const std::optional<highlight::piece> next = scan.next()) {
                const core::zstring_view style = code_style_name(next->kind);
                if (!style.empty()) {
                    attrs_.clear();
                    attrs_.emplace_back(attr_t::style, style);
                    out_.open(tag_t::span, out_.copy_attributes(attrs_));
                }
                for (const char16_t c : next->text) out_.character(c);
                if (!style.empty()) out_.close(tag_t::span);
            }
        } else {
            for (const char16_t c : body) out_.character(c);
        }

        out_.close(tag_t::pre);
        block_edge();
    }

    // Таблица: [t] и его полное имя [table], строки и ячейки как в HTML.
    bool is_table_tag() const noexcept {
        return name_ == u"t" || name_ == u"table" || name_ == u"tr" || name_ == u"td" ||
               name_ == u"th";
    }

    tag_t table_tag() const noexcept {
        if (name_ == u"tr") return tag_t::tr;
        if (name_ == u"td") return tag_t::td;
        if (name_ == u"th") return tag_t::th;
        return tag_t::table;
    }

    tag_t simple_tag() const noexcept {
        if (name_ == u"b") return tag_t::b;
        if (name_ == u"i") return tag_t::i;
        if (name_ == u"u") return tag_t::u;
        if (name_ == u"s") return tag_t::s;
        if (name_ == u"sub") return tag_t::sub;
        if (name_ == u"sup") return tag_t::sup;
        return tag_t::code;  // [tt]
    }

    static tag_t heading(char16_t digit) noexcept {
        // Уровни глубже третьего складываются в h3: у дерева три заголовка.
        if (digit == u'1') return tag_t::h1;
        if (digit == u'2') return tag_t::h2;
        return tag_t::h3;
    }

    void open_link(const core::sta_u16string& target) {
        attrs_.clear();
        attrs_.emplace_back(attr_t::href, out_.copy(target));
        out_.open(tag_t::a, out_.copy_attributes(attrs_));
    }

    std::u16string_view capture(std::u16string_view name) {
        const std::size_t start = pos_;
        std::size_t at = start;
        while (at < in_.size()) {
            const std::size_t open = in_.find(u'[', at);
            if (open == std::u16string_view::npos) break;
            if (matches_close(open, name)) {
                pos_ = open + name.size() + 3;
                return trimmed(in_.substr(start, open - start));
            }
            at = open + 1;
        }
        pos_ = in_.size();
        return trimmed(in_.substr(start));
    }

    bool matches_close(std::size_t at, std::u16string_view name) const {
        if (at + name.size() + 3 > in_.size()) return false;
        if (in_[at + 1] != u'/') return false;
        for (std::size_t i = 0; i < name.size(); ++i) {
            if (to_lower_ascii(in_[at + 2 + i]) != name[i]) return false;
        }
        return in_[at + 2 + name.size()] == u']';
    }

    static std::u16string_view trimmed(std::u16string_view text) {
        while (!text.empty() && is_ws(text.front())) text.remove_prefix(1);
        while (!text.empty() && is_ws(text.back())) text.remove_suffix(1);
        return text;
    }

    std::u16string_view in_;
    std::size_t pos_ = 0;
    tree_builder& out_;

    int pending_breaks_ = 0;
    bool swallow_newline_ = false;
    bool at_line_start_ = true;
    int quote_level_ = 0;
    int link_depth_ = 0;  // внутри [url=...]...[/url] адрес в тексте не ссылка

    xml::sta_vector<bool> lists_;

    core::sta_u16string name_;
    core::sta_u16string value_;
    core::sta_u16string close_name_;
    core::sta_u16string language_;
    core::sta_u16string width_;
    core::sta_u16string height_;
    xml::sta_vector<attribute_t> attrs_;
};

}  // namespace

html::document parse(std::u16string_view input) {
    html::document_builder building;
    parser reader(input, building.tree());
    reader.run();
    return std::move(building).finish();
}

html::document parse(core::u8_view input) {
    const core::u16_text text = input.to_utf16();
    return parse(text.plain());
}

}  // namespace wxl::rsdn
