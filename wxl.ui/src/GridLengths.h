#pragma once

// The rows and the columns of a Grid, written the way XAML names them and checked when the code is
// compiled:
//
//     Grid {RowDefinitions<u"auto,*">, ColumnDefinitions<u"*,2*,120">, ...}
//
// The text is a template argument -- the same impl::values that `iterate<L"ab">` takes -- so the
// compiler parses it once, and that parse is the check: an entry that is none of the four forms, or
// an empty one, is a compile error. What the parse makes is a static array of GridLength of exactly
// the length written, one per program for each text; RowDefinitions<...> and ColumnDefinitions<...>
// are views on it. Nothing of the text is left to read while the program runs, and a Preset or a
// Template holding such a value holds a view on storage that lives as long as the program.
//
// The four forms: `*` takes a share of what is left, `N*` takes N shares, `auto` (or `Auto`) is as
// large as the content needs, a bare number is that many pixels. N is digits, with a fraction after
// a point if wanted. Spaces around an entry are allowed.
//
// Rows and columns are values of two types, RowLengths and ColumnLengths, so that a Grid knows
// which one it was given without a name (its positional route, the profile's syntheticMembers).
// How many lengths there are is not part of either type: a choice between two texts --
// `iconAfter ? ColumnDefinitions<u"*,auto"> : ColumnDefinitions<u"auto,*">` -- is a choice between
// two values of one type.
//
// Lengths a program computes are not text: they go to the named tag as GridLength values -- a span
// of them or a braced list, `rowDefinitions = lengths` -- borrowed for the expression that writes
// them (impl::grid_lengths_op below), and so refused by a Preset and a Template, as a collection list
// is. Grid objects go the way of any collection, `rowDefinitions[a, b]`.
//
// A header of its own, because the tags name the types in Members.h, which a class that includes
// Members.h could not provide.

#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <type_traits>

#include <wxl/Microsoft.UI.Xaml.Structs.h>
#include "impl/member.h"

namespace wxl {

class Grid;

namespace impl {

struct grid_lengths_access;

// What both RowLengths and ColumnLengths are: a view on the static array the compiler made from a
// text. Made only by the parse below (grid_lengths_access), so it never views anything shorter-lived.
class grid_lengths_view
{
public:
    constexpr std::span<GridLength const> lengths() const noexcept { return lengths_; }
    constexpr std::size_t size() const noexcept { return lengths_.size(); }

protected:
    constexpr explicit grid_lengths_view(std::span<GridLength const> lengths) noexcept
        : lengths_(lengths) {}

private:
    std::span<GridLength const> lengths_;
};

}  // namespace impl

/// The rows of a Grid: RowDefinitions<u"...">, written unnamed in the Grid's braces.
class RowLengths : public impl::grid_lengths_view
{
    friend struct impl::grid_lengths_access;
    using grid_lengths_view::grid_lengths_view;
};

/// The columns of a Grid: ColumnDefinitions<u"...">, written unnamed in the Grid's braces.
class ColumnLengths : public impl::grid_lengths_view
{
    friend struct impl::grid_lengths_access;
    using grid_lengths_view::grid_lengths_view;
};

namespace impl {

// Called only where a text is wrong, and not constexpr: reached while the array below is being
// evaluated, it makes the evaluation fail, and the compiler names this function.
inline void grid_lengths_entry_is_not_a_length() {}

// The same, where an entry has more digits than a double holds exactly (see the parse below).
inline void grid_lengths_entry_has_too_many_digits() {}

// The same, where the text is not a string literal (see the parse below).
inline void grid_lengths_text_is_not_a_string_literal() {}

template <typename Char>
constexpr bool grid_lengths_is_digit(Char unit) noexcept {
    return unit >= Char('0') && unit <= Char('9');
}

template <typename Char>
constexpr bool grid_lengths_is_space(Char unit) noexcept {
    return unit == Char(' ') || unit == Char('\t');
}

template <values Text>
using grid_lengths_char = typename std::remove_cvref_t<decltype(Text)>::element_t;

// How many lengths a text has: one more than its commas. The type of the array needs the number
// before the parse fills it; whether each entry is a length is the parse's to say.
template <values Text>
consteval std::size_t grid_lengths_count() {
    std::size_t count = 1;
    for (std::size_t at = 0; at < Text.count; ++at) {
        if (Text.items[at] == grid_lengths_char<Text>(',')) ++count;
    }
    return count;
}

// One entry, [first, last) with the spaces around it trimmed: its length, or the compile error.
template <typename Char>
consteval GridLength grid_lengths_parse_entry(Char const* first, Char const* last) {
    while (first != last && grid_lengths_is_space(*first)) ++first;
    while (first != last && grid_lengths_is_space(*(last - 1))) --last;

    auto const is = [&](char const* word) {
        Char const* at = first;
        for (; *word != 0; ++word, ++at) {
            if (at == last || *at != Char(*word)) return false;
        }
        return at == last;
    };
    if (is("auto") || is("Auto")) return {1.0, GridUnitType::Auto};

    bool const star = first != last && *(last - 1) == Char('*');
    if (star) --last;
    if (star && first == last) return {1.0, GridUnitType::Star};

    // Digits, then a point and digits if there is a fraction; nothing else. All the digits make one
    // whole number, divided once by ten to the power of the fraction's digits. Both are exact in a
    // double -- the number up to 2^53, the power up to 10^22, and more digits than that are the
    // compile error -- so the one division rounds correctly: "0.3" is the double nearest 0.3, as
    // std::from_chars reads it. Adding digit times a tenth each step rounds at every step and made
    // 0.30000000000000004.
    if (first == last || !grid_lengths_is_digit(*first)) grid_lengths_entry_is_not_a_length();
    unsigned long long whole = 0;
    auto const take = [&whole](Char digit) {
        whole = whole * 10 + static_cast<unsigned long long>(digit - Char('0'));
        if (whole > (1ull << 53)) grid_lengths_entry_has_too_many_digits();
    };
    for (; first != last && grid_lengths_is_digit(*first); ++first) take(*first);
    double power = 1;
    if (first != last) {
        if (*first != Char('.')) grid_lengths_entry_is_not_a_length();
        ++first;
        if (first == last) grid_lengths_entry_is_not_a_length();
        for (int fraction_digits = 1; first != last; ++first, ++fraction_digits) {
            if (!grid_lengths_is_digit(*first)) grid_lengths_entry_is_not_a_length();
            if (fraction_digits > 22) grid_lengths_entry_has_too_many_digits();
            take(*first);
            power *= 10;
        }
    }
    return {static_cast<double>(whole) / power, star ? GridUnitType::Star : GridUnitType::Pixel};
}

// The one parse of a text: each entry in turn, into the array of exactly that many lengths.
//
// The text is a string literal, and nothing else values takes. values drops the terminator a
// literal ends in from the count but keeps its slot, and gives characters one spare slot besides,
// so a literal is the one run whose capacity is two more than its count. A lone character -- '*'
// -- and an array of characters with no terminator are one more; numbers are not characters at
// all. Asked here, in the evaluation, and not as a constraint of RowDefinitions: MSVC 14.53 does
// not evaluate a constraint on the text where RowDefinitions<...> stands in a default member
// initializer -- it reports a read of an uninitialized symbol.
template <values Text>
consteval std::array<GridLength, grid_lengths_count<Text>()> grid_lengths_parse() {
    if (!is_character<grid_lengths_char<Text>> || std::size(Text.items) != Text.count + 2) {
        grid_lengths_text_is_not_a_string_literal();
    }
    std::array<GridLength, grid_lengths_count<Text>()> lengths{};
    auto const* const text = Text.items;
    std::size_t entry = 0;
    std::size_t start = 0;
    for (std::size_t at = 0; at <= Text.count; ++at) {
        if (at == Text.count || text[at] == grid_lengths_char<Text>(',')) {
            lengths[entry++] = grid_lengths_parse_entry(text + start, text + at);
            start = at + 1;
        }
    }
    return lengths;
}

// The array itself: a constant of the program, one for each text.
template <values Text>
inline constexpr std::array<GridLength, grid_lengths_count<Text>()> grid_lengths_array =
    grid_lengths_parse<Text>();

struct grid_lengths_access {
    template <typename Lengths, std::size_t Count>
    static constexpr Lengths make(std::array<GridLength, Count> const& lengths) noexcept {
        return Lengths{std::span<GridLength const>{lengths}};
    }
};

}  // namespace impl

/// The rows of a Grid, parsed from the text when the code is compiled: `RowDefinitions<u"auto,*">`.
template <impl::values Text>
inline constexpr RowLengths RowDefinitions =
    impl::grid_lengths_access::make<RowLengths>(impl::grid_lengths_array<Text>);

/// The columns of a Grid, parsed from the text when the code is compiled: `ColumnDefinitions<u"*,auto">`.
template <impl::values Text>
inline constexpr ColumnLengths ColumnDefinitions =
    impl::grid_lengths_access::make<ColumnLengths>(impl::grid_lengths_array<Text>);

namespace impl {

// The rows or the columns of a Grid from GridLength values: the same building of definitions the
// parsed text gets (impl/layout_helpers.cpp), reached from the DSL's borrowed form below.
void set_row_definitions(Grid const& grid, std::span<GridLength const> lengths);
void set_column_definitions(Grid const& grid, std::span<GridLength const> lengths);

// `rowDefinitions = lengths` or `rowDefinitions = {a, b, c}`: lengths a program computed, borrowed
// for the expression that writes them -- the braces of the Grid, or Apply. They are not copied, so a
// Preset and a Template, which apply their arguments later, refuse this form (borrows_items).
template <PropertyKey key, typename Owner = void>
struct grid_lengths_op {
    std::span<GridLength const> lengths_;

    template <typename Obj>
    void operator()(Obj const& object) const {
        check_owner<Owner, Obj>();
        if constexpr (key == PropertyKey::RowDefinitions) {
            set_row_definitions(object, lengths_);
        } else {
            static_assert(key == PropertyKey::ColumnDefinitions,
                          "wxl: lengths of a grid are its rows or its columns");
            set_column_definitions(object, lengths_);
        }
    }
};

template <PropertyKey key, typename Owner>
inline constexpr bool borrows_items<grid_lengths_op<key, Owner>> = true;

template <PropertyKey key, typename Owner>
inline constexpr bool describes<grid_lengths_op<key, Owner>> = true;

// The named tags of a Grid's rows and columns: the lengths a program computed, borrowed, and nothing
// else. A text is not assigned to them: RowDefinitions<...> and ColumnDefinitions<...> are written
// unnamed in the Grid's braces.
template <PropertyKey key, typename Owner>
struct grid_lengths_tag {
    constexpr grid_lengths_op<key, Owner> operator=(std::span<GridLength const> lengths) const {
        return {lengths};
    }

    constexpr grid_lengths_op<key, Owner> operator=(
        std::initializer_list<GridLength> lengths) const {
        return {{lengths.begin(), lengths.size()}};
    }
};

template <PropertyKey key, typename Owner>
struct property_tag<key, RowLengths, Owner> : grid_lengths_tag<key, Owner> {
    using grid_lengths_tag<key, Owner>::operator=;
};

template <PropertyKey key, typename Owner>
struct property_tag<key, ColumnLengths, Owner> : grid_lengths_tag<key, Owner> {
    using grid_lengths_tag<key, Owner>::operator=;
};

}  // namespace impl

}  // namespace wxl
