module;

#include "abi.h"

// Grapheme clusters: where one user-perceived character ends and the next
// begins.
//
// A code point is what the encodings count, and a letter is often several of
// them: "и" and a combining breve, a flag of two regional indicators, a family
// emoji joined by ZWJ, a Hangul syllable spelled in jamo, a Devanagari conjunct.
// Cutting between those code points leaves the encoding intact and the letter
// broken -- a mark shaped without its base, a conjunct fallen apart. Whatever
// cuts, styles or spaces text for display asks here, not at a code point or a
// UTF-16 unit.
//
// The rules are the extended grapheme clusters of UAX #29, and the property
// table they read is generated from one pinned Unicode version by
// wxl.core/tools/gen-grapheme-table.py (run with uv). The same version's
// GraphemeBreakTest.txt is the test.
//
// Why not ICU or DirectWrite: the icu.dll in Windows appears in 1903 while the
// Windows App SDK runs on 1809, and its Unicode version follows the Windows
// build, so two machines would disagree about the same text; DirectWrite's
// clusters exist only after shaping and depend on the font. This table depends
// on nothing but std.

export module wxl.core:grapheme;

import std;

namespace wxl::core {

export {

/// Where letters begin in a stream of code points, fed one at a time.
///
/// A state machine rather than a function of two neighbours, because three of
/// the rules look further back than one code point: a flag is two regional
/// indicators and the third starts a new one (GB12, GB13), an emoji sequence
/// joins across Extend* ZWJ (GB11), and a conjunct joins a consonant to the
/// linker before it across InCB=Extend marks (GB9c).
///
/// The first code point always begins a letter. Starting over at a letter
/// boundary gives the same answers as continuing through it, which is what
/// lets the UTF-16 helpers below restart wherever a boundary is already known:
/// the only rules that look back past the start of a letter are the flag
/// rules, and a boundary inside a run of regional indicators only ever falls
/// after an even number of them -- the count a fresh start assumes.
class grapheme_breaker {
public:
    /// Whether a letter begins at this code point, given the ones fed before
    /// it. The code point need not be a scalar value: a lone surrogate from
    /// broken text is taken as a letter of its own kind, like anything the
    /// tables do not list.
    bool breaks_before(char32_t code_point) noexcept;

private:
    std::uint8_t previous_ = 0xFF;        ///< property byte of the last code point; 0xFF: none yet
    bool odd_regional_ = false;           ///< an odd run of regional indicators ends at the last one
    std::uint8_t emoji_ = 0;              ///< 1: Extended_Pictographic Extend*; 2: that, then ZWJ
    bool after_linker_ = false;           ///< InCB=Linker, then only InCB=Extend
};

/// The start of the letter after the one that starts at `at`, or the end of
/// the text. `at` has to be a letter boundary -- 0, or an offset one of these
/// functions returned; past the end, the answer is the end.
std::size_t next_grapheme_boundary(std::wstring_view text, std::size_t at) noexcept;
std::size_t next_grapheme_boundary(std::u16string_view text, std::size_t at) noexcept;

/// The nearest letter boundary at or before `at` -- the grapheme counterpart of
/// floor_code_point_boundary(), for a limit counted in units. Walks from the
/// start of the text, so it costs the length up to `at`; a caller that cuts
/// many times in one text walks with next_grapheme_boundary() instead.
std::size_t floor_grapheme_boundary(std::wstring_view text, std::size_t at) noexcept;
std::size_t floor_grapheme_boundary(std::u16string_view text, std::size_t at) noexcept;

/// The Unicode version the property table was generated from.
std::string_view grapheme_unicode_version() noexcept;

}  // export

}  // namespace wxl::core
