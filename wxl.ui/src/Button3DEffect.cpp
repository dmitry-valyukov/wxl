// wxl::Button3DEffect -- the setters behind it; see Button3DEffect.h.

#include "Button3DEffect.h"

#include <algorithm>

#include "BevelEffect.h"
#include "ThemeBrush.h"
#include "Thickness.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "impl/member.h"

namespace wxl {

struct Button3DEffect::State : core::sta_refcounted {
    Color ink = rgb(237, 239, 242);
    Color albedo = rgb(68, 72, 79);
    double shadow = 0.7;
    double emboss = -1.0;
};

Button3DEffect::Button3DEffect() : state_{new State, /*add_ref=*/false} {}
Button3DEffect::Button3DEffect(Button3DEffect const& other) noexcept = default;
Button3DEffect& Button3DEffect::operator=(Button3DEffect const& other) noexcept = default;
Button3DEffect::~Button3DEffect() = default;

void Button3DEffect::foreground(Color value) const { state_->ink = value; }
void Button3DEffect::background(Color value) const { state_->albedo = value; }
void Button3DEffect::shadow(double value) const { state_->shadow = std::clamp(value, 0.0, 1.0); }
void Button3DEffect::emboss(double value) const { state_->emboss = std::clamp(value, -1.0, 1.0); }

namespace {

// The light comes from the top left: 135 degrees round the screen, counted
// the classical way, 0 to the right and 90 straight up. The one thing the
// light decides is where the spot sits on the face -- this far from the
// middle, in the light's direction -- which puts it at about (0.2, 0.2).
// The face in profile is a shallow round dish (emboss < 0) or dome (> 0): a
// dish is dark in the spot, where its surface turns away from the light,
// and light in the opposite corner, where the surface turns towards it; a
// dome the other way round.
constexpr double lightAzimuth = 105.0;
constexpr double spotDistance = 0.67;

constexpr Point spot() {
    double const a = impl::radians(lightAzimuth);
    return {static_cast<float>(0.5 + spotDistance * impl::cosine(a)),
            static_cast<float>(0.5 - spotDistance * impl::sine(a))};
}

struct Face {
    Color nearLamp, middle, farFromLamp;
};

// Two brushes, a dark one and a light one, over the button's own colour:
// the schematic rendering of relief. The dark brush is the colour pulled
// towards black by `strength`, the light brush the colour pulled towards
// white by as much, both in the encoded values the screen shows, so that the
// two steps look alike -- a disc shaded from the one to the other over a
// panel of the middle tone is the classic dent or bump. `strength` is the
// curvature times `shadow`: a full dish under hard light runs nearly black
// to nearly white, a shallow one a little either way.
constexpr Color towardsBlack(Color c, double strength) {
    auto const pull = [strength](uint8_t v) { return static_cast<uint8_t>(v * (1.0 - strength) + 0.5); };
    return {c.A, pull(c.R), pull(c.G), pull(c.B)};
}

constexpr Color towardsWhite(Color c, double strength) {
    auto const pull = [strength](uint8_t v) { return static_cast<uint8_t>(v + (255.0 - v) * strength + 0.5); };
    return {c.A, pull(c.R), pull(c.G), pull(c.B)};
}

constexpr Face face(Color albedo, double shadow, double emboss) {
    double const depth = emboss < 0.0 ? -emboss : emboss;
    double const strength = impl::root(depth, 2) * shadow;  // a shallow plate already shows: the shade grows fast, then levels
    Color const dark = towardsBlack(albedo, strength*1.25);
    Color const light = towardsWhite(albedo, strength*0.75);
    // A dish is dark under the lamp and light opposite; a dome the other way round.
    return emboss < 0.0 ? Face{dark, albedo, light} : Face{light, albedo, dark};
}

// The bevel: the body of the key, raised out of the panel whatever its face
// does, so its rim is lit on the top and left and shaded on the bottom and
// right, always, at full strength -- the same rim the calculator wore before
// the effect existed. The dish or dome is the face's business, not the
// rim's. Translucent, over whatever colour the button is.
constexpr Color bevelLight = rgba(255, 255, 255, 0.55);
constexpr Color bevelShade = rgba(0, 0, 0, 0.75);

// The face: a spot of the near tone where the light puts it, the button's
// own colour in the middle, and the opposite spot of the far tone in the
// opposite corner. That corner lies about 0.6 of the radius from the origin,
// so the far tone is reached before it, at 0.7, and the button's own colour
// between the two, at 0.4. For a dish the near spot is the shadow and the far
// one the light, for a dome the other way round. No gleam on the face: this
// is a schematic rendering, and the light brush of the bevel is the gleam.
RadialGradientBrush faceBrush(Face const& tones) {
    return RadialGradientBrush{
        dsl::center = {0.5, 0.5},
        dsl::gradientOrigin = spot(),
        dsl::radiusX = 1.85,
        dsl::radiusY = 1.15,
        GradientStop{tones.nearLamp, dsl::offset = 0.0},
        GradientStop{tones.middle, dsl::offset = 0.4},
        GradientStop{tones.farFromLamp, dsl::offset = 0.7},
    };
}

}  // namespace

void Button3DEffect::wear(Button const& button) const {
    State const& s = *state_;

    // Under the pointer a quarter more light falls on the button; pressed, it
    // goes down and loses a fifth, and its glyphs dim with it.
    Face const rest = face(s.albedo, s.shadow, s.emboss);
    Face const hover = face(color_helper::shade(s.albedo, 1.25), s.shadow, s.emboss);
    Face const pressed = face(color_helper::shade(s.albedo, 0.8), s.shadow, s.emboss);

    Apply{
        button,
        dsl::foreground = SolidColorBrush{s.ink},
        dsl::background = faceBrush(rest),
        ThemeBrush{u"ButtonBackgroundPointerOver", faceBrush(hover)},
        ThemeBrush{u"ButtonBackgroundPressed", faceBrush(pressed)},
        ThemeBrush{u"ButtonForegroundPointerOver", SolidColorBrush{s.ink}},
        ThemeBrush{u"ButtonForegroundPressed", SolidColorBrush{color_helper::shade(s.ink, 0.55)}},
        ThemeBrush{u"ButtonBorderBrushPointerOver", SolidColorBrush{colors.black}},
        ThemeBrush{u"ButtonBorderBrushPressed", SolidColorBrush{colors.black}},
        dsl::borderBrush = SolidColorBrush{colors.black},
        BorderThickness{1},
        BevelEffect{bevelLight, bevelShade, Margin{1.5}, dsl::strokeThickness = 1, dsl::offset = 2},
    };
}

}  // namespace wxl
