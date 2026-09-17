module;

#include "abi.h"

module wxl.core;

import std;

namespace wxl::core {
namespace {

// Eight bytes at a time. Real text is mostly ASCII even when it is Russian --
// the spaces, the punctuation and the digits are -- and a word with no high
// bit set needs no decoding at all.
constexpr std::uint64_t high_bits = 0x8080808080808080ull;
constexpr std::uint64_t low_bits = 0x0101010101010101ull;

/// One per byte that continues a sequence (10xxxxxx). Shifting a bit down to
/// position 0 of its own byte and masking is exact: no borrow travels between
/// bytes the way it does in the classic zero-byte trick.
constexpr std::uint64_t continuation_marks(std::uint64_t word) noexcept {
    return (word >> 7) & ~(word >> 6) & low_bits;
}

/// One per byte that leads a four-byte sequence (11110xxx) -- the sequences
/// that become a surrogate pair, and so the only ones UTF-16 spends two units
/// on.
constexpr std::uint64_t lead4_marks(std::uint64_t word) noexcept {
    return (word >> 7) & (word >> 6) & (word >> 5) & (word >> 4) & ~(word >> 3) & low_bits;
}

constexpr std::size_t word_size = sizeof(std::uint64_t);

// The same eight bytes read as four UTF-16 units.
constexpr std::size_t wide_step = word_size / sizeof(wchar_t);

constexpr std::uint64_t lane_ones = 0x0001000100010001ull;
constexpr std::uint64_t lane_high = 0x8000800080008000ull;

/// How many of the four units are zero.
///
/// The borrow that walks out of a zero lane can only mislead the lane above it
/// when that lane holds exactly one, and every use below masks the low bits
/// away first, so no lane here ever holds one.
constexpr std::size_t zero_lanes(std::uint64_t lanes) noexcept {
    return static_cast<std::size_t>(std::popcount((lanes - lane_ones) & ~lanes & lane_high));
}

std::uint64_t load_word(const char* at) noexcept {
    std::uint64_t word = 0;
    std::memcpy(&word, at, word_size);
    return word;
}

/// The code point of a sequence whose length is already known. The lead byte
/// carries the top bits: as many as the length leaves it.
constexpr char32_t decode(const char* at, int size) noexcept {
    const auto lead = static_cast<unsigned char>(*at);

    if (size == 1) return lead;

    char32_t code_point = lead & (0xFFu >> (size + 1));

    for (int index = 1; index < size; ++index)
        code_point = (code_point << 6) | (static_cast<unsigned char>(at[index]) & 0x3Fu);

    return code_point;
}

}  // namespace

std::size_t impl::code_point_count(const std::string_view utf8) noexcept {
    const char* at = utf8.data();
    const char* const end = at + utf8.size();

    // Every byte starts a code point except the ones that continue one, so the
    // count is the size less the continuations -- a byte count, which means
    // neither the walk nor its result depends on where the sequences fall.
    std::size_t continuations = 0;

    while (static_cast<std::size_t>(end - at) >= word_size) {
        continuations += static_cast<std::size_t>(std::popcount(continuation_marks(load_word(at))));
        at += word_size;
    }

    while (at != end) continuations += unicode::is_continuation(*at++) ? 1u : 0u;

    return utf8.size() - continuations;
}

std::size_t impl::utf16_size(const std::string_view utf8) noexcept {
    const char* at = utf8.data();
    const char* const end = at + utf8.size();

    std::size_t continuations = 0;
    std::size_t pairs = 0;

    while (static_cast<std::size_t>(end - at) >= word_size) {
        const std::uint64_t word = load_word(at);
        continuations += static_cast<std::size_t>(std::popcount(continuation_marks(word)));
        pairs += static_cast<std::size_t>(std::popcount(lead4_marks(word)));
        at += word_size;
    }

    while (at != end) {
        const auto byte = static_cast<unsigned char>(*at++);
        continuations += (byte & 0xC0u) == 0x80u ? 1u : 0u;
        pairs += (byte & 0xF8u) == 0xF0u ? 1u : 0u;
    }

    return utf8.size() - continuations + pairs;
}

std::size_t impl::utf8_size(const std::wstring_view utf16) noexcept {
    // Counted as three questions asked of every unit rather than a ladder of
    // comparisons, because three questions can be asked of four units at once:
    //
    //   one byte for every unit, plus one more where the unit needs two, plus
    //   one more where it needs three, less one for each half of a surrogate
    //   pair -- which needs two, not the three its value would suggest, since
    //   the pair spells four bytes between them.
    //
    // The loop therefore never looks ahead at the unit that completes a pair.
    const wchar_t* at = utf16.data();
    const wchar_t* const end = at + utf16.size();

    std::size_t bytes = utf16.size();

    while (static_cast<std::size_t>(end - at) >= wide_step) {
        std::uint64_t word = 0;
        std::memcpy(&word, at, word_size);

        const std::uint64_t two = word & 0xFF80FF80FF80FF80ull;
        const std::uint64_t three = word & 0xF800F800F800F800ull;
        const std::uint64_t paired = three ^ 0xD800D800D800D800ull;

        bytes += wide_step - zero_lanes(two);
        bytes += wide_step - zero_lanes(three);
        bytes -= zero_lanes(paired);

        at += wide_step;
    }

    while (at != end) {
        const auto value = static_cast<char32_t>(static_cast<std::uint16_t>(*at++));

        bytes += value < 0x80 ? 0u : value < 0x800 ? 1u : unicode::is_surrogate(value) ? 1u : 2u;
    }

    return bytes;
}

wchar_t* impl::write_utf16_to(wchar_t* out, const std::string_view utf8) noexcept {
    const char* at = utf8.data();
    const char* const end = at + utf8.size();

    while (at != end) {
        while (static_cast<std::size_t>(end - at) >= word_size) {
            if ((load_word(at) & high_bits) != 0) break;

            for (std::size_t index = 0; index != word_size; ++index)
                out[index] = static_cast<wchar_t>(static_cast<unsigned char>(at[index]));

            out += word_size;
            at += word_size;
        }

        while (at != end && static_cast<unsigned char>(*at) < 0x80)
            *out++ = static_cast<wchar_t>(static_cast<unsigned char>(*at++));

        if (at == end) break;

        const int size = unicode::utf8_sequence_size(*at);

        // Not a check of the text -- the caller has vouched for it -- but the
        // one thing that keeps a broken byte from walking the pointer past the
        // end instead of stopping at it.
        ensure(size != 0);

        const char32_t code_point = decode(at, size);
        at += size;

        if (code_point < 0x10000) {
            *out++ = static_cast<wchar_t>(code_point);
        } else {
            const char32_t rest = code_point - 0x10000;
            *out++ = static_cast<wchar_t>(0xD800u + (rest >> 10));
            *out++ = static_cast<wchar_t>(0xDC00u + (rest & 0x3FFu));
        }
    }

    return out;
}

char* impl::write_utf8_to(char* out, const std::wstring_view utf16) noexcept {
    const wchar_t* at = utf16.data();
    const wchar_t* const end = at + utf16.size();

    // Four units at a time, in the two shapes a run of text actually comes in.
    constexpr std::uint64_t wide_high_bits = 0xFF80FF80FF80FF80ull;
    constexpr std::uint64_t three_byte_bits = 0xF800F800F800F800ull;

    while (at != end) {
        while (static_cast<std::size_t>(end - at) >= wide_step) {
            std::uint64_t word = 0;
            std::memcpy(&word, at, word_size);

            // All four ASCII: they narrow to one byte each.
            if ((word & wide_high_bits) == 0) {
                for (std::size_t index = 0; index != wide_step; ++index)
                    out[index] = static_cast<char>(at[index]);

                out += wide_step;
                at += wide_step;
                continue;
            }

            // All four in the two-byte range, which is what a run of Russian
            // text is: every letter of every European alphabet lives there,
            // and so do all the accents and most of the punctuation. The pair
            // of bytes each one becomes is built for four units at once --
            // 110xxxxx for the top five bits, 10xxxxxx for the bottom six --
            // and stored as one eight-byte write.
            if ((word & three_byte_bits) == 0 && zero_lanes(word & wide_high_bits) == 0) {
                const std::uint64_t encoded = ((word & 0x003F003F003F003Full) << 8) |
                                              0x8000800080008000ull |
                                              ((word >> 6) & 0x001F001F001F001Full) |
                                              0x00C000C000C000C0ull;

                std::memcpy(out, &encoded, word_size);

                out += word_size;
                at += wide_step;
                continue;
            }

            break;
        }

        while (at != end && static_cast<std::uint16_t>(*at) < 0x80)
            *out++ = static_cast<char>(*at++);

        if (at == end) break;

        auto code_point = static_cast<char32_t>(static_cast<std::uint16_t>(*at++));

        if (unicode::is_high_surrogate(code_point)) {
            ensure(at != end);

            const auto low = static_cast<char32_t>(static_cast<std::uint16_t>(*at++));
            code_point = 0x10000 + ((code_point - 0xD800u) << 10) + (low - 0xDC00u);
        }

        if (code_point < 0x80) {
            *out++ = static_cast<char>(code_point);
        } else if (code_point < 0x800) {
            *out++ = static_cast<char>(0xC0u | (code_point >> 6));
            *out++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
        } else if (code_point < 0x10000) {
            *out++ = static_cast<char>(0xE0u | (code_point >> 12));
            *out++ = static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu));
            *out++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
        } else {
            *out++ = static_cast<char>(0xF0u | (code_point >> 18));
            *out++ = static_cast<char>(0x80u | ((code_point >> 12) & 0x3Fu));
            *out++ = static_cast<char>(0x80u | ((code_point >> 6) & 0x3Fu));
            *out++ = static_cast<char>(0x80u | (code_point & 0x3Fu));
        }
    }

    return out;
}

namespace unicode {

u16_text repaired(const std::wstring_view utf16) {
    std::u16string result;
    result.reserve(utf16.size());

    for (std::size_t at = 0; at != utf16.size(); ++at) {
        const auto unit = static_cast<char32_t>(static_cast<std::uint16_t>(utf16[at]));

        if (!is_surrogate(unit)) {
            result.push_back(static_cast<char16_t>(unit));
            continue;
        }

        const bool paired =
            is_high_surrogate(unit) && at + 1 != utf16.size() &&
            is_low_surrogate(static_cast<char32_t>(static_cast<std::uint16_t>(utf16[at + 1])));

        if (!paired) {
            result.push_back(static_cast<char16_t>(replacement_character));
            continue;
        }

        result.push_back(static_cast<char16_t>(unit));
        result.push_back(static_cast<char16_t>(static_cast<std::uint16_t>(utf16[++at])));
    }

    return {std::move(result), impl::validated{}};
}

u16_text repaired(const std::u16string_view utf16) {
    return repaired(std::wstring_view(reinterpret_cast<const wchar_t*>(utf16.data()), utf16.size()));
}

u8_text to_utf8(const std::filesystem::path& path) {
    // native() and not u8string(): the file system hands out 16-bit units and
    // promises nothing about them, and repaired() is the only conversion here
    // that has an answer for a name which is not well-formed.
    return repaired(path.native()).to_utf8();
}

}  // namespace unicode

}  // namespace wxl::core
