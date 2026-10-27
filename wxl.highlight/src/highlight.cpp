// The highlighting scanner: opening and closing sequences for comments and strings,
// binary search over sorted keyword sets, and word boundaries -- what RsdnFormatter
// does with .NET regular expressions and a trie.

module wxl.highlight;

import std;

namespace wxl::highlight {

namespace {

constexpr std::size_t npos = std::u16string_view::npos;

// Every keyword in the tables is shorter than this limit; a longer candidate is not compared.
constexpr std::size_t kMaxKeyword = 64;

// A word character, as `\w` in .NET: a letter, a digit or an underscore. Every non-ASCII
// character counts as a letter, so `if` right after a non-ASCII letter is not a keyword.
constexpr bool is_word(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') ||
           c == u'_' || c >= 0x80;
}

constexpr bool is_space(char16_t c) noexcept {
    return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == u'\f' || c == u'\v';
}

constexpr bool is_newline(char16_t c) noexcept { return c == u'\n' || c == u'\r'; }

constexpr bool is_hex(char16_t c) noexcept {
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
}

constexpr char16_t to_lower_ascii(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
}

bool starts_at(std::u16string_view code, std::size_t at, std::u16string_view what) noexcept {
    return at <= code.size() && code.size() - at >= what.size() &&
           code.compare(at, what.size(), what) == 0;
}

// Checks the boundary before a keyword the way TrieKeywordMatcher does.
bool boundary_before(boundary_t boundary, std::u16string_view code, std::size_t at) noexcept {
    const auto word = [&] { return at == 0 || !is_word(code[at - 1]); };
    const auto dot = [&] { return at > 0 && code[at - 1] == u'.'; };
    const auto at_sign = [&] { return at > 0 && code[at - 1] == u'@'; };
    const auto double_question = [&] {
        return at >= 2 && code[at - 2] == u'?' && code[at - 1] == u'?';
    };

    switch (boundary) {
    case boundary_t::none: return true;
    case boundary_t::word: return word();
    case boundary_t::dot: return dot();
    case boundary_t::dot_or_word: return dot() || word();
    case boundary_t::hash_with_space: {
        // A number sign, any amount of white space, then the word: `# define`.
        std::size_t i = at;
        while (i > 0 && is_space(code[i - 1])) --i;
        return i > 0 && code[i - 1] == u'#';
    }
    case boundary_t::at_or_word: return at_sign() || word();
    case boundary_t::dot_or_at_or_word: return dot() || at_sign() || word();
    case boundary_t::double_question: return double_question();
    case boundary_t::not_ampersand: return at == 0 || code[at - 1] != u'&';
    case boundary_t::word_or_double_question: return word() || double_question();
    case boundary_t::exclamation_and_word: return word();
    case boundary_t::open_paren: return at > 0 && code[at - 1] == u'(';
    }
    return true;
}

// Checks the boundary after a keyword: returns the end of the piece, or npos if there is no
// boundary.
std::size_t boundary_after(boundary_t boundary, std::u16string_view code, std::size_t end) noexcept {
    const auto word = [&] { return end >= code.size() || !is_word(code[end]); };
    const auto dot = [&] { return end < code.size() && code[end] == u'.'; };
    const auto at_sign = [&] { return end < code.size() && code[end] == u'@'; };

    switch (boundary) {
    case boundary_t::none:
    case boundary_t::open_paren:
        return end;
    case boundary_t::dot: return dot() ? end : npos;
    case boundary_t::dot_or_word: return dot() || word() ? end : npos;
    case boundary_t::at_or_word: return at_sign() || word() ? end : npos;
    case boundary_t::dot_or_at_or_word: return dot() || at_sign() || word() ? end : npos;
    case boundary_t::exclamation_and_word:
        // The exclamation mark belongs to the word (`assert!`) and is followed by a boundary.
        if (end < code.size() && code[end] == u'!' &&
            (end + 1 >= code.size() || !is_word(code[end + 1])))
            return end + 1;
        return npos;
    default:
        // word, hash_with_space, double_question, not_ampersand and
        // word_or_double_question: after the word, each of them is `\b`.
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

    // Plain text runs to the first position where another piece starts; that probe is stored
    // so that the next call does not repeat it.
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
        bool prefixed = false;
        if (!rule.prefixes.empty() && (at == 0 || !is_word(code_[at - 1]))) {
            if (rule.prefixes.find(code_[at]) != npos && starts_at(code_, at + 1, rule.open)) {
                open_at = at + 1;
                prefixed = true;
            } else if (code_[at] == u'u' && rule.prefixes.find(u'8') != npos &&
                       at + 1 < code_.size() && code_[at + 1] == u'8' &&
                       starts_at(code_, at + 2, rule.open)) {
                open_at = at + 2;
                prefixed = true;
            }
        }
        if (!prefixed && !starts_at(code_, at, rule.open)) continue;

        // The shell `#` opens a comment only at the start of a word; otherwise
        // `${name#pattern}` and `$#` would turn the rest of the line into a comment.
        if (rule.after_space && at != 0 && !is_space(code_[at - 1])) continue;

        if (rule.line_start_only) {
            if (at != 0 && code_[at - 1] != u'\n') continue;
            // A one-character opening sequence at the start of a line is Perl POD (`=head1`):
            // it counts only when a word character follows it.
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
        // To the end of the line; the line break itself is not part of the piece.
        while (i < size && !is_newline(code_[i])) ++i;
        return i;
    }

    if (rule.single_char) {
        if (i >= size || is_newline(code_[i])) return npos;
        if (rule.escape != 0 && code_[i] == rule.escape) {
            ++i;
            if (i < size && (code_[i] == u'x' || code_[i] == u'X')) {
                ++i;
                for (int digits = 0; digits < 4 && i < size && is_hex(code_[i]); ++digits) ++i;
            } else if (i < size) {
                ++i;
            }
        } else if (starts_at(code_, i, rule.close)) {
            return npos;  // `''` is empty, not a literal
        } else {
            ++i;
        }
        return starts_at(code_, i, rule.close) ? i + rule.close.size() : npos;
    }

    while (i < size) {
        const char16_t c = code_[i];
        if (!rule.multiline && is_newline(c)) return i;  // an unterminated piece ends with the line
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
    return size;  // an unterminated comment runs to the end of the code, as editors display it
}

std::optional<scanner::found> scanner::keyword_at(std::size_t at) const noexcept {
    char16_t candidate[kMaxKeyword];
    std::size_t prepared = 0;  // the number of candidate characters already filled in

    for (const keyword_set& set : language_->keywords) {
        if (!boundary_before(set.prefix, code_, at)) continue;

        const std::size_t longest = std::min({set.longest, code_.size() - at, kMaxKeyword});
        for (; prepared < longest; ++prepared) {
            const char16_t c = code_[at + prepared];
            candidate[prepared] = language_->case_insensitive ? to_lower_ascii(c) : c;
        }

        // Longer words are tried first, so that `foreach` takes precedence over `for`.
        for (std::size_t length = longest; length > 0; --length) {
            const std::u16string_view word(candidate, length);
            if (!std::ranges::binary_search(set.words, word)) continue;
            const std::size_t end = boundary_after(set.postfix, code_, at + length);
            if (end != npos) return found{end, kind_t::keyword};
        }
    }
    return std::nullopt;
}

}  // namespace wxl::highlight
