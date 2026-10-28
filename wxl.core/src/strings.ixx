module;

#include "abi.h"

// Working on text without rewriting it: trimming, splitting, comparing and
// putting pieces back together.
//
// Two rules run through all of it. Nothing here allocates unless its name says
// it hands out a string of its own -- trim() and split() answer in views into
// the text they were given, so a caller that only wanted to look pays nothing.
// And everything that touches letters is ASCII, deliberately and visibly:
// case in Unicode depends on the language (a Turkish "i" does not become "I"),
// so a library that folded case silently would be wrong in exactly the places
// nobody tests. What these are for is protocol text -- keys, units, package
// ids, XML names -- where ASCII is the whole alphabet.

export module wxl.core:strings;

import :checks;
import std;

export namespace wxl::core {

/// Space, tab, newline, vertical tab, form feed, carriage return -- the six
/// characters XML, JSON and every configuration format agree to ignore.
template <typename CharT>
constexpr bool is_ascii_space(CharT character) noexcept {
    return character == CharT{' '} || (character >= CharT{'\t'} && character <= CharT{'\r'});
}

/// Tests whether \p character is an ASCII decimal digit, `0` through `9`.
template <typename CharT>
constexpr bool is_ascii_digit(CharT character) noexcept {
    return character >= CharT{'0'} && character <= CharT{'9'};
}

/// Converts \p character to lowercase if it is an ASCII uppercase letter, and returns any
/// other character unchanged.
template <typename CharT>
constexpr CharT to_ascii_lower(CharT character) noexcept {
    return (character >= CharT{'A'} && character <= CharT{'Z'})
               ? static_cast<CharT>(character - CharT{'A'} + CharT{'a'})
               : character;
}

/// Converts \p character to uppercase if it is an ASCII lowercase letter, and returns any
/// other character unchanged.
template <typename CharT>
constexpr CharT to_ascii_upper(CharT character) noexcept {
    return (character >= CharT{'a'} && character <= CharT{'z'})
               ? static_cast<CharT>(character - CharT{'a'} + CharT{'A'})
               : character;
}

/// Whether every byte is ASCII -- the question worth asking before a
/// transcoding, because a text that answers yes converts by widening bytes and
/// needs no decoding at all.
bool is_ascii(std::string_view text) noexcept;

// ---- Trimming ----------------------------------------------------------------------

/// The text without the whitespace it starts with, as a view into it.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim_front(std::basic_string_view<CharT> text) noexcept {
    std::size_t at = 0;
    while (at != text.size() && is_ascii_space(text[at])) ++at;
    return text.substr(at);
}

/// The text without the whitespace it ends with.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim_back(std::basic_string_view<CharT> text) noexcept {
    std::size_t size = text.size();
    while (size != 0 && is_ascii_space(text[size - 1])) --size;
    return text.substr(0, size);
}

/// The text without the whitespace at either end.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim(std::basic_string_view<CharT> text) noexcept {
    return trim_back(trim_front(text));
}

/// The same three, cutting a named set of characters instead of whitespace.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim_front(
    std::basic_string_view<CharT> text,
    std::type_identity_t<std::basic_string_view<CharT>> characters) noexcept {
    const std::size_t at = text.find_first_not_of(characters);
    return at == text.npos ? text.substr(text.size()) : text.substr(at);
}

/// Returns \p text without the characters of the set \p characters that it ends with, as a
/// view into it.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim_back(
    std::basic_string_view<CharT> text,
    std::type_identity_t<std::basic_string_view<CharT>> characters) noexcept {
    const std::size_t last = text.find_last_not_of(characters);
    return last == text.npos ? text.substr(0, 0) : text.substr(0, last + 1);
}

/// Returns \p text without the characters of the set \p characters at either end, as a view
/// into it.
template <typename CharT>
constexpr std::basic_string_view<CharT> trim(
    std::basic_string_view<CharT> text,
    std::type_identity_t<std::basic_string_view<CharT>> characters) noexcept {
    return trim_back(trim_front(text, characters), characters);
}

// ---- Comparing ---------------------------------------------------------------------

/// Tests whether \p left and \p right are equal when ASCII letters are compared without
/// regard to case. All other characters must match exactly.
template <typename CharT>
constexpr bool equal_ignore_ascii_case(std::basic_string_view<CharT> left,
                                       std::basic_string_view<CharT> right) noexcept {
    if (left.size() != right.size()) return false;

    for (std::size_t at = 0; at != left.size(); ++at)
        if (to_ascii_lower(left[at]) != to_ascii_lower(right[at])) return false;

    return true;
}

/// Tests whether \p text begins with \p prefix, comparing ASCII letters without regard to
/// case.
template <typename CharT>
constexpr bool starts_with_ignore_ascii_case(std::basic_string_view<CharT> text,
                                             std::basic_string_view<CharT> prefix) noexcept {
    return text.size() >= prefix.size() &&
           equal_ignore_ascii_case(text.substr(0, prefix.size()), prefix);
}

/// Tests whether \p text ends with \p suffix, comparing ASCII letters without regard to
/// case.
template <typename CharT>
constexpr bool ends_with_ignore_ascii_case(std::basic_string_view<CharT> text,
                                           std::basic_string_view<CharT> suffix) noexcept {
    return text.size() >= suffix.size() &&
           equal_ignore_ascii_case(text.substr(text.size() - suffix.size()), suffix);
}

// ---- Case --------------------------------------------------------------------------

/// Lower-cases in place, which is what a string one owns wants; ascii_lower()
/// below is for a view one does not.
template <typename CharT, typename Traits, typename Allocator>
void make_ascii_lower(std::basic_string<CharT, Traits, Allocator>& text) noexcept {
    for (CharT& character : text) character = to_ascii_lower(character);
}

/// Converts every ASCII lowercase letter of \p text to uppercase, in place.
template <typename CharT, typename Traits, typename Allocator>
void make_ascii_upper(std::basic_string<CharT, Traits, Allocator>& text) noexcept {
    for (CharT& character : text) character = to_ascii_upper(character);
}

/// Returns a copy of \p text in which every ASCII uppercase letter is converted to
/// lowercase.
std::string ascii_lower(std::string_view text);
/// Returns a copy of \p text in which every ASCII lowercase letter is converted to
/// uppercase.
std::string ascii_upper(std::string_view text);
/// Returns a copy of \p text in which every ASCII uppercase letter is converted to
/// lowercase.
std::wstring ascii_lower(std::wstring_view text);
/// Returns a copy of \p text in which every ASCII lowercase letter is converted to
/// uppercase.
std::wstring ascii_upper(std::wstring_view text);

// ---- Splitting ---------------------------------------------------------------------

/// The pieces a separator cuts the text into, handed out one at a time as
/// views into it. Nothing is copied, and nothing is collected: a caller that
/// wants a vector builds one, and a caller that wants the third field stops
/// after three.
///
/// The count is exact -- n separators make n + 1 pieces -- so empty pieces are
/// kept, and empty text is one empty piece. Dropping them is a filter, and one
/// the library has no business applying on its own: in "a,,b" the empty field
/// is data to a CSV reader and noise to a list of names.
/// The separator is held by value, whichever of the two it is -- one character
/// or a whole word. A view into a character passed by value would outlive the
/// character, and both std::basic_string_view::find() and the size below take
/// either kind, so there is nothing to gain by storing anything else.
template <typename CharT, typename Separator>
class split_view
{
public:
    /// The type that represents a piece of the text, a view into it.
    using view = std::basic_string_view<CharT>;

    /// Constructs the sequence of pieces that \p separator cuts \p text into. The text is
    /// referenced, not copied.
    constexpr split_view(view text, Separator separator) noexcept
        : text_(text), separator_(separator) {}

    /// How much of the text the separator takes up.
    static constexpr std::size_t separator_size(const Separator& separator) noexcept {
        if constexpr (std::same_as<Separator, CharT>)
            return 1;
        else
            return separator.size();
    }

    /// Represents a forward iterator over the pieces of the text. The walk ends at
    /// `std::default_sentinel`, which `end()` returns.
    class iterator
    {
    public:
        /// The type that identifies the iterator category, `std::forward_iterator_tag`.
        using iterator_category = std::forward_iterator_tag;
        /// The type that identifies the iterator concept, `std::forward_iterator_tag`.
        using iterator_concept = std::forward_iterator_tag;
        /// The type of a piece, a view into the text.
        using value_type = view;
        /// The type that represents the distance between two iterators.
        using difference_type = std::ptrdiff_t;
        /// The type that dereferencing returns: a piece, by value.
        using reference = view;
        /// The type `void`: dereferencing returns a value, not a reference.
        using pointer = void;

        constexpr iterator() noexcept = default;

        /// Constructs an iterator over \p text, cut by \p separator, that stands on the
        /// first piece.
        constexpr iterator(view text, Separator separator) noexcept
            : text_(text), separator_(separator) {
            take();
        }

        /// Returns the current piece, as a view into the text.
        constexpr view operator*() const noexcept { return piece_; }

        /// Advances to the next piece.
        constexpr iterator& operator++() noexcept {
            take();
            return *this;
        }

        /// Advances to the next piece and returns a copy of the iterator from before the
        /// increment.
        constexpr iterator operator++(int) noexcept {
            iterator was = *this;
            ++*this;
            return was;
        }

        /// Everything past the piece just handed out and the separator that
        /// ended it. The one thing a caller cannot work out for itself once
        /// the pieces start coming, and what makes "key=value with = signs in
        /// the value" a two-line job.
        constexpr view rest() const noexcept { return text_; }

        /// Tests whether the iterator has moved past the last piece.
        constexpr bool operator==(std::default_sentinel_t) const noexcept { return done_; }

        /// Tests whether two iterators stand at the same position in the same text.
        constexpr bool operator==(const iterator& other) const noexcept {
            return done_ == other.done_ && last_ == other.last_ &&
                   text_.data() == other.text_.data() && text_.size() == other.text_.size();
        }

    private:
        /// The walk ends one step after the last piece rather than when the
        /// text runs out: the text running out is itself a piece, the empty
        /// one after the final separator.
        constexpr void take() noexcept {
            if (last_) {
                done_ = true;
                piece_ = view{};
                return;
            }

            const std::size_t at = text_.find(separator_);

            if (at == view::npos) {
                piece_ = text_;
                text_ = text_.substr(text_.size());
                last_ = true;
                return;
            }

            piece_ = text_.substr(0, at);
            text_ = text_.substr(at + separator_size(separator_));
        }

        view text_;
        Separator separator_{};
        view piece_;
        bool last_ = false;
        bool done_ = false;
    };

    /// Returns an iterator that stands on the first piece.
    constexpr iterator begin() const noexcept { return iterator(text_, separator_); }
    /// Returns the sentinel that marks the end of the pieces.
    constexpr std::default_sentinel_t end() const noexcept { return {}; }

private:
    view text_;
    Separator separator_;
};

/// The text cut on a word. The separator may not be empty -- there is no
/// answer to what an empty one would cut between.
template <typename CharT>
constexpr auto split(std::basic_string_view<CharT> text,
                     std::type_identity_t<std::basic_string_view<CharT>> separator) noexcept {
    ensure(!separator.empty());

    return split_view<CharT, std::basic_string_view<CharT>>(text, separator);
}

/// The text cut on a single character.
template <typename CharT>
constexpr auto split(std::basic_string_view<CharT> text, CharT separator) noexcept {
    return split_view<CharT, CharT>(text, separator);
}

// ---- Joining -----------------------------------------------------------------------

/// Appends the values with the separator between them. The room needed is
/// worked out first whenever the range can be walked twice, so a join of forty
/// names grows the string once.
template <typename CharT, typename Traits, typename Allocator, typename Range>
void append_joined(std::basic_string<CharT, Traits, Allocator>& out, const Range& values,
                   std::type_identity_t<std::basic_string_view<CharT>> separator) {
    using view = std::basic_string_view<CharT>;

    if constexpr (std::ranges::forward_range<const Range&>) {
        std::size_t size = 0;
        std::size_t count = 0;

        for (const auto& value : values) {
            size += view(value).size();
            ++count;
        }

        if (count != 0) out.reserve(out.size() + size + separator.size() * (count - 1));
    }

    bool first = true;

    for (const auto& value : values) {
        if (!std::exchange(first, false)) out.append(separator);
        out.append(view(value));
    }
}

/// The values with the separator between them, in a string of its own.
template <typename Range>
std::string join(const Range& values, std::string_view separator) {
    std::string result;
    append_joined(result, values, separator);
    return result;
}

/// Returns the values with \p separator between them, in a new `std::wstring`.
template <typename Range>
std::wstring join(const Range& values, std::wstring_view separator) {
    std::wstring result;
    append_joined(result, values, separator);
    return result;
}

// ---- Escaping ----------------------------------------------------------------------

/// Appends the text with the four characters that cannot stand in XML written
/// as entities: an ampersand, the two angle brackets and a double quote. That
/// is enough for both element content and an attribute value in double
/// quotes, which is how everything in this tree writes attributes.
void append_xml_escaped(std::string& out, std::string_view text);

/// The escaped text, in a string of its own.
std::string xml_escaped(std::string_view text);

}  // export namespace wxl::core
