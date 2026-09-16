module;

#include "abi.h"

// Unicode: the type of checked text, the three encodings, and the traffic
// between them.
//
// UTF-8 is what this tree keeps text in -- wxl.xml parses it, FB3 stores it,
// reading positions are counted in it -- and UTF-16 is what Windows speaks, so
// the pair below is the one that gets used. UTF-32 appears only as char32_t,
// the type a single code point travels in.
//
// Everything that decodes takes well-formed text, and says so in the signature
// rather than in a comment: a transcoder that also had to answer "and what if
// it is broken?" would carry a branch on every code point for a question the
// caller settled long before. basic_text is that contract written where the
// compiler reads it, and u8_view / u16_view are the two shapes text arrives in.
//
// The units underneath are char8_t and char16_t, the two types whose whole job
// is to name an encoding. Nothing else in the tree speaks them and nothing has
// to: chars(), wchars() and c_str() hand back the ordinary std::string_view,
// std::wstring_view and const wchar_t* that fmt, the file APIs and Windows
// take, so the reinterpretation lives here instead of at every boundary.
//
// Three doors in and no others. A literal is checked where it is written --
// u8"..." and u"..." convert on their own, at compile time. checked() walks
// text that came from outside. assume_valid() is for text somebody else has
// already checked -- a document wxl.xml validated whole -- and its name is the
// word a reader greps for on the day the guarantee turns out to be false.
//
// Text assembled piece by piece needs none of the three: a string grows by
// code points, which it checks are scalar values, and by text that is checked
// already, and it is cut only between code points.

export module wxl.core:unicode;

import :compressed_optional;
import std;

namespace wxl::core {

export template <typename T>
class basic_text;

namespace impl {

/// The key to basic_text's constructor, deliberately not exported: a type
/// whose invariant any caller can set aside in passing is a comment with extra
/// syntax. Inside the module it costs nothing, and outside it there is no way
/// to name it, which leaves the three doors above.
struct validated {
    explicit validated() = default;
};

// Eight bytes at a time, for the scan below. Real text is mostly ASCII even
// when it is Russian -- the spaces, the punctuation and the digits are -- and a
// word with no high bit set needs no decoding at all.
inline constexpr std::uint64_t high_bits = 0x8080808080808080ull;
inline constexpr std::size_t word_size = sizeof(std::uint64_t);

inline std::uint64_t load_word(const void* at) noexcept {
    std::uint64_t word = 0;
    std::memcpy(&word, at, word_size);
    return word;
}

/// The smallest code point each sequence length is allowed to carry. An
/// over-long encoding decodes to something legitimate, which is exactly why it
/// has to be refused: two spellings of one character are two ways past any
/// check that compares spellings.
inline constexpr char32_t smallest_for[5] = {0, 0, 0x80, 0x800, 0x10000};

// The transcoders and the measuring walks, on plain bytes. They are declared
// here rather than below because basic_text's own methods call them, and the
// public shapes that take a basic_text cannot be declared before the type
// they take. Definitions are in unicode.cpp.
std::size_t utf16_size(std::string_view utf8) noexcept;
std::size_t utf8_size(std::wstring_view utf16) noexcept;
std::size_t code_point_count(std::string_view utf8) noexcept;
wchar_t* write_utf16_to(wchar_t* out, std::string_view utf8) noexcept;
char* write_utf8_to(char* out, std::wstring_view utf16) noexcept;

}  // namespace impl

export {

/// The largest code point Unicode defines.
inline constexpr char32_t max_code_point = 0x10FFFF;

/// U+FFFD, what text that has to survive broken input puts in its place.
inline constexpr char32_t replacement_character = 0xFFFD;

/// True for the 2048 code points UTF-16 reserves to build surrogate pairs
/// from; they are not characters, so no encoding may carry one on its own.
constexpr bool is_surrogate(char32_t code_point) noexcept {
    return (code_point & 0xFFFFF800u) == 0xD800u;
}

/// The first half of a surrogate pair, U+D800..U+DBFF.
constexpr bool is_high_surrogate(char32_t code_point) noexcept {
    return (code_point & 0xFFFFFC00u) == 0xD800u;
}

/// The second half of a surrogate pair, U+DC00..U+DFFF.
constexpr bool is_low_surrogate(char32_t code_point) noexcept {
    return (code_point & 0xFFFFFC00u) == 0xDC00u;
}

/// A code point that may actually be encoded: in range, and not one of the
/// halves of a surrogate pair. This is the precondition stated by every
/// function here that takes a char32_t.
constexpr bool is_scalar_value(char32_t code_point) noexcept {
    return code_point <= max_code_point && !is_surrogate(code_point);
}

/// How many bytes UTF-8 spells this code point in, 1 to 4.
constexpr int utf8_size(char32_t code_point) noexcept {
    ensure(is_scalar_value(code_point));

    return code_point < 0x80 ? 1 : code_point < 0x800 ? 2 : code_point < 0x10000 ? 3 : 4;
}

/// How many units UTF-16 spells this code point in: two above the basic plane,
/// where it becomes a surrogate pair, and one everywhere else.
constexpr int utf16_size(char32_t code_point) noexcept {
    ensure(is_scalar_value(code_point));

    return code_point < 0x10000 ? 1 : 2;
}

/// True for a byte that continues a multi-byte sequence (10xxxxxx) rather than
/// starting one. No ASCII byte is ever one, which is what lets a byte-oriented
/// scanner walk UTF-8 text of any language without decoding it.
constexpr bool is_continuation(char byte) noexcept {
    return (static_cast<unsigned char>(byte) & 0xC0u) == 0x80u;
}

/// The length of the sequence this lead byte starts, 1 to 4, or 0 if the byte
/// cannot start one at all.
constexpr int utf8_sequence_size(char byte) noexcept {
    const auto lead = static_cast<unsigned char>(byte);

    if (lead < 0x80) return 1;
    if ((lead & 0xE0u) == 0xC0u) return 2;
    if ((lead & 0xF0u) == 0xE0u) return 3;
    if ((lead & 0xF8u) == 0xF0u) return 4;

    return 0;
}

// ---- Checking text that came from outside ------------------------------------------

}  // export

namespace impl {

/// The walk both spellings of UTF-8 share -- a literal is char8_t and
/// everything else in the tree is char. constexpr because a literal is checked
/// where it is written; the eight-byte stride is what a run-time call skips
/// ahead with, and a constant evaluation walks the same bytes one at a time.
template <typename CharT>
constexpr nullable<std::size_t> scan_utf8(std::basic_string_view<CharT> text) noexcept {
    std::size_t at = 0;

    while (at != text.size()) {
        if !consteval {
            while (text.size() - at >= word_size && (load_word(text.data() + at) & high_bits) == 0)
                at += word_size;
        }

        while (at != text.size() && static_cast<unsigned char>(text[at]) < 0x80) ++at;

        if (at == text.size()) break;

        const int size = utf8_sequence_size(static_cast<char>(text[at]));

        if (size == 0 || text.size() - at < static_cast<std::size_t>(size)) return at;

        char32_t code_point = static_cast<unsigned char>(text[at]) & (0xFFu >> (size + 1));

        for (int index = 1; index != size; ++index) {
            const auto byte = static_cast<unsigned char>(text[at + static_cast<std::size_t>(index)]);

            if ((byte & 0xC0u) != 0x80u) return at;

            code_point = (code_point << 6) | (byte & 0x3Fu);
        }

        if (size == 1) code_point = static_cast<unsigned char>(text[at]);

        if (code_point < smallest_for[size] || !is_scalar_value(code_point)) return at;

        at += static_cast<std::size_t>(size);
    }

    return {};
}

/// The same for UTF-16, where an unpaired surrogate is the only way to be
/// broken. No stride: the walk is already one unit at a time.
template <typename CharT>
constexpr nullable<std::size_t> scan_utf16(std::basic_string_view<CharT> text) noexcept {
    for (std::size_t at = 0; at != text.size(); ++at) {
        const auto unit = static_cast<char32_t>(static_cast<std::uint16_t>(text[at]));

        if (!is_surrogate(unit)) continue;

        // A pair, or nothing: a low surrogate on its own has no first half to
        // belong to, and a high one at the very end has no second.
        if (!is_high_surrogate(unit) || at + 1 == text.size()) return at;

        const auto next = static_cast<char32_t>(static_cast<std::uint16_t>(text[at + 1]));

        if (!is_low_surrogate(next)) return at;

        ++at;
    }

    return {};
}

/// One step back from the second half of a pair, and nowhere otherwise.
template <typename CharT>
constexpr std::size_t floor_utf16_boundary(std::basic_string_view<CharT> text,
                                           std::size_t at) noexcept {
    if (at >= text.size()) return text.size();

    if (at == 0 || !is_low_surrogate(static_cast<char32_t>(static_cast<std::uint16_t>(text[at]))))
        return at;

    const auto before = static_cast<char32_t>(static_cast<std::uint16_t>(text[at - 1]));

    return is_high_surrogate(before) ? at - 1 : at;
}

}  // namespace impl

export {

/// The offset of the first unit that breaks UTF-8, or nothing if the whole text
/// is UTF-8. Over-long forms, surrogates and anything above U+10FFFF count as
/// breaks: each of them decodes to something that looks legitimate, which is
/// precisely why it has to be refused rather than accepted.
constexpr nullable<std::size_t> find_invalid_utf8(const std::string_view text) noexcept {
    return impl::scan_utf8(text);
}

constexpr nullable<std::size_t> find_invalid_utf8(const std::u8string_view text) noexcept {
    return impl::scan_utf8(text);
}

/// Whether the text is UTF-8 -- the same walk, for the caller with nothing to
/// say about where the break was.
constexpr bool is_valid_utf8(const std::string_view text) noexcept {
    return !impl::scan_utf8(text).has_value();
}

/// The offset of the first unit that breaks UTF-16 -- an unpaired surrogate,
/// which is the only way UTF-16 can be broken -- or nothing if the text is
/// UTF-16. Windows hands out such text: a file name is a sequence of 16-bit
/// units, and nothing on the way in makes it well-formed.
constexpr nullable<std::size_t> find_invalid_utf16(const std::wstring_view text) noexcept {
    return impl::scan_utf16(text);
}

constexpr nullable<std::size_t> find_invalid_utf16(const std::u16string_view text) noexcept {
    return impl::scan_utf16(text);
}

/// Whether the text is UTF-16.
constexpr bool is_valid_utf16(const std::wstring_view text) noexcept {
    return !impl::scan_utf16(text).has_value();
}

/// The nearest offset at or before `at` where the text may be cut: `at` itself,
/// unless it falls between the halves of a surrogate pair, and then the start
/// of the pair. An offset past the end answers the end.
///
/// A limit counted in units -- sixty of them for a hint, a run no longer than
/// a buffer holds -- lands inside a pair as readily as anywhere else, and
/// neither half is a character on its own: a font draws a box for it, a
/// transcoder writes U+FFFD. Stepping back rather than forward keeps the part
/// before the cut within the limit it was cut to. A lone surrogate has no
/// partner to stay with, so a cut beside it stays where it was asked for.
///
/// Checked text meets the same question in u16_view::substr, which refuses
/// such a cut instead of moving it: its offsets come from a walk of its own.
constexpr std::size_t floor_code_point_boundary(const std::wstring_view text,
                                                const std::size_t at) noexcept {
    return impl::floor_utf16_boundary(text, at);
}

constexpr std::size_t floor_code_point_boundary(const std::u16string_view text,
                                                const std::size_t at) noexcept {
    return impl::floor_utf16_boundary(text, at);
}

}  // export

namespace impl {

// One code point onto the end of a string of UTF-8 or UTF-16 units, whatever
// the unit type is called. The callers check that it is a scalar value.

template <typename String>
void push_utf8(String& out, const char32_t code_point) {
    using unit = typename String::value_type;

    if (code_point < 0x80) {
        out.push_back(static_cast<unit>(code_point));
    } else if (code_point < 0x800) {
        out.push_back(static_cast<unit>(0xC0u | (code_point >> 6)));
        out.push_back(static_cast<unit>(0x80u | (code_point & 0x3Fu)));
    } else if (code_point < 0x10000) {
        out.push_back(static_cast<unit>(0xE0u | (code_point >> 12)));
        out.push_back(static_cast<unit>(0x80u | ((code_point >> 6) & 0x3Fu)));
        out.push_back(static_cast<unit>(0x80u | (code_point & 0x3Fu)));
    } else {
        out.push_back(static_cast<unit>(0xF0u | (code_point >> 18)));
        out.push_back(static_cast<unit>(0x80u | ((code_point >> 12) & 0x3Fu)));
        out.push_back(static_cast<unit>(0x80u | ((code_point >> 6) & 0x3Fu)));
        out.push_back(static_cast<unit>(0x80u | (code_point & 0x3Fu)));
    }
}

template <typename String>
void push_utf16(String& out, const char32_t code_point) {
    using unit = typename String::value_type;

    if (code_point < 0x10000) {
        out.push_back(static_cast<unit>(code_point));
    } else {
        const char32_t rest = code_point - 0x10000;
        out.push_back(static_cast<unit>(0xD800u + (rest >> 10)));
        out.push_back(static_cast<unit>(0xDC00u + (rest & 0x3FFu)));
    }
}

}  // namespace impl

// ---- The type ----------------------------------------------------------------------

export {

/// Well-formed Unicode text, in whichever string type holds it.
///
/// Only the two shapes below exist -- a view of text somebody else owns, and a
/// string that owns its own -- because those are the two shapes text travels
/// in. The primary template is left undefined so that basic_text of anything
/// else fails to compile rather than quietly meaning something.
template <typename T>
class basic_text;

/// UTF-8, the encoding this tree keeps text in.
using u8_view = basic_text<std::u8string_view>;
using u8_text = basic_text<std::u8string>;

/// UTF-16, the encoding Windows speaks.
using u16_view = basic_text<std::u16string_view>;
using u16_text = basic_text<std::u16string>;

/// A view of text known to be well-formed. It borrows like any other view, so
/// the text it points into has to outlive it.
///
/// Nothing here can change the text, and that is the point: inheriting from a
/// view would have brought remove_prefix() along with it, and a prefix removed
/// mid-sequence would leave the guarantee attached to text that no longer
/// deserves it.
template <typename CharT, typename Traits>
class basic_text<std::basic_string_view<CharT, Traits>> {
public:
    using value_type = CharT;
    using plain_type = std::basic_string_view<CharT, Traits>;

    static constexpr bool is_utf8 = std::same_as<CharT, char8_t>;
    static constexpr bool is_utf16 = std::same_as<CharT, char16_t>;

    static_assert(is_utf8 || is_utf16,
                  "basic_text holds char8_t or char16_t: the units whose type names the "
                  "encoding. chars(), wchars() and c_str() are how it reaches the rest of "
                  "the world");

    /// Empty text is well-formed in every encoding, so the default is as valid
    /// as anything else.
    constexpr basic_text() noexcept = default;

    constexpr basic_text(plain_type text, impl::validated) noexcept : text_(text) {}

    /// A literal, checked where it is written: u8"Поток №1" is a u8_view and a
    /// mis-encoded one is a compilation error, not a call that has to happen
    /// first. consteval and nothing else -- a run-time array cannot reach this
    /// constructor at all, so nothing slips through it unchecked.
    ///
    /// The trailing null the literal carries is not part of the text, which is
    /// the one thing to know before pointing this at an array that is not a
    /// literal: an array of characters with no terminator loses its last one.
    template <std::size_t N>
    consteval basic_text(const CharT (&literal)[N]) : text_(literal, N - 1) {
        if constexpr (is_utf8) {
            if (impl::scan_utf8(plain_type(literal, N - 1))) throw "not well-formed UTF-8";
        } else {
            if (impl::scan_utf16(plain_type(literal, N - 1))) throw "not well-formed UTF-16";
        }
    }

    /// The same text with the guarantee dropped, for the byte work that has no
    /// use for it -- trimming, splitting, searching.
    constexpr plain_type plain() const noexcept { return text_; }

    constexpr const CharT* data() const noexcept { return text_.data(); }
    constexpr std::size_t size() const noexcept { return text_.size(); }
    constexpr bool empty() const noexcept { return text_.empty(); }

    /// The same slice std::basic_string_view::substr makes, and still checked
    /// text: a part of well-formed text is well-formed exactly when both of
    /// its ends fall between code points, and that is what is tested here.
    ///
    /// Both tests are one comparison and no walk -- UTF-8 says in every byte
    /// whether it starts a sequence, and UTF-16 says it in every unit -- so
    /// this is what the plain substr costs. Without it, slicing checked text
    /// would mean stepping out through chars() and promising the result back
    /// with assume_valid(), which is a promise nobody can check and a habit
    /// that empties the type of its meaning.
    ///
    /// Cutting a code point in half is a program error rather than bad data:
    /// the offset comes from the caller's own walk through text it was handed
    /// whole. So it aborts, the way ensure() does everywhere else here.
    ///
    /// Only a view has this. A string owns what it holds, and a substr of it
    /// would have to answer who owns the part -- a question a caller settles
    /// by taking the view first, and one this library does not answer by
    /// copying on its own.
    constexpr basic_text substr(const std::size_t at,
                                  const std::size_t count = plain_type::npos) const {
        const plain_type part = text_.substr(at, count);

        ensure(begins_a_code_point(at) && "checked text may only be cut between code points");
        ensure(begins_a_code_point(at + part.size()) &&
               "checked text may only be cut between code points");

        return {part, impl::validated{}};
    }

    /// The same bytes as a std::string_view -- what fmt, the file APIs and
    /// wxl.xml take. char8_t and char differ in name alone, and a cast written
    /// out at every boundary would be a cast nobody reads.
    std::string_view chars() const noexcept
        requires is_utf8
    {
        return std::string_view(reinterpret_cast<const char*>(text_.data()), text_.size());
    }

    /// The same units as a std::wstring_view -- what Windows takes.
    std::wstring_view wchars() const noexcept
        requires is_utf16
    {
        return std::wstring_view(reinterpret_cast<const wchar_t*>(text_.data()), text_.size());
    }

    /// The text in that encoding. Asking for the one it is already in costs
    /// nothing and answers with itself, so a caller that does not know which it
    /// has -- a template, a call chain -- writes one of these and is right
    /// either way.
    constexpr basic_text to_utf8() const noexcept
        requires is_utf8
    {
        return *this;
    }

    constexpr basic_text to_utf16() const noexcept
        requires is_utf16
    {
        return *this;
    }

    u16_text to_utf16() const
        requires is_utf8;

    u8_text to_utf8() const
        requires is_utf16;

    constexpr bool operator==(const basic_text&) const = default;

    /// Against plain text, because comparing is the one thing that cannot
    /// break the guarantee: what comes out is a bool, not text. This is what
    /// keeps `node.name() == "section"` reading the way it always did.
    constexpr bool operator==(plain_type other) const noexcept { return text_ == other; }

    bool operator==(std::string_view other) const noexcept
        requires is_utf8
    {
        return chars() == other;
    }

    bool operator==(std::wstring_view other) const noexcept
        requires is_utf16
    {
        return wchars() == other;
    }

private:
    /// Whether the unit at that offset starts a code point -- true at the end
    /// of the text too, since that is where a slice may also stop.
    constexpr bool begins_a_code_point(const std::size_t at) const noexcept {
        if (at == text_.size())
            return true;

        if constexpr (is_utf8)
            return !is_continuation(static_cast<char>(text_[at]));
        else
            return !is_low_surrogate(text_[at]);
    }

    plain_type text_;
};

/// Text known to be well-formed, in a string of its own -- what checked text
/// becomes when it has to outlive the buffer it arrived in, what comes back
/// from a transcoding, and what text is assembled in.
///
/// It changes only in ways that keep it well-formed, the way Rust's String
/// does: it grows by whole code points and by checked text, and it is cut only
/// between code points. As with any string, a change may move the units, and a
/// view taken before it is not to be read after.
template <typename CharT, typename Traits, typename Allocator>
class basic_text<std::basic_string<CharT, Traits, Allocator>> {
public:
    using value_type = CharT;
    using plain_type = std::basic_string<CharT, Traits, Allocator>;
    using view_type = basic_text<std::basic_string_view<CharT, Traits>>;

    static constexpr bool is_utf8 = std::same_as<CharT, char8_t>;
    static constexpr bool is_utf16 = std::same_as<CharT, char16_t>;

    static_assert(is_utf8 || is_utf16);

    constexpr basic_text() noexcept = default;

    basic_text(plain_type text, impl::validated) noexcept : text_(std::move(text)) {}

    /// A copy of a view one was handed, for keeping -- and the one way to get
    /// a null-terminated string out of a view, which c_str() below needs.
    explicit basic_text(view_type text) : text_(text.plain()) {}

    /// A string answers as a view, and the view is safe as well: a whole text
    /// that is well-formed stays well-formed when nothing is cut off it. This
    /// is why the guarantee survives being passed on rather than stopping at
    /// the first function that wanted a view.
    constexpr operator view_type() const noexcept {
        return view_type(std::basic_string_view<CharT, Traits>(text_), impl::validated{});
    }

    constexpr const plain_type& plain() const& noexcept { return text_; }

    /// The string itself, for a caller done with the guarantee. Moving it out
    /// leaves an empty basic_text, which is still a valid one.
    plain_type plain() && noexcept { return std::move(text_); }

    constexpr const CharT* data() const noexcept { return text_.data(); }
    constexpr std::size_t size() const noexcept { return text_.size(); }
    constexpr bool empty() const noexcept { return text_.empty(); }

    /// Room for this many units in all, so that text assembled piece by piece
    /// grows its buffer once.
    void reserve(const std::size_t units) { text_.reserve(units); }

    /// Appends one code point, in as many units as the encoding spells it
    /// with. One that is not a scalar value stops the program, the way a cut
    /// through a code point does in substr(): it came from the caller's own
    /// decoding, not from the data.
    void push_back(const char32_t code_point) {
        ensure(is_scalar_value(code_point));

        if constexpr (is_utf8)
            impl::push_utf8(text_, code_point);
        else
            impl::push_utf16(text_, code_point);
    }

    /// A unit is not a code point: a byte of UTF-8 converted to one reads as a
    /// Latin-1 letter, and half a pair stops the program only once it runs. So
    /// a unit does not compile.
    void push_back(char) = delete;
    void push_back(char8_t) = delete;
    void push_back(char16_t) = delete;
    void push_back(wchar_t) = delete;

    /// Appends checked text. Two well-formed texts end to end are well-formed:
    /// neither ends or begins inside a code point.
    basic_text& append(const view_type text) {
        text_.append(text.plain());
        return *this;
    }

    basic_text& operator+=(const view_type text) { return append(text); }

    /// Removes `count` units from `at`, as std::basic_string::erase() does, and
    /// only between code points: both ends are tested the way substr() tests
    /// a cut, and a cut through a code point stops the program.
    basic_text& erase(const std::size_t at, const std::size_t count = plain_type::npos) {
        const view_type removed = static_cast<view_type>(*this).substr(at, count);
        text_.erase(at, removed.size());
        return *this;
    }

    std::string_view chars() const noexcept
        requires is_utf8
    {
        return std::string_view(reinterpret_cast<const char*>(text_.data()), text_.size());
    }

    std::wstring_view wchars() const noexcept
        requires is_utf16
    {
        return std::wstring_view(reinterpret_cast<const wchar_t*>(text_.data()), text_.size());
    }

    /// Null-terminated, for the calls that take a pointer and no length --
    /// which is most of Windows. std::basic_string keeps the terminator, so
    /// this is the same guarantee its own c_str() gives, read as the character
    /// type the callee expects. A view has no terminator and so has no c_str().
    const char* c_str() const noexcept
        requires is_utf8
    {
        return reinterpret_cast<const char*>(text_.c_str());
    }

    const wchar_t* c_str() const noexcept
        requires is_utf16
    {
        return reinterpret_cast<const wchar_t*>(text_.c_str());
    }

    /// The text in that encoding; the one it is already in answers with itself
    /// and copies nothing.
    constexpr const basic_text& to_utf8() const noexcept
        requires is_utf8
    {
        return *this;
    }

    constexpr const basic_text& to_utf16() const noexcept
        requires is_utf16
    {
        return *this;
    }

    u16_text to_utf16() const
        requires is_utf8;

    u8_text to_utf8() const
        requires is_utf16;

    bool operator==(const basic_text&) const = default;

    bool operator==(std::basic_string_view<CharT, Traits> other) const noexcept {
        return std::basic_string_view<CharT, Traits>(text_) == other;
    }

    bool operator==(std::string_view other) const noexcept
        requires is_utf8
    {
        return chars() == other;
    }

    bool operator==(std::wstring_view other) const noexcept
        requires is_utf16
    {
        return wchars() == other;
    }

private:
    plain_type text_;
};

/// Text somebody else has already checked: a document wxl.xml validated before
/// its grammar walked it, a name the file system spelled and the program has
/// tested another way. Nothing is looked at, which is the whole point -- and
/// the reason the call has to be written out rather than happening by
/// conversion. A literal needs none of this: it converts on its own.
inline u8_view assume_valid(const std::string_view text) noexcept {
    return {std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()),
            impl::validated{}};
}

constexpr u8_view assume_valid(const std::u8string_view text) noexcept {
    return {text, impl::validated{}};
}

inline u16_view assume_valid(const std::wstring_view text) noexcept {
    return {std::u16string_view(reinterpret_cast<const char16_t*>(text.data()), text.size()),
            impl::validated{}};
}

constexpr u16_view assume_valid(const std::u16string_view text) noexcept {
    return {text, impl::validated{}};
}

/// The text if it is well-formed, and nothing if it is not -- the door for
/// everything that came from outside the program.
inline std::optional<u8_view> checked(const std::string_view text) noexcept {
    if (find_invalid_utf8(text)) return std::nullopt;

    return assume_valid(text);
}

constexpr std::optional<u8_view> checked(const std::u8string_view text) noexcept {
    if (find_invalid_utf8(text)) return std::nullopt;

    return assume_valid(text);
}

inline std::optional<u16_view> checked(const std::wstring_view text) noexcept {
    if (find_invalid_utf16(text)) return std::nullopt;

    return assume_valid(text);
}

constexpr std::optional<u16_view> checked(const std::u16string_view text) noexcept {
    if (find_invalid_utf16(text)) return std::nullopt;

    return assume_valid(text);
}

/// The same text with every unpaired surrogate replaced by U+FFFD.
///
/// This is what a file name needs. Windows does not promise one is well-formed
/// UTF-16, and where the name has to be written into a file that is UTF-8, a
/// lone surrogate cannot simply be passed on: it would become three bytes no
/// reader of UTF-8 accepts, and the file would fail to load the next time.
/// Refusing outright is the other honest answer, and the caller that wants it
/// has checked(); this is for the caller that must write something.
u16_text repaired(std::wstring_view utf16);
u16_text repaired(std::u16string_view utf16);

// ---- Measuring ---------------------------------------------------------------------

/// How many code points the text holds. Bytes are what a string_view counts,
/// and characters are what a reading position, a column or a limit means.
inline std::size_t code_point_count(const u8_view utf8) noexcept {
    return impl::code_point_count(utf8.chars());
}

/// How many UTF-16 units the text needs -- the size to make room for before
/// write_utf16().
inline std::size_t utf16_size(const u8_view utf8) noexcept {
    return impl::utf16_size(utf8.chars());
}

/// How many UTF-8 bytes the text needs -- the size to make room for before
/// write_utf8().
inline std::size_t utf8_size(const u16_view utf16) noexcept {
    return impl::utf8_size(utf16.wchars());
}

// ---- Transcoding -------------------------------------------------------------------

/// Writes the text as UTF-16 into `out`, which must have room for
/// utf16_size(utf8) units, and returns one past the last unit written.
///
/// This is the primitive the conveniences are built from: it neither allocates
/// nor knows what kind of string it writes into, so a caller with a buffer of
/// its own -- a std::wstring, a wxl::wstring on the STA pool, a fixed array --
/// pays for nothing it does not use.
inline wchar_t* write_utf16(wchar_t* out, const u8_view utf8) noexcept {
    return impl::write_utf16_to(out, utf8.chars());
}

/// Writes the text as UTF-8 into `out`, which must have room for
/// utf8_size(utf16) bytes, and returns one past the last byte written.
inline char* write_utf8(char* out, const u16_view utf16) noexcept {
    return impl::write_utf8_to(out, utf16.wchars());
}

/// Appends one code point to a UTF-8 string. The code point must be a scalar
/// value; the caller is what checks that, because the caller is where it came
/// from.
template <typename Traits, typename Allocator>
void append_utf8(std::basic_string<char, Traits, Allocator>& out, char32_t code_point) {
    ensure(is_scalar_value(code_point));
    impl::push_utf8(out, code_point);
}

/// Appends one code point to a UTF-16 string, as a pair above the basic plane.
/// The code point must be a scalar value.
template <typename Traits, typename Allocator>
void append_utf16(std::basic_string<wchar_t, Traits, Allocator>& out, char32_t code_point) {
    ensure(is_scalar_value(code_point));
    impl::push_utf16(out, code_point);
}

/// Appends the text, transcoded, to a string of any allocator: the size is
/// measured first, so the buffer grows once and the conversion writes straight
/// into it.
template <typename Traits, typename Allocator>
void append_utf16(std::basic_string<wchar_t, Traits, Allocator>& out, u8_view utf8) {
    const std::size_t was = out.size();

    // resize_and_overwrite keeps what the string already held and leaves the
    // rest untouched, so the transcoder writes into raw storage instead of
    // over a zero-fill nobody asked for.
    out.resize_and_overwrite(was + utf16_size(utf8), [was, utf8](wchar_t* buffer, std::size_t size) {
        write_utf16(buffer + was, utf8);
        return size;
    });
}

/// The same the other way round.
template <typename Traits, typename Allocator>
void append_utf8(std::basic_string<char, Traits, Allocator>& out, u16_view utf16) {
    const std::size_t was = out.size();

    out.resize_and_overwrite(was + utf8_size(utf16), [was, utf16](char* buffer, std::size_t size) {
        write_utf8(buffer + was, utf16);
        return size;
    });
}

/// A path as UTF-8. Through the units the file system actually holds, and with
/// U+FFFD where they do not spell anything: std::filesystem::path::string()
/// would convert through the active code page and mangle every name that page
/// cannot write, and u8string() has no answer at all for a name that is not
/// well-formed UTF-16 -- which nothing stops a name from being.
u8_text to_utf8(const std::filesystem::path& path);

// ---- Walking ------------------------------------------------------------------------

/// The code points of a UTF-8 text, one at a time and decoding nothing the
/// caller does not ask for.
///
/// The view holds the text rather than copying it, so the text has to outlive
/// it. Besides the code point itself, an iterator hands out where it sits
/// (position()) and the bytes it was spelled with (sequence()) -- the two
/// things a caller counting characters or slicing text would otherwise have to
/// work out a second time.
class code_points {
public:
    explicit code_points(u8_view utf8) noexcept : text_(utf8.chars()) {}

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = char32_t;
        using difference_type = std::ptrdiff_t;
        using reference = char32_t;
        using pointer = void;

        constexpr iterator() noexcept = default;

        constexpr iterator(std::string_view text, std::size_t at) noexcept
            : text_(text), at_(at) {}

        constexpr char32_t operator*() const noexcept {
            const auto lead = static_cast<unsigned char>(text_[at_]);

            if (lead < 0x80) return lead;

            const int size = utf8_sequence_size(text_[at_]);
            const unsigned lead_bits = 0xFFu >> (size + 1);

            char32_t code_point = lead & lead_bits;
            for (int index = 1; index < size; ++index)
                code_point =
                    (code_point << 6) | (static_cast<unsigned char>(text_[at_ + index]) & 0x3Fu);

            return code_point;
        }

        /// The byte offset of the code point the iterator stands on.
        constexpr std::size_t position() const noexcept { return at_; }

        /// The bytes the code point is spelled with, as a view into the text.
        /// Well-formed in its own right: a whole sequence is a whole sequence
        /// wherever it is cut out of, which is why the guarantee travels with
        /// it instead of stopping here.
        u8_view sequence() const noexcept {
            return assume_valid(
                text_.substr(at_, static_cast<std::size_t>(utf8_sequence_size(text_[at_]))));
        }

        constexpr iterator& operator++() noexcept {
            at_ += static_cast<std::size_t>(utf8_sequence_size(text_[at_]));
            return *this;
        }

        constexpr iterator operator++(int) noexcept {
            iterator was = *this;
            ++*this;
            return was;
        }

        constexpr bool operator==(const iterator& other) const noexcept {
            return at_ == other.at_;
        }

    private:
        std::string_view text_;
        std::size_t at_ = 0;
    };

    constexpr iterator begin() const noexcept { return iterator(text_, 0); }
    constexpr iterator end() const noexcept { return iterator(text_, text_.size()); }

    u8_view text() const noexcept { return assume_valid(text_); }

private:
    std::string_view text_;
};

// ---- The transcoding methods -------------------------------------------------------
//
// Out of line because they answer with the other specialization, which is not
// complete where they are declared.

template <typename CharT, typename Traits>
u16_text basic_text<std::basic_string_view<CharT, Traits>>::to_utf16() const
    requires is_utf8
{
    std::u16string result;

    // resize_and_overwrite rather than resize: the conversion fills every unit
    // itself, and the zero-fill resize() does first would be written over
    // immediately.
    result.resize_and_overwrite(
        impl::utf16_size(chars()), [this](char16_t* buffer, std::size_t size) {
            impl::write_utf16_to(reinterpret_cast<wchar_t*>(buffer), chars());
            return size;
        });

    return {std::move(result), impl::validated{}};
}

template <typename CharT, typename Traits>
u8_text basic_text<std::basic_string_view<CharT, Traits>>::to_utf8() const
    requires is_utf16
{
    std::u8string result;

    result.resize_and_overwrite(
        impl::utf8_size(wchars()), [this](char8_t* buffer, std::size_t size) {
            impl::write_utf8_to(reinterpret_cast<char*>(buffer), wchars());
            return size;
        });

    return {std::move(result), impl::validated{}};
}

template <typename CharT, typename Traits, typename Allocator>
u16_text basic_text<std::basic_string<CharT, Traits, Allocator>>::to_utf16() const
    requires is_utf8
{
    return view_type(*this).to_utf16();
}

template <typename CharT, typename Traits, typename Allocator>
u8_text basic_text<std::basic_string<CharT, Traits, Allocator>>::to_utf8() const
    requires is_utf16
{
    return view_type(*this).to_utf8();
}

/// So that a failing test says what the text was rather than dumping its bytes.
/// UTF-16 goes out as UTF-8, which is what a console and a log file take.
template <typename T>
std::ostream& operator<<(std::ostream& out, const basic_text<T>& text) {
    if constexpr (basic_text<T>::is_utf8)
        return out << text.chars();
    else
        return out << text.to_utf8().chars();
}

}  // export

}  // namespace wxl::core
