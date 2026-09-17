module;

#include "abi.h"

// Numbers on their way to and from text, in no locale at all.
//
// This is the whole point of the partition. A number printed through a stream
// or through std::to_string comes out with whatever decimal separator the
// program was configured for, so a file written on a machine set to Russian
// says 1,5 and the same program reading it on a machine set to English gets
// 1. Settings, registries and every other file this tree writes are formats,
// not documents, and a format has to read back the way it was written --
// which is why everything here goes through std::to_chars and
// std::from_chars, the two operations the standard defines without a locale
// to begin with.
//
// The grammar is theirs as well: no leading whitespace, no leading plus, no
// thousands separators, and a hexadecimal float only where it is asked for.

export module wxl.core:numbers;

import :checks;
import :unicode;
import std;

namespace wxl::core::impl {

/// Enough for every built-in arithmetic type in every format to_chars offers:
/// a 128-bit integer takes 40 characters and a long double in fixed notation
/// with full precision stays well under this.
inline constexpr std::size_t number_buffer_size = 64;

}  // namespace wxl::core::impl

export namespace wxl::core {

/// Reads the number the whole text spells into `result`, and says whether it
/// did. The whole text has to be the number: a trailing unit or a stray
/// character is what tells a caller reading "12pt" that it has more than a
/// number in hand, and swallowing it silently would turn a broken file into a
/// plausible one.
///
/// On a refusal `result` is left exactly as it was -- not half-read, not
/// zeroed. So a caller with a default writes the default first and calls this
/// once, with nothing to test afterwards:
///
///     double split = 0.5;
///     try_parse(attribute, split);
///
/// For "12pt" and its kind there is parse_prefix() below, which hands back the
/// number and the rest separately.
template <typename T>
bool try_parse(std::string_view text, T& result) noexcept {
    T value{};

    const auto [stop, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || stop != text.data() + text.size()) return false;

    result = value;
    return true;
}

/// A number and whatever follows it -- the shape a length, a duration or a
/// version needs, where the unit is glued to the digits.
template <typename T>
struct number_prefix {
    T value;

    /// The text left over, starting at the first character the number did not
    /// use. Empty when the number was the whole text.
    std::string_view rest;
};

/// The number the text starts with, or nothing when it does not start with
/// one.
template <typename T>
std::optional<number_prefix<T>> parse_prefix(std::string_view text) noexcept {
    T value{};

    const auto [stop, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{}) return {};

    return number_prefix<T>{value, text.substr(static_cast<std::size_t>(stop - text.data()))};
}

/// The same, from UTF-16 text -- what a value read out of a control or a
/// Windows API arrives as. There is no from_chars for wide units, in the
/// standard or in Boost.Charconv, and every wide reader Windows or the C
/// library offers reads the locale and wants a terminator. A number is ASCII
/// by definition, though, so the units narrow to bytes one for one, and a
/// unit that does not narrow is the answer "not a number" rather than a
/// conversion.
///
/// The allocator is the caller's to name, as a template, the way text_builder
/// names it: on the GUI thread that is try_parse<sta_allocator>(text,
/// value), and the default heap is for the threads that own no pool. `T` needs
/// no default of its own for that -- it is deduced from `result`.
template <template <typename> class Allocator = std::allocator, typename T>
bool try_parse(std::wstring_view text, T& result) {
    // A number fits in the string's own small buffer; only a text longer than
    // that is allocated at all, and then from the allocator named. "Longer
    // than the buffer" is not the same answer as "not a number", and saying
    // so would be a lie about the text rather than about the buffer.
    std::basic_string<char, std::char_traits<char>, Allocator<char>> narrow;
    narrow.resize(text.size());

    char* out = narrow.data();
    for (const wchar_t unit : text) {
        if (static_cast<std::uint16_t>(unit) >= 0x80) return false;
        *out++ = static_cast<char>(unit);
    }

    return try_parse(std::string_view(narrow), result);
}

/// Appends the number. Nothing is reserved and nothing is measured first: the
/// digits go into a buffer on the stack, and one append moves them.
template <typename T, typename Traits, typename Allocator, typename... Format>
void append_number(std::basic_string<char, Traits, Allocator>& out, T value, Format... format) {
    std::array<char, impl::number_buffer_size> buffer;

    const auto [stop, error] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, format...);

    ensure(error == std::errc{});

    out.append(buffer.data(), stop);
}

/// The same into UTF-16 text. Digits are ASCII, so the widening is a copy.
template <typename T, typename Traits, typename Allocator, typename... Format>
void append_number(std::basic_string<wchar_t, Traits, Allocator>& out, T value, Format... format) {
    std::array<char, impl::number_buffer_size> buffer;

    const auto [stop, error] =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, format...);

    ensure(error == std::errc{});

    const auto digits = static_cast<std::size_t>(stop - buffer.data());
    const std::size_t was = out.size();

    out.resize_and_overwrite(was + digits, [&buffer, was, digits](wchar_t* room, std::size_t size) {
        for (std::size_t at = 0; at != digits; ++at)
            room[was + at] = static_cast<wchar_t>(buffer[at]);
        return size;
    });
}

/// Appends the number with a fixed number of digits after the point -- the
/// form a share, a price or a size is shown in, where "12.5%" and "12.50%"
/// are different answers and the shortest round-trip is the wrong one.
template <typename CharT, typename Traits, typename Allocator>
void append_fixed(std::basic_string<CharT, Traits, Allocator>& out, double value, int precision) {
    append_number(out, value, std::chars_format::fixed, precision);
}

/// The number as text of its own, in whatever format std::to_chars is given.
template <typename T, typename... Format>
std::string to_string(T value, Format... format) {
    std::string result;
    append_number(result, value, format...);
    return result;
}

template <typename T, typename... Format>
std::wstring to_wstring(T value, Format... format) {
    std::wstring result;
    append_number(result, value, format...);
    return result;
}

/// Checked text without the check: every character of a number is ASCII.
template <typename T, typename... Format>
u16_text to_u16(T value, Format... format) {
    return u16_text{unicode::assume_valid(to_wstring(value, format...))};
}

/// The unit a byte count was scaled into, in steps of 1024 -- the steps
/// Windows itself shows sizes in.
enum class byte_unit { bytes, kilobytes, megabytes, gigabytes, terabytes, petabytes };

/// A byte count and the unit it is worth saying out loud in.
struct scaled_bytes {
    double value;
    byte_unit unit;
};

/// The count scaled into the largest unit it fills at least once: 900 stays
/// 900 bytes, 2048 becomes 2 kilobytes.
///
/// The words are the caller's, and deliberately: "KB", "КБ" and "Кб" are the
/// same unit in three languages, and a library that picked one would be
/// writing the application's interface for it.
constexpr scaled_bytes scale_bytes(std::uint64_t bytes) noexcept {
    auto value = static_cast<double>(bytes);
    auto unit = static_cast<int>(byte_unit::bytes);

    while (value >= 1024.0 && unit < static_cast<int>(byte_unit::petabytes)) {
        value /= 1024.0;
        ++unit;
    }

    return {value, static_cast<byte_unit>(unit)};
}

}  // export namespace wxl::core
