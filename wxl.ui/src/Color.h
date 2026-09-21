#pragma once

#include "core.h"

// wxl::Color -- given from above, projected onto Windows.UI.Color rather
// than generated from it (see wxl.gen/gen/projection.cpp).
//
// The metadata type is a bare four-byte aggregate, and so is this one: no
// constructor, so it stays an aggregate and travels in a register. What
// declarative code writes is the CSS forms around it -- rgb(), rgba() and
// RGBA{"#RRGGBBAA"} -- each read at compile time, and a handful of named
// colours.

namespace wxl {

struct Color {
    uint8_t A;
    uint8_t R;
    uint8_t G;
    uint8_t B;
    friend constexpr bool operator==(Color, Color) noexcept = default;
};

namespace impl {
// Never defined: reaching one while a colour is read is the compile error.
void rgb_channel_out_of_0_255();
void rgba_alpha_out_of_0_1();

consteval uint8_t channel(int value) {
    if (value < 0 || value > 255) rgb_channel_out_of_0_255();
    return static_cast<uint8_t>(value);
}
}  // namespace impl

/// A colour in CSS function notation, opaque: rgb(131, 50, 50). Read at
/// compile time, so a channel outside 0..255 fails the build.
consteval Color rgb(int red, int green, int blue) {
    return {255, impl::channel(red), impl::channel(green), impl::channel(blue)};
}

/// The same with alpha as CSS writes it, a fraction: rgba(131, 50, 50, 0.2).
consteval Color rgba(int red, int green, int blue, double alpha) {
    if (alpha < 0.0 || alpha > 1.0) impl::rgba_alpha_out_of_0_1();
    return {static_cast<uint8_t>(alpha * 255.0 + 0.5), impl::channel(red), impl::channel(green),
            impl::channel(blue)};
}

/// A colour in CSS hex notation, the form an editor's colour picker reads and
/// writes: "#RGB", "#RGBA", "#RRGGBB" or "#RRGGBBAA". Alpha comes last, which
/// is why the type says RGBA. Read at compile time, so anything else fails the build.
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
    inline static constexpr Color transparent = rgba(0, 0, 0, 0);
    inline static constexpr Color black = rgb(0, 0, 0);
    inline static constexpr Color white = rgb(255, 255, 255);
    inline static constexpr Color red = rgb(255, 0, 0);
    inline static constexpr Color green = rgb(0, 128, 0);
    inline static constexpr Color blue = rgb(0, 0, 255);
    inline static constexpr Color gray = rgb(128, 128, 128);
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
