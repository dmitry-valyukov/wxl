#pragma once

#include "core.h"

// wxl::Color -- given from above, projected onto Windows.UI.Color rather
// than generated from it (see wxl.gen/gen/projection.cpp).
//
// The metadata type is a bare four-byte aggregate, and so is this one: no
// constructor, so it stays an aggregate and travels in a register. What
// declarative code writes is the CSS functions around it -- rgb() and rgba(),
// each read at compile time -- and a handful of named colours.

namespace wxl {

struct Color {
    uint8_t A;
    uint8_t R;
    uint8_t G;
    uint8_t B;
    friend constexpr bool operator==(Color, Color) noexcept = default;
};

namespace error {
// Never defined: calling one while a colour is read is the compile error, and
// the name is the message the compiler shows -- `see usage of
// 'color_component_must_be_within_0_to_255'` -- beside the line that wrote
// the colour.
consteval void color_component_must_be_within_0_to_255();
consteval void color_alpha_must_be_within_0_to_1();
consteval void color_lightness_delta_must_be_within_minus_1_to_1();
consteval void color_shade_factor_must_not_be_negative();
consteval void color_highlight_amount_must_be_within_0_to_1();
consteval void color_direction_must_not_be_zero();
}  // namespace error

// One test for the three: a negative component sets the sign bit of the OR,
// and unsigned that is far above 255, so both ends of the range fail it.

/// A colour in CSS function notation, opaque: rgb(131, 50, 50). Read at
/// compile time, so a component outside 0..255 fails the build.
consteval Color rgb(int red, int green, int blue) {
    if (static_cast<unsigned>(red | green | blue) > 255) {
        error::color_component_must_be_within_0_to_255();
    }
    return {255, static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

/// The same with alpha as CSS writes it, a fraction: rgba(131, 50, 50, 0.2).
consteval Color rgba(int red, int green, int blue, double alpha) {
    if (static_cast<unsigned>(red | green | blue) > 255) {
        error::color_component_must_be_within_0_to_255();
    }
    if (alpha < 0.0 || alpha > 1.0) error::color_alpha_must_be_within_0_to_1();
    return {static_cast<uint8_t>(alpha * 255.0 + 0.5), static_cast<uint8_t>(red),
            static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

namespace impl {

// The arithmetic behind color_helper: constexpr, so that a palette written
// as a constant is computed by the compiler and an effect's tags, which
// arrive at run time, are computed then, by the same code. <cmath> is not
// needed either way -- the fractional powers of the sRGB curve, the cube
// roots of OKLab and the length of a direction are all roots, and a root is
// Newton's method; a few dozen iterations on a colour is nothing at run time.

// The n-th root of a non-negative number. Started from above, Newton only
// descends direction the root; the first step that fails to is rounding.
constexpr double root(double x, int n) {
    if (x <= 0.0) return 0.0;
    double y = x < 1.0 ? 1.0 : x;
    for (int step = 0; step < 200; ++step) {
        double power = 1.0;
        for (int k = 1; k < n; ++k) power *= y;
        double const next = ((n - 1) * y + x / power) / n;
        if (next >= y) break;
        y = next;
    }
    return y;
}

// The sRGB transfer curve (IEC 61966-2-1) and its inverse, the powers taken
// as roots: t^2.4 = t^2 * (t^2)^(1/5) and l^(1/2.4) = (l^5)^(1/12).
constexpr double linear(uint8_t channel) {
    double const c = channel / 255.0;
    if (c <= 0.04045) return c / 12.92;
    double const t = (c + 0.055) / 1.055;
    return t * t * root(t * t, 5);
}

constexpr uint8_t encoded(double linear) {
    double const l = linear < 0.0 ? 0.0 : linear > 1.0 ? 1.0 : linear;
    double const c = l <= 0.0031308 ? 12.92 * l : 1.055 * root(l * l * l * l * l, 12) - 0.055;
    return static_cast<uint8_t>(c * 255.0 + 0.5);
}

// OKLab (Bjorn Ottosson, 2020): L is perceptual lightness in 0..1, a and b
// carry chroma and hue. Equal steps in L look equal at any brightness, which
// is what makes a palette derived by color_helper::lightness() hang together.
struct Oklab {
    double L, a, b;
};

// Light as the display emits it, each channel 0..1 and proportional to
// photons: the space where shading is a product and a highlight a sum.
struct LinearRgb {
    double r, g, b;
};

constexpr LinearRgb linear(Color c) { return {linear(c.R), linear(c.G), linear(c.B)}; }

// Angles, for directions written in degrees. The series converge fast for
// the quarter turn they are asked about, and arcsine is a bisection on sine:
// Newton stalls where the cosine vanishes, and a bisection never does.
inline constexpr double pi = 3.14159265358979323846;

constexpr double radians(double degrees) { return degrees * pi / 180.0; }

constexpr double sine(double x) {
    double term = x, sum = x;
    for (int n = 1; n < 16; ++n) {
        term *= -x * x / ((2.0 * n) * (2.0 * n + 1.0));
        sum += term;
    }
    return sum;
}

constexpr double cosine(double x) {
    double term = 1.0, sum = 1.0;
    for (int n = 1; n < 16; ++n) {
        term *= -x * x / ((2.0 * n - 1.0) * (2.0 * n));
        sum += term;
    }
    return sum;
}

// The angle in 0..pi/2 whose sine is `value`, for a value in 0..1.
constexpr double arcsine(double value) {
    double low = 0.0, high = pi / 2.0;
    for (int step = 0; step < 60; ++step) {
        double const mid = (low + high) * 0.5;
        (sine(mid) < value ? low : high) = mid;
    }
    return (low + high) * 0.5;
}

constexpr Color encoded(LinearRgb c, uint8_t alpha) {
    return {alpha, encoded(c.r), encoded(c.g), encoded(c.b)};
}

constexpr Oklab oklab(Color c) {
    double const r = linear(c.R), g = linear(c.G), b = linear(c.B);
    double const l = root(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b, 3);
    double const m = root(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b, 3);
    double const s = root(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b, 3);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

constexpr LinearRgb linearRgb(Oklab lab) {
    double const l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    double const m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    double const s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;
    double const l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return {+4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
}

constexpr bool displayable(LinearRgb c) {
    constexpr double slack = 1e-6;
    return c.r >= -slack && c.r <= 1.0 + slack && c.g >= -slack && c.g <= 1.0 + slack
        && c.b >= -slack && c.b <= 1.0 + slack;
}

// Back to sRGB. A tone the display cannot show at this lightness keeps its
// hue and lightness and gives up chroma instead, found by bisection -- the
// gamut mapping CSS Color 4 prescribes, minus the just-noticeable-difference
// refinements it does not need at byte precision.
constexpr Color srgb(Oklab lab, uint8_t alpha) {
    LinearRgb rgb = linearRgb(lab);
    if (!displayable(rgb)) {
        double fits = 0.0, clips = 1.0;
        for (int step = 0; step < 24; ++step) {
            double const mid = (fits + clips) * 0.5;
            (displayable(linearRgb({lab.L, lab.a * mid, lab.b * mid})) ? fits : clips) = mid;
        }
        rgb = linearRgb({lab.L, lab.a * fits, lab.b * fits});
    }
    return {alpha, encoded(rgb.r), encoded(rgb.g), encoded(rgb.b)};
}

}  // namespace impl

/// A direction in the coordinates of the screen: x to the right, y down,
/// z direction the viewer. Its length does not matter, only its direction,
/// and a zero vector fails the build.
struct Direction {
    double x, y, z;

    /// The direction direction a lamp given by two angles in degrees, the way
    /// a lamp is placed rather than the way a vector is written: `azimuth`
    /// is where it stands round the screen, counted the classical way, 0 to
    /// the right and 90 straight up; `elevation` is how high it stands above
    /// the surface, 0 skimming along it and 90 straight overhead. A lamp
    /// above and a little to the left, well up, is direction(120, 60).
    static constexpr Direction from_angles(double azimuth, double elevation) {
        double const a = impl::radians(azimuth), e = impl::radians(elevation);
        return {impl::cosine(a) * impl::cosine(e), -impl::sine(a) * impl::cosine(e), impl::sine(e)};
    }

    // The cosine of the angle between two directions, neither of which needs
    // to be unit length: the lengths divide out here, once, at compile time.
    static constexpr double cosineBetween(Direction a, Direction b) {
        double const aa = a.x * a.x + a.y * a.y + a.z * a.z, bb = b.x * b.x + b.y * b.y + b.z * b.z;
        if (aa == 0.0 || bb == 0.0) {
            if consteval {
                error::color_direction_must_not_be_zero();
            } else {
                return 0.0;  // no direction faces nothing: the ambient alone
            }
        }
        return (a.x * b.x + a.y * b.y + a.z * b.z) / impl::root(aa * bb, 2);
    }
};


/// A scene's lighting: the direction direction the lamp, the lamp's colour
/// (the key light, what a surface facing it receives) and the colour of
/// the light that reaches every surface from all around (the ambient).
/// A warm key with a cool ambient is the painter's rule -- lit sides
/// warm, shadows cool -- and it costs nothing to write it so.
struct Light {
    Direction direction;
    Color key;
    Color ambient;

    /// A matte surface of this colour, facing `normal`, under `light`:
    /// Lambert's law. The surface receives the ambient plus as much of the
    /// key as the cosine between its normal and the direction to the lamp,
    /// nothing from a lamp behind it, and reflects its albedo's share of each
    /// channel. One albedo and one lamp thus give every face of a drawn
    /// solid its colour: the face direction the lamp bright and warm, the face
    /// away in the cool ambient, the slopes in between.
    static constexpr Color illuminate(Color albedo, Light const& light, Direction normal) {
        double const between = Direction::cosineBetween(normal, light.direction);
        double const facing = between < 0.0 ? 0.0 : between;
        impl::LinearRgb const a = impl::linear(albedo), key = impl::linear(light.key),
                                 ambient = impl::linear(light.ambient);
        return impl::encoded({a.r * (ambient.r + key.r * facing), a.g * (ambient.g + key.g * facing),
                              a.b * (ambient.b + key.b * facing)},
                             albedo.A);
    }
};

/// Colours derived from colours. Two kinds of derivation live here.
/// lightness() is perceptual: a palette step that looks the same size on any
/// colour. shade(), highlight() and illuminate() are physical: what a lit
/// surface of that colour does, computed in linear light so that a drawn
/// object shades the way a real one would. Alpha rides along untouched
/// through all of them. Every function is constexpr: written in a constant
/// it is read by the compiler, and an argument out of range fails the build;
/// reached at run time -- from an effect's tags -- the argument is clamped
/// into range instead.
struct color_helper {

    /// The colour moved along perceptual lightness: lightness(graphite, 0.1)
    /// is a step lighter, lightness(graphite, -0.1) the same step darker, and
    /// the two steps look equal whatever the colour -- the move is made in
    /// OKLCH, with hue and chroma kept. Lightness saturates at black and
    /// white; a tone sRGB cannot show at the new lightness loses chroma, not
    /// hue. A delta outside -1..1 fails the build.
    static constexpr Color lightness(Color color, double delta) {
        if (delta < -1.0 || delta > 1.0) {
            if consteval {
                error::color_lightness_delta_must_be_within_minus_1_to_1();
            } else {
                delta = delta < -1.0 ? -1.0 : 1.0;
            }
        }
        impl::Oklab lab = impl::oklab(color);
        lab.L += delta;
        lab.L = lab.L < 0.0 ? 0.0 : lab.L > 1.0 ? 1.0 : lab.L;
        return impl::srgb(lab, color.A);
    }

    /// The colour under `factor` of its light: shade(paint, 0.5) is the paint
    /// with half the light on it, shade(paint, 1.2) with a fifth more, and
    /// shade(paint, 0) is black. A product in linear light, which is what a
    /// shadow is: hue and saturation stay, only the amount of light changes
    /// (in OKLab, lightness and chroma fall together). More light than the
    /// display can show clips at white. A negative factor fails the build.
    static constexpr Color shade(Color albedo, double factor) {
        if (factor < 0.0) {
            if consteval {
                error::color_shade_factor_must_not_be_negative();
            } else {
                factor = 0.0;
            }
        }
        impl::LinearRgb const c = impl::linear(albedo);
        return impl::encoded({c.r * factor, c.g * factor, c.b * factor}, albedo.A);
    }

    /// The colour with `amount` of a light reflected off it: a highlight. A
    /// sum in linear light, which is what a gloss is -- the lamp's own colour
    /// added on top of the surface's -- so the result drifts direction the
    /// light and loses saturation, the opposite of what a shadow does.
    /// highlight(paint, white, 0) is the paint, highlight(anything, white, 1)
    /// is white. An amount outside 0..1 fails the build.
    static constexpr Color highlight(Color albedo, Color light, double amount) {
        if (amount < 0.0 || amount > 1.0) {
            if consteval {
                error::color_highlight_amount_must_be_within_0_to_1();
            } else {
                amount = amount < 0.0 ? 0.0 : 1.0;
            }
        }
        impl::LinearRgb const c = impl::linear(albedo), l = impl::linear(light);
        return impl::encoded({c.r + l.r * amount, c.g + l.g * amount, c.b + l.b * amount}, albedo.A);
    }
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
