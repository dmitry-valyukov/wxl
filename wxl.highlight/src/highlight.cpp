// Сканер подсветки: правила «открыл -- закрыл» для комментариев и строк,
// двоичный поиск по отсортированным наборам ключевых слов, границы слов --
// то, что у RsdnFormatter делали регулярные выражения .NET и trie.

module wxl.highlight;

import std;

namespace wxl::highlight {

namespace {

constexpr std::size_t npos = std::wstring_view::npos;

// Самое длинное ключевое слово в таблицах короче; кандидат длиннее и
// сравнивать незачем.
constexpr std::size_t kMaxKeyword = 64;

// Символ слова -- как \w у .NET: буквы, цифры, подчёркивание. Всё не-ASCII
// считается буквой: ключевое слово внутри «переменнаяif» -- не слово.
constexpr bool is_word(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
           c == L'_' || c >= 0x80;
}

constexpr bool is_space(wchar_t c) noexcept {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\f' || c == L'\v';
}

constexpr bool is_newline(wchar_t c) noexcept { return c == L'\n' || c == L'\r'; }

constexpr bool is_hex(wchar_t c) noexcept {
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
}

constexpr wchar_t to_lower_ascii(wchar_t c) noexcept {
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
}

bool starts_at(std::wstring_view code, std::size_t at, std::wstring_view what) noexcept {
    return at <= code.size() && code.size() - at >= what.size() &&
           code.compare(at, what.size(), what) == 0;
}

// Граница перед ключевым словом, как её проверяет TrieKeywordMatcher.
bool boundary_before(boundary_t boundary, std::wstring_view code, std::size_t at) noexcept {
    const auto word = [&] { return at == 0 || !is_word(code[at - 1]); };
    const auto dot = [&] { return at > 0 && code[at - 1] == L'.'; };
    const auto at_sign = [&] { return at > 0 && code[at - 1] == L'@'; };
    const auto double_question = [&] {
        return at >= 2 && code[at - 2] == L'?' && code[at - 1] == L'?';
    };

    switch (boundary) {
    case boundary_t::none: return true;
    case boundary_t::word: return word();
    case boundary_t::dot: return dot();
    case boundary_t::dot_or_word: return dot() || word();
    case boundary_t::hash_with_space: {
        // Решётка, потом сколько угодно пробелов, потом слово: # define.
        std::size_t i = at;
        while (i > 0 && is_space(code[i - 1])) --i;
        return i > 0 && code[i - 1] == L'#';
    }
    case boundary_t::at_or_word: return at_sign() || word();
    case boundary_t::dot_or_at_or_word: return dot() || at_sign() || word();
    case boundary_t::double_question: return double_question();
    case boundary_t::not_ampersand: return at == 0 || code[at - 1] != L'&';
    case boundary_t::word_or_double_question: return word() || double_question();
    case boundary_t::exclamation_and_word: return word();
    case boundary_t::open_paren: return at > 0 && code[at - 1] == L'(';
    }
    return true;
}

// Граница после: где кончается кусок, или npos, если границы нет.
std::size_t boundary_after(boundary_t boundary, std::wstring_view code, std::size_t end) noexcept {
    const auto word = [&] { return end >= code.size() || !is_word(code[end]); };
    const auto dot = [&] { return end < code.size() && code[end] == L'.'; };
    const auto at_sign = [&] { return end < code.size() && code[end] == L'@'; };

    switch (boundary) {
    case boundary_t::none:
    case boundary_t::open_paren:
        return end;
    case boundary_t::dot: return dot() ? end : npos;
    case boundary_t::dot_or_word: return dot() || word() ? end : npos;
    case boundary_t::at_or_word: return at_sign() || word() ? end : npos;
    case boundary_t::dot_or_at_or_word: return dot() || at_sign() || word() ? end : npos;
    case boundary_t::exclamation_and_word:
        // Восклицательный знак -- часть слова: assert!, и за ним граница.
        if (end < code.size() && code[end] == L'!' &&
            (end + 1 >= code.size() || !is_word(code[end + 1])))
            return end + 1;
        return npos;
    default:
        // word, hash_with_space, double_question, not_ampersand,
        // word_or_double_question -- после слова все они просто \b.
        return word() ? end : npos;
    }
}

}  // namespace

std::optional<piece> scanner::next() noexcept {
    const std::size_t size = code_.size();
    if (pos_ >= size) return std::nullopt;

    if (const std::optional<found> here = probe(pos_)) {
        const piece result{code_.substr(pos_, here->end - pos_), here->kind};
        pos_ = here->end;
        return result;
    }

    // Обычный текст -- до первой точки, где начинается что-то другое; эта
    // проба запоминается, чтобы следующий вызов не делал её заново.
    std::size_t end = pos_ + 1;
    while (end < size) {
        if (std::optional<found> ahead = probe(end)) {
            probed_at_ = end;
            probed_ = ahead;
            break;
        }
        ++end;
    }

    const piece result{code_.substr(pos_, end - pos_), kind_t::plain};
    pos_ = end;
    return result;
}

std::optional<scanner::found> scanner::probe(std::size_t at) const noexcept {
    if (at == probed_at_) return probed_;
    if (std::optional<found> delimited = delimited_at(at)) return delimited;
    return keyword_at(at);
}

std::optional<scanner::found> scanner::delimited_at(std::size_t at) const noexcept {
    for (const delimited_rule& rule : language_->delimiters) {
        std::size_t open_at = at;
        if (!rule.prefixes.empty() && rule.prefixes.find(code_[at]) != npos &&
            starts_at(code_, at + 1, rule.open)) {
            open_at = at + 1;
        } else if (!starts_at(code_, at, rule.open)) {
            continue;
        }

        // Решётка shell: комментарий она открывает только с начала слова,
        // иначе ${имя#хвост} и $# съели бы остаток строки.
        if (rule.after_space && at != 0 && !is_space(code_[at - 1])) continue;

        if (rule.line_start_only) {
            if (at != 0 && code_[at - 1] != L'\n') continue;
            // Односимвольный открыватель в начале строки -- POD в Perl (=head1):
            // без буквы за ним это не он.
            if (rule.open.size() == 1) {
                const std::size_t after = open_at + 1;
                if (after >= code_.size() || !is_word(code_[after])) continue;
            }
        }

        const std::size_t end = delimited_end(rule, open_at + rule.open.size());
        if (end == npos) continue;
        return found{end, rule.kind};
    }
    return std::nullopt;
}

std::size_t scanner::delimited_end(const delimited_rule& rule, std::size_t at) const noexcept {
    const std::size_t size = code_.size();
    std::size_t i = at;

    if (rule.close.empty()) {
        // До конца строки; сам перевод строки -- не комментарий.
        while (i < size && !is_newline(code_[i])) ++i;
        return i;
    }

    if (rule.single_char) {
        if (i >= size || is_newline(code_[i])) return npos;
        if (rule.escape != 0 && code_[i] == rule.escape) {
            ++i;
            if (i < size && (code_[i] == L'x' || code_[i] == L'X')) {
                ++i;
                for (int digits = 0; digits < 4 && i < size && is_hex(code_[i]); ++digits) ++i;
            } else if (i < size) {
                ++i;
            }
        } else if (starts_at(code_, i, rule.close)) {
            return npos;  // '' -- пусто, не литерал
        } else {
            ++i;
        }
        return starts_at(code_, i, rule.close) ? i + rule.close.size() : npos;
    }

    while (i < size) {
        const wchar_t c = code_[i];
        if (!rule.multiline && is_newline(c)) return i;  // незакрытая кончается со строкой
        if (rule.escape != 0 && c == rule.escape) {
            i = std::min(i + 2, size);
            continue;
        }
        if (starts_at(code_, i, rule.close)) {
            if (rule.doubled_close && starts_at(code_, i + rule.close.size(), rule.close)) {
                i += 2 * rule.close.size();
                continue;
            }
            return i + rule.close.size();
        }
        ++i;
    }
    return size;  // незакрытый комментарий -- до конца, как рисует его редактор
}

std::optional<scanner::found> scanner::keyword_at(std::size_t at) const noexcept {
    wchar_t candidate[kMaxKeyword];
    std::size_t prepared = 0;  // сколько символов кандидата уже уложено

    for (const keyword_set& set : language_->keywords) {
        if (!boundary_before(set.prefix, code_, at)) continue;

        const std::size_t longest = std::min({set.longest, code_.size() - at, kMaxKeyword});
        for (; prepared < longest; ++prepared) {
            const wchar_t c = code_[at + prepared];
            candidate[prepared] = language_->case_insensitive ? to_lower_ascii(c) : c;
        }

        // Самое длинное слово первым: у «foreach» не должен побеждать «for».
        for (std::size_t length = longest; length > 0; --length) {
            const std::wstring_view word(candidate, length);
            if (!std::ranges::binary_search(set.words, word)) continue;
            const std::size_t end = boundary_after(set.postfix, code_, at + length);
            if (end != npos) return found{end, kind_t::keyword};
        }
    }
    return std::nullopt;
}

}  // namespace wxl::highlight
