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
