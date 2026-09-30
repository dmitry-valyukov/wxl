// Токенизатор BB-кода над tree_builder семейства. Здесь только запись
// тегов и правила переводов строк; дерево, восстановление и пробелы --
// в wxl.html:builder.

module wxl.bb;

import std;
import wxl.core;
import wxl.html;
import wxl.xml;

namespace wxl::bb {

namespace {

using html::attr_t;
using html::attribute_t;
using html::tag_t;
using html::tree_builder;

constexpr bool is_ascii_letter(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

constexpr bool is_ascii_digit(char16_t c) noexcept { return c >= u'0' && c <= u'9'; }

constexpr char16_t to_lower_ascii(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
}

class parser {
public:
    parser(std::u16string_view input, tree_builder& out) : in_(input), out_(out) {}

    void run() {
        while (pos_ < in_.size()) {
            const char16_t c = in_[pos_];
            if (c == u'[' && tag()) continue;
            if (c == u'\r') {  // \r\n и одинокий \r — одинаково: перевод несёт \n
                ++pos_;
                if (pos_ >= in_.size() || in_[pos_] != u'\n') newline();
                continue;
            }
            if (c == u'\n') {
                newline();
                ++pos_;
                continue;
            }
            flush_breaks();
            out_.character(c);
            ++pos_;
        }
        // Переводы, повисшие в конце, не нужны никому.
    }

private:
    // ---- переводы строк ----
    //
    // Форумный пост — плоский текст: \n значим и становится <br>. Но один
    // перевод сразу за блочным тегом оформляет разметку, а не текст, — он
    // проглатывается, иначе каждый [quote] начинался бы пустой строкой.

    void newline() {
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

    // Блочная граница: висящие переводы умирают, следующий проглатывается.
    void block_edge() {
        pending_breaks_ = 0;
        swallow_newline_ = true;
    }

    // ---- теги ----

    // В pos_ стоит '['. Истина — распознанный тег, pos_ уехал за него;
    // ложь — не тег или незнакомый: '[' остаётся буквальным текстом, как
    // рисуют его сами форумы.
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
            while (i < in_.size() && (is_ascii_letter(in_[i]) || is_ascii_digit(in_[i]))) {
                name_ += to_lower_ascii(in_[i]);
                ++i;
            }
        }
        if (name_.empty()) return false;

        value_.clear();
        if (!closing && i < in_.size() && in_[i] == u'=') {
            ++i;
            // Значение в кавычках или голое — до ']'.
            const bool quoted = i < in_.size() && in_[i] == u'"';
            if (quoted) ++i;
            while (i < in_.size() && in_[i] != u']') {
                value_ += in_[i];
                ++i;
            }
            if (quoted && !value_.empty() && value_.back() == u'"') value_.pop_back();
        }

        if (i >= in_.size() || in_[i] != u']' || i - pos_ > 256) return false;
        const std::size_t past = i + 1;

        return closing ? close_tag(past) : open_tag(past);
    }

    bool open_tag(std::size_t past) {
        if (name_ == u"b" || name_ == u"i" || name_ == u"u" || name_ == u"s" ||
            name_ == u"sub" || name_ == u"sup") {
            flush_breaks();
            out_.open(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == u"color") return font_tag(attr_t::color, past);
        if (name_ == u"size") return font_tag(attr_t::size, past);
        if (name_ == u"font") return font_tag(attr_t::face, past);
        if (name_ == u"url") {
            flush_breaks();
            pos_ = past;
            if (!value_.empty()) {
                attrs_.clear();
                attrs_.emplace_back(attr_t::href, out_.copy(value_));
                out_.open(tag_t::a, out_.copy_attributes(attrs_));
                return true;
            }
            // [url]цель[/url]: содержимое — и адрес, и текст ссылки.
            const std::u16string_view target = capture(u"url");
            value_.assign(target);
            attrs_.clear();
            attrs_.emplace_back(attr_t::href, out_.copy(value_));
            out_.open(tag_t::a, out_.copy_attributes(attrs_));
            for (const char16_t c : target) out_.character(c);
            out_.close(tag_t::a);
            return true;
        }
        if (name_ == u"img") {
            flush_breaks();
            pos_ = past;
            const std::u16string_view source = capture(u"img");
            value_.assign(source);
            attrs_.clear();
            attrs_.emplace_back(attr_t::src, out_.copy(value_));
            out_.open(tag_t::img, out_.copy_attributes(attrs_));
            return true;
        }
        if (name_ == u"quote") {
            block_edge();
            out_.open(tag_t::blockquote);
            if (!value_.empty()) {
                // Автор цитаты — жирной строкой над ней.
                out_.open(tag_t::b);
                for (const char16_t c : value_) out_.character(c);
                out_.character(u':');
                out_.close(tag_t::b);
                out_.line_break();
            }
            block_edge();
            pos_ = past;
            return true;
        }
        if (name_ == u"code") {
            block_edge();
            out_.open(tag_t::pre);
            pos_ = past;
            // Всё до [/code] — буквально: теги внутри кода не разметка.
            const std::u16string_view body = capture(u"code");
            for (const char16_t c : body) out_.character(c);
            out_.close(tag_t::pre);
            block_edge();
            return true;
        }
        if (name_ == u"list") {
            block_edge();
            const bool ordered = !value_.empty();
            lists_.push_back(ordered);
            out_.open(ordered ? tag_t::ol : tag_t::ul);
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
            name_ == u"sub" || name_ == u"sup") {
            flush_breaks();
            out_.close(simple_tag());
            pos_ = past;
            return true;
        }
        if (name_ == u"color" || name_ == u"size" || name_ == u"font") {
            flush_breaks();
            out_.close(tag_t::font);
            pos_ = past;
            return true;
        }
        if (name_ == u"url") {
            flush_breaks();
            out_.close(tag_t::a);
            pos_ = past;
            return true;
        }
        if (name_ == u"quote") {
            out_.close(tag_t::blockquote);
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

    tag_t simple_tag() const noexcept {
        if (name_ == u"b") return tag_t::b;
        if (name_ == u"i") return tag_t::i;
        if (name_ == u"u") return tag_t::u;
        if (name_ == u"s") return tag_t::s;
        if (name_ == u"sub") return tag_t::sub;
        return tag_t::sup;
    }

    bool font_tag(attr_t attribute, std::size_t past) {
        flush_breaks();
        attrs_.clear();
        attrs_.emplace_back(attribute, out_.copy(value_));
        out_.open(tag_t::font, out_.copy_attributes(attrs_));
        pos_ = past;
        return true;
    }

    // Содержимое до парного закрывающего [/имя], буквально; pos_ уезжает за
    // него. Нет закрывающего — до конца входа: обрезанный пост дороже тега.
    std::u16string_view capture(std::u16string_view name) {
        const std::size_t start = pos_;
        std::size_t at = start;
        while (at < in_.size()) {
            const std::size_t open = in_.find(u'[', at);
            if (open == std::u16string_view::npos) break;
            if (matches_close(open, name)) {
                pos_ = open + name.size() + 3;  // "[/имя]"
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
        while (!text.empty() && (text.front() == u' ' || text.front() == u'\r' ||
                                 text.front() == u'\n' || text.front() == u'\t'))
            text.remove_prefix(1);
        while (!text.empty() && (text.back() == u' ' || text.back() == u'\r' ||
                                 text.back() == u'\n' || text.back() == u'\t'))
            text.remove_suffix(1);
        return text;
    }

    std::u16string_view in_;
    std::size_t pos_ = 0;
    tree_builder& out_;

    int pending_breaks_ = 0;
    bool swallow_newline_ = false;

    // Открытые списки: [list] нумерованный или нет — чтобы [/list] закрыл
    // тот тег, который открывался.
    xml::sta_vector<bool> lists_;

    core::sta_u16string name_;
    core::sta_u16string value_;
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

}  // namespace wxl::bb
