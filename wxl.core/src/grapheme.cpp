module wxl.core;

import std;

namespace wxl::core {
namespace {

#include "grapheme_data.inc"

// The numbering the generator writes. It is spelled out twice -- here and in
// GCB / INCB of gen-grapheme-table.py -- and GraphemeBreakTest.txt is what
// notices when the two stop agreeing.
enum class break_class : std::uint8_t {
    other,
    cr,
    lf,
    control,
    extend,
    zwj,
    regional_indicator,
    prepend,
    spacing_mark,
    l,
    v,
    t,
    lv,
    lvt,
};

enum class conjunct_class : std::uint8_t { none, linker, consonant, extend };

constexpr std::uint8_t extended_pictographic = 0x40;

constexpr std::uint8_t no_code_point_yet = 0xFF;

/// Two loads and no branch on the way: the index names the page, the page
/// holds the byte. Past U+10FFFF there is nothing to look up, and a code point
/// from nowhere is a letter of its own like anything unlisted.
constexpr std::uint8_t properties_of(const char32_t code_point) noexcept {
    if (code_point > 0x10FFFF) return 0;

    const std::size_t page = grapheme_page_index[code_point >> grapheme_page_shift];
    const std::size_t within = code_point & ((1u << grapheme_page_shift) - 1);

    return grapheme_pages[(page << grapheme_page_shift) + within];
}

constexpr bool is_control(const break_class value) noexcept {
    return value == break_class::control || value == break_class::cr || value == break_class::lf;
}

/// UAX #29, section 3.1.1, in rule order: the first rule that matches decides.
/// GB1 and GB2 -- the ends of the text -- are the caller's.
constexpr bool breaks(const break_class previous, const break_class current,
                      const conjunct_class conjunct, const bool pictographic,
                      const bool odd_regional, const std::uint8_t emoji,
                      const bool after_linker) noexcept {
    using enum break_class;

    if (previous == cr && current == lf) return false;                         // GB3
    if (is_control(previous)) return true;                                      // GB4
    if (is_control(current)) return true;                                       // GB5

    if (previous == l && (current == l || current == v || current == lv || current == lvt))
        return false;                                                           // GB6
    if ((previous == lv || previous == v) && (current == v || current == t))
        return false;                                                           // GB7
    if ((previous == lvt || previous == t) && current == t) return false;       // GB8

    if (current == extend || current == zwj) return false;                      // GB9
    if (current == spacing_mark) return false;                                  // GB9a
    if (previous == prepend) return false;                                      // GB9b

    if (conjunct == conjunct_class::consonant && after_linker) return false;    // GB9c
    if (pictographic && emoji == 2) return false;                               // GB11

    if (previous == regional_indicator && current == regional_indicator && odd_regional)
        return false;                                                           // GB12, GB13

    return true;                                                                // GB999
}

/// One code point of UTF-16 at `at`, stepping past it. A lone surrogate is
/// handed back as itself: the grapheme table does not list it, so it is a
/// letter of its own, and a caller cutting broken text still gets offsets on
/// unit boundaries.
template <typename CharT>
char32_t decode(const std::basic_string_view<CharT> text, std::size_t& at) noexcept {
    const auto unit = static_cast<char32_t>(static_cast<std::uint16_t>(text[at++]));

    if (is_high_surrogate(unit) && at < text.size()) {
        const auto next = static_cast<char32_t>(static_cast<std::uint16_t>(text[at]));

        if (is_low_surrogate(next)) {
            ++at;
            return 0x10000 + ((unit - 0xD800) << 10) + (next - 0xDC00);
        }
    }

    return unit;
}

template <typename CharT>
std::size_t next_boundary(const std::basic_string_view<CharT> text, std::size_t at) noexcept {
    if (at >= text.size()) return text.size();

    grapheme_breaker breaker;
    breaker.breaks_before(decode(text, at));

    while (at < text.size()) {
        std::size_t after = at;

        if (breaker.breaks_before(decode(text, after))) return at;

        at = after;
    }

    return text.size();
}

template <typename CharT>
std::size_t floor_boundary(const std::basic_string_view<CharT> text, const std::size_t at) noexcept {
    if (at >= text.size()) return text.size();

    grapheme_breaker breaker;
    std::size_t letter = 0;

    for (std::size_t position = 0; position <= at;) {
        std::size_t after = position;

        if (breaker.breaks_before(decode(text, after))) letter = position;

        position = after;
    }

    return letter;
}

}  // namespace

bool grapheme_breaker::breaks_before(const char32_t code_point) noexcept {
    const std::uint8_t properties = properties_of(code_point);
    const auto current = static_cast<break_class>(properties & 0x0F);
    const auto conjunct = static_cast<conjunct_class>((properties >> 4) & 0x03);
    const bool pictographic = (properties & extended_pictographic) != 0;

    const bool result = previous_ == no_code_point_yet ||                       // GB1
                        breaks(static_cast<break_class>(previous_), current, conjunct,
                               pictographic, odd_regional_, emoji_, after_linker_);

    // A run of regional indicators pairs up from its start, so what matters is
    // whether the run so far is odd.
    const bool regional = current == break_class::regional_indicator;
    const bool after_regional = previous_ == std::to_underlying(break_class::regional_indicator);
    odd_regional_ = regional && !(after_regional && odd_regional_);

    // GB11 joins across Extended_Pictographic Extend* ZWJ, and nothing else
    // may come between.
    if (pictographic)
        emoji_ = 1;
    else if (emoji_ == 1 && current == break_class::zwj)
        emoji_ = 2;
    else if (!(emoji_ == 1 && current == break_class::extend))
        emoji_ = 0;

    after_linker_ = conjunct == conjunct_class::linker ||
                    (after_linker_ && conjunct == conjunct_class::extend);

    previous_ = static_cast<std::uint8_t>(current);

    return result;
}

std::size_t next_grapheme_boundary(const std::wstring_view text, const std::size_t at) noexcept {
    return next_boundary(text, at);
}

std::size_t next_grapheme_boundary(const std::u16string_view text, const std::size_t at) noexcept {
    return next_boundary(text, at);
}

std::size_t next_grapheme_boundary(const u16_view text, const std::size_t at) noexcept {
    return next_boundary(text.plain(), at);
}

std::size_t floor_grapheme_boundary(const std::wstring_view text, const std::size_t at) noexcept {
    return floor_boundary(text, at);
}

std::size_t floor_grapheme_boundary(const std::u16string_view text, const std::size_t at) noexcept {
    return floor_boundary(text, at);
}

std::size_t floor_grapheme_boundary(const u16_view text, const std::size_t at) noexcept {
    return floor_boundary(text.plain(), at);
}

std::string_view grapheme_unicode_version() noexcept {
    return grapheme_table_version;
}

}  // namespace wxl::core
