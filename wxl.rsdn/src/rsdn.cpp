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

constexpr bool is_ascii_letter(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

constexpr bool is_ascii_digit(wchar_t c) noexcept { return c >= L'0' && c <= L'9'; }

constexpr bool is_ws(wchar_t c) noexcept {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
}

constexpr bool is_newline(wchar_t c) noexcept { return c == L'\n' || c == L'\r'; }

constexpr wchar_t to_lower_ascii(wchar_t c) noexcept {
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
}

// Символ слова -- чтобы адрес не начинался посреди слова, а смайл посреди
// адреса.
constexpr bool is_word(wchar_t c) noexcept {
    return is_ascii_letter(c) || is_ascii_digit(c) || c == L'_' || c >= 0x80;
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
constexpr bool is_prefix_letter(wchar_t c) noexcept {
    return (c >= L'A' && c <= L'Z') || (c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё' ||
           c == L'·' || c == L'_';
}

bool one_of(std::wstring_view name, std::span<const std::wstring_view> names) noexcept {
    for (const std::wstring_view candidate : names) {
        if (name == candidate) return true;
    }
    return false;
}

// Теги кода без подсветки: сам [code] и языки, для которых у форума имя
// есть, а таблицы подсветки нет. Такой остался один -- 1С: ключевых слов
// для него нет ни у RsdnFormatter, ни у подсветки самого сайта.
bool is_plain_code_tag(std::wstring_view name) noexcept {
    constexpr std::wstring_view known[] = {L"code", L"1c"};
    return one_of(name, known);
}

// Языки, которые подсветка знает, а форум тегом -- нет: сайт красит их
// только внутри [code=lua]. Голое [lua] на форуме остаётся текстом, и у
// нас остаётся тоже -- незнакомый тег буквален.
bool is_language_without_tag(std::wstring_view name) noexcept {
    constexpr std::wstring_view known[] = {
        L"bash", L"sh",   L"shell", L"lua",      L"cmake",
        L"json", L"vbs",  L"vbscript", L"jscript", L"golang",
    };
    return one_of(name, known);
}

// Языковые теги кода: у RSDN каждый язык -- свой тег ([c#]...[/c#]), и все
// они для дерева один <pre>.
bool is_code_tag(std::wstring_view name) noexcept {
    return is_plain_code_tag(name) ||
           (!is_language_without_tag(name) && highlight::find_language(name) != nullptr);
}

// Смайлы -- картинки из smiles/ рядом с разметкой, размеры те, что рисует
// сам форум. Порядок важен: длинный код раньше короткого, иначе «:)))»
// стало бы улыбкой и двумя скобками. `not_after` -- символ, после которого
// код не смайл: «::)» на форуме так и экранируют.
struct smile_t {
    std::wstring_view code;
    std::wstring_view file;
    int width;
    int height;
    wchar_t not_after;
};

constexpr smile_t kSmiles[] = {
    {L":-)))", L"lol.gif", 15, 15, L':'},
    {L":)))", L"lol.gif", 15, 15, L':'},
    {L":-))", L"biggrin.gif", 15, 15, L':'},
    {L":))", L"biggrin.gif", 15, 15, L':'},
    {L":-)", L"smile.gif", 15, 15, L':'},
    {L":)", L"smile.gif", 15, 15, L':'},
    {L";-)", L"wink.gif", 15, 15, L';'},
    {L";o)", L"wink.gif", 15, 15, L';'},
    {L";O)", L"wink.gif", 15, 15, L';'},
    {L";)", L"wink.gif", 15, 15, L';'},
    {L":-(", L"frown.gif", 15, 15, L':'},
    {L":(", L"frown.gif", 15, 15, L':'},
    {L":-/", L"smirk.gif", 15, 15, L':'},
    {L":-\\", L"smirk.gif", 15, 15, L':'},
    {L":???:", L"confused.gif", 15, 22, 0},
    {L":up:", L"sup.gif", 15, 15, 0},
    {L":down:", L"down.gif", 15, 15, 0},
    {L":super:", L"super.gif", 26, 28, 0},
    {L":shuffle:", L"shuffle.gif", 15, 20, 0},
    {L":crash:", L"crash.gif", 30, 26, 0},
    {L":maniac:", L"maniac.gif", 70, 25, 0},
    {L":user:", L"user.gif", 40, 20, 0},
    {L":wow:", L"wow.gif", 19, 19, 0},
    {L":beer:", L"beer.gif", 57, 16, 0},
    {L":team:", L"invasion.gif", 110, 107, 0},
    {L":no:", L"no.gif", 15, 15, 0},
    {L":nopont:", L"nopont.gif", 35, 35, 0},
    {L":xz:", L"xz.gif", 37, 15, 0},
    {L":facepalm:", L"facepalm.gif", 18, 18, 0},
    {L":sarcasm:", L"sarcasm.gif", 50, 38, 0},
};

constexpr std::wstring_view kSmileDirectory = L"smiles/";

// Имя стиля куска кода: то, что RsdnBlock и тема знают как kw/str/com.
std::wstring_view code_style_name(highlight::kind_t kind) noexcept {
    switch (kind) {
    case highlight::kind_t::keyword: return L"kw";
    case highlight::kind_t::string: return L"str";
    case highlight::kind_t::comment: return L"com";
    default: return {};
    }
}

bool starts_with_ascii_nocase(std::wstring_view text, std::size_t at,
                              std::wstring_view what) noexcept {
    if (text.size() - at < what.size()) return false;
    for (std::size_t i = 0; i < what.size(); ++i) {
        if (to_lower_ascii(text[at + i]) != what[i]) return false;
    }
    return true;
}

void append_number(xml::sta_wstring& out, int number) {
    wchar_t digits[12];
    int count = 0;
    do {
        digits[count++] = static_cast<wchar_t>(L'0' + number % 10);
        number /= 10;
    } while (number != 0);
    while (count-- > 0) out += digits[count];
}

class parser {
public:
    parser(std::wstring_view input, tree_builder& out) : in_(input), out_(out) {}

    void run() {
        while (pos_ < in_.size()) {
            if (at_line_start_) line_start();
            if (pos_ >= in_.size()) break;

            const wchar_t c = in_[pos_];
            if (c == L'\\' && pos_ + 1 < in_.size() && in_[pos_ + 1] == L'[') {
                // Экранированная скобка: «\[b]» -- буквально «[b]».
                flush_breaks();
                out_.character(L'[');
                pos_ += 2;
                continue;
            }
            if (c == L'[' && tag()) continue;
            if (c == L'\r') {
                ++pos_;
                if (pos_ >= in_.size() || in_[pos_] != L'\n') newline();
                continue;
            }
            if (c == L'\n') {
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
            while (at < in_.size() && in_[at] == L'>') {
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

        constexpr std::wstring_view schemes[] = {L"http://", L"https://", L"ftp://", L"www."};
        std::size_t head = 0;
        for (const std::wstring_view scheme : schemes) {
            if (starts_with_ascii_nocase(in_, pos_, scheme)) {
                head = scheme.size();
                break;
            }
        }
        if (head == 0) return false;

        std::size_t end = pos_ + head;
        while (end < in_.size() && !is_ws(in_[end]) && in_[end] != L'[' && in_[end] != L']' &&
               in_[end] != L'<' && in_[end] != L'>' && in_[end] != L'"')
            ++end;

        // Точка в конце предложения -- не часть адреса; скобка -- если ей
        // нечего закрывать внутри.
        bool has_open_paren = false;
        for (std::size_t i = pos_; i < end; ++i) has_open_paren |= in_[i] == L'(';
        while (end > pos_ + head) {
            const wchar_t last = in_[end - 1];
            const bool punctuation = last == L'.' || last == L',' || last == L';' ||
                                     last == L':' || last == L'!' || last == L'?' ||
                                     last == L'\'' || (last == L')' && !has_open_paren);
            if (!punctuation) break;
            --end;
        }
        if (end == pos_ + head) return false;  // одна схема -- не адрес

        const std::wstring_view address = in_.substr(pos_, end - pos_);
        value_.clear();
        if (head == 4) value_.assign(L"http://");  // www. без схемы
        value_.append(address);

        flush_breaks();
        open_link(value_);
        for (const wchar_t c : address) out_.character(c);
        out_.close(tag_t::a);
        pos_ = end;
        return true;
    }

    bool starts_at(std::wstring_view what) const noexcept {
        return in_.size() - pos_ >= what.size() && in_.compare(pos_, what.size(), what) == 0;
    }

    // ---- теги ----

    bool tag() {
        std::size_t i = pos_ + 1;
        bool closing = false;
        if (i < in_.size() && in_[i] == L'/') {
            closing = true;
            ++i;
        }

        name_.clear();
        if (!closing && i < in_.size() && in_[i] == L'*') {
            name_ = L"*";
            ++i;
        } else {
            while (i < in_.size() &&
                   (is_ascii_letter(in_[i]) || is_ascii_digit(in_[i]) || in_[i] == L'#' ||
                    in_[i] == L'+' || in_[i] == L'?')) {
                name_ += to_lower_ascii(in_[i]);
                ++i;
            }
        }
        if (name_.empty()) return false;

        // Значение -- через «=» или просто через пробел: [img=small] и
        // [img small] форум понимает одинаково.
        value_.clear();
        if (!closing) {
            while (i < in_.size() && in_[i] == L' ') ++i;
            if (i < in_.size() && in_[i] == L'=') {
                ++i;
                while (i < in_.size() && in_[i] == L' ') ++i;
            }
            const bool quoted = i < in_.size() && in_[i] == L'"';
            if (quoted) ++i;
            while (i < in_.size() && in_[i] != L']' && !is_newline(in_[i])) {
                value_ += in_[i];
                ++i;
            }
            if (quoted && !value_.empty() && value_.back() == L'"') value_.pop_back();
            while (!value_.empty() && value_.back() == L' ') value_.pop_back();
        }

        if (i >= in_.size() || in_[i] != L']' || i - pos_ > 256) return false;
        const std::size_t past = i + 1;

        return closing ? close_tag(past) : open_tag(past);
    }

    bool open_tag(std::size_t past) {
        if (name_ == L"b" || name_ == L"i" || name_ == L"u" || name_ == L"s" ||
            name_ == L"sub" || name_ == L"sup" || name_ == L"tt") {
            flush_breaks();
            out_.open(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == L"url") {
            flush_breaks();
            pos_ = past;
            if (!value_.empty()) {
                open_link(value_);
                ++link_depth_;
                return true;
            }
            const std::wstring_view target = capture(L"url");
            value_.assign(target);
            open_link(value_);
            for (const wchar_t c : target) out_.character(c);
            out_.close(tag_t::a);
            return true;
        }
        if (name_ == L"email") {
            flush_breaks();
            pos_ = past;
            const std::wstring_view address = capture(L"email");
            value_.assign(L"mailto:");
            value_.append(address);
            open_link(value_);
            for (const wchar_t c : address) out_.character(c);
            out_.close(tag_t::a);
            return true;
        }
        if (name_ == L"img") {
            // Значение -- small или large; дерево размеров картинки не
            // знает, а ошибочное значение форум тоже пропускает молча.
            flush_breaks();
            pos_ = past;
            const std::wstring_view source = capture(L"img");
            value_.assign(source);
            attrs_.clear();
            attrs_.emplace_back(attr_t::src, out_.copy(value_));
            out_.open(tag_t::img, out_.copy_attributes(attrs_));
            return true;
        }
        if (name_ == L"q" || name_ == L"quote" || name_ == L"tagline" ||
            name_ == L"moderator") {
            block_edge();
            out_.open(tag_t::blockquote);
            if (!value_.empty()) titled(value_);  // [quote=автор]: автор жирной строкой
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == L"cut") {
            // Спойлер -- раскрывающийся блок дерева; заголовок свой или
            // форумный «Скрытый текст».
            block_edge();
            out_.open(tag_t::details);
            out_.open(tag_t::summary);
            if (value_.empty()) value_.assign(L"Скрытый текст");
            for (const wchar_t c : value_) out_.character(c);
            out_.close(tag_t::summary);
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == L"hr") {
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
        if (name_.size() == 2 && name_[0] == L'h' && name_[1] >= L'1' && name_[1] <= L'6') {
            block_edge();
            out_.open(heading(name_[1]));
            pos_ = past;
            return true;
        }
        if (name_ == L"list") {
            block_edge();
            const bool ordered = !value_.empty();
            lists_.push_back(ordered);
            if (ordered) {
                // Вид нумерации -- атрибут type, как у HTML; «1» -- умолчание.
                const wchar_t kind = value_.size() == 1 ? value_[0] : L'1';
                if (kind == L'a' || kind == L'A' || kind == L'i' || kind == L'I') {
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
        if (name_ == L"*") {
            block_edge();
            out_.open(tag_t::li);
            pos_ = past;
            return true;
        }
        return false;
    }

    bool close_tag(std::size_t past) {
        if (name_ == L"b" || name_ == L"i" || name_ == L"u" || name_ == L"s" ||
            name_ == L"sub" || name_ == L"sup" || name_ == L"tt") {
            flush_breaks();
            out_.close(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == L"url" || name_ == L"email") {
            flush_breaks();
            out_.close(tag_t::a);
            if (link_depth_ > 0) --link_depth_;
            pos_ = past;
            return true;
        }
        if (name_ == L"q" || name_ == L"quote" || name_ == L"tagline" ||
            name_ == L"moderator") {
            out_.close(tag_t::blockquote);
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == L"cut") {
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
        if (name_.size() == 2 && name_[0] == L'h' && name_[1] >= L'1' && name_[1] <= L'6') {
            out_.close(heading(name_[1]));
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == L"list") {
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
    void titled(const xml::sta_wstring& title) {
        out_.open(tag_t::b);
        for (const wchar_t c : title) out_.character(c);
        out_.character(L':');
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
        language_.assign(value_.empty() ? std::wstring_view{name_} : std::wstring_view{value_});
        const std::wstring_view body = capture(close_name_);

        if (const highlight::language* language = highlight::find_language(language_)) {
            highlight::scanner scan(*language, body);
            while (const std::optional<highlight::piece> next = scan.next()) {
                const std::wstring_view style = code_style_name(next->kind);
                if (!style.empty()) {
                    attrs_.clear();
                    attrs_.emplace_back(attr_t::style, style);
                    out_.open(tag_t::span, out_.copy_attributes(attrs_));
                }
                for (const wchar_t c : next->text) out_.character(c);
                if (!style.empty()) out_.close(tag_t::span);
            }
        } else {
            for (const wchar_t c : body) out_.character(c);
        }

        out_.close(tag_t::pre);
        block_edge();
    }

    // Таблица: [t] и его полное имя [table], строки и ячейки как в HTML.
    bool is_table_tag() const noexcept {
        return name_ == L"t" || name_ == L"table" || name_ == L"tr" || name_ == L"td" ||
               name_ == L"th";
    }

    tag_t table_tag() const noexcept {
        if (name_ == L"tr") return tag_t::tr;
        if (name_ == L"td") return tag_t::td;
        if (name_ == L"th") return tag_t::th;
        return tag_t::table;
    }

    tag_t simple_tag() const noexcept {
        if (name_ == L"b") return tag_t::b;
        if (name_ == L"i") return tag_t::i;
        if (name_ == L"u") return tag_t::u;
        if (name_ == L"s") return tag_t::s;
        if (name_ == L"sub") return tag_t::sub;
        if (name_ == L"sup") return tag_t::sup;
        return tag_t::code;  // [tt]
    }

    static tag_t heading(wchar_t digit) noexcept {
        // Уровни глубже третьего складываются в h3: у дерева три заголовка.
        if (digit == L'1') return tag_t::h1;
        if (digit == L'2') return tag_t::h2;
        return tag_t::h3;
    }

    void open_link(const xml::sta_wstring& target) {
        attrs_.clear();
        attrs_.emplace_back(attr_t::href, out_.copy(target));
        out_.open(tag_t::a, out_.copy_attributes(attrs_));
    }

    std::wstring_view capture(std::wstring_view name) {
        const std::size_t start = pos_;
        std::size_t at = start;
        while (at < in_.size()) {
            const std::size_t open = in_.find(L'[', at);
            if (open == std::wstring_view::npos) break;
            if (matches_close(open, name)) {
                pos_ = open + name.size() + 3;
                return trimmed(in_.substr(start, open - start));
            }
            at = open + 1;
        }
        pos_ = in_.size();
        return trimmed(in_.substr(start));
    }

    bool matches_close(std::size_t at, std::wstring_view name) const {
        if (at + name.size() + 3 > in_.size()) return false;
        if (in_[at + 1] != L'/') return false;
        for (std::size_t i = 0; i < name.size(); ++i) {
            if (to_lower_ascii(in_[at + 2 + i]) != name[i]) return false;
        }
        return in_[at + 2 + name.size()] == L']';
    }

    static std::wstring_view trimmed(std::wstring_view text) {
        while (!text.empty() && is_ws(text.front())) text.remove_prefix(1);
        while (!text.empty() && is_ws(text.back())) text.remove_suffix(1);
        return text;
    }

    std::wstring_view in_;
    std::size_t pos_ = 0;
    tree_builder& out_;

    int pending_breaks_ = 0;
    bool swallow_newline_ = false;
    bool at_line_start_ = true;
    int quote_level_ = 0;
    int link_depth_ = 0;  // внутри [url=...]...[/url] адрес в тексте не ссылка

    xml::sta_vector<bool> lists_;

    xml::sta_wstring name_;
    xml::sta_wstring value_;
    xml::sta_wstring close_name_;
    xml::sta_wstring language_;
    xml::sta_wstring width_;
    xml::sta_wstring height_;
    xml::sta_vector<attribute_t> attrs_;
};

}  // namespace

html::document parse(std::wstring_view input) {
    html::document_builder building;
    parser reader(input, building.tree());
    reader.run();
    return std::move(building).finish();
}

html::document parse(core::u8_view input) { return parse(input.to_utf16().wchars()); }

}  // namespace wxl::rsdn
