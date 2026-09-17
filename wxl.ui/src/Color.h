#pragma once

#include "core.h"

// wxl::Color -- given from above, projected onto Windows.UI.Color rather
// than generated from it (see wxl.gen/gen/projection.cpp).
//
// The metadata type is a bare four-byte aggregate with no behaviour at all,
// so a generated copy of it would be strictly worse than a hand-written one
// that carries the conveniences declarative UI code actually wants: literal
// construction from 0xAARRGGBB, the handful of named colors, and a
// layout guaranteed to match the ABI struct so it can be passed straight
// through.

namespace wxl {

struct Color {
    uint8_t A;
    uint8_t R;
    uint8_t G;
    uint8_t B;
    friend constexpr bool operator==(Color, Color) noexcept = default;
};

struct ARGB : Color {
    constexpr ARGB() noexcept : ARGB(255, 0, 0, 0) {}

    constexpr explicit ARGB(uint32_t argb)
        : ARGB((argb >> 24) & 0xFF, (argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF) {}

    constexpr ARGB(uint8_t red, uint8_t green, uint8_t blue) noexcept
        : ARGB(255, red, green, blue) {}

    constexpr ARGB(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) noexcept {
        A=alpha; R=red; G=green; B=blue;
    }
};

/// A colour in CSS hex notation, the form an editor's colour picker reads and
/// writes: "#RGB", "#RGBA", "#RRGGBB" or "#RRGGBBAA". Alpha comes last, which
/// is why this is not ARGB. Read at compile time, so anything else fails the build.
struct RGBA : Color {
    template <std::size_t Size>
    consteval explicit RGBA(char const (&css)[Size]) {
        std::size_t const digits = Size - 2;
        if (css[0] != '#' || css[Size - 1] != '\0'
            || (digits != 3 && digits != 4 && digits != 6 && digits != 8))
            not_css_hex();

        bool const doubled = digits <= 4;
        auto const channel = [&css, doubled](std::size_t at) {
            return doubled ? static_cast<uint8_t>(hex(css[1 + at]) * 17)
                           : static_cast<uint8_t>(hex(css[1 + 2 * at]) * 16 + hex(css[2 + 2 * at]));
        };

        R = channel(0);
        G = channel(1);
        B = channel(2);
        A = digits == 4 || digits == 8 ? channel(3) : uint8_t{255};
    }

private:
    static constexpr int hex(char const digit) {
        if (digit >= '0' && digit <= '9') return digit - '0';
        if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
        not_css_hex();
        return 0;
    }

    // Never defined: reaching it while the literal is read is the compile error.
    static void not_css_hex();
};

inline static constexpr struct {
    inline static constexpr ARGB transparent{0};
    inline static constexpr ARGB black{0, 0, 0};
    inline static constexpr ARGB white{255, 255, 255};
    inline static constexpr ARGB red{255, 0, 0};
    inline static constexpr ARGB green{0, 128, 0};
    inline static constexpr ARGB blue{0, 0, 255};
    inline static constexpr ARGB gray{128, 128, 128};
} colors;

}  // namespace wxl

namespace wxl::core {

// A colour with nothing set -- the button colours of a title bar, which WinRT
// boxes as IReference<Color> so that "not set" can hand the choice back to the
// system. Every bit pattern of the four bytes is a colour, transparent black
// included, so there is no sentinel to compress the empty state into, and
// nullable<Color> is std::optional<Color>, the way bool's is.
template <>
struct optional_selector<wxl::Color> {
    using nullable = std::optional<wxl::Color>;
};

}  // namespace wxl::core
