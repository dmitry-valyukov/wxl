#pragma once

// wxl::relief_helper -- the colours of a drawn solid: what a raised or sunken
// surface of one colour looks like under one lamp, and what a flat panel
// looks like under a lamp that hangs over it. Built on color_helper, constexpr
// like it (read by the compiler in a constant, computed at run time from an
// effect's tags, out-of-range arguments failing the build in the one case and
// clamped in the other), and meant to be what a pseudo-3D control computes
// its style from -- a keypad, a bevelled rim, a lit panel -- so that every
// part of the picture is shaded by the same light, and so that a hover or a
// press is a change of light, not a second palette.
//
//     constexpr Light lamp {Direction::from_angles(120, 50), rgb(255, 246, 232), rgb(140, 146, 168)};
//     constexpr auto edges = relief_helper::raised(plastic, lamp);
//     BevelEffect {edges.nearLamp, edges.farFromLamp}
//
// The lamp is a direction and two colours; every face of a relief is that
// lamp on the surface's albedo by Lambert's law, so the lit side is warm
// with the key light, the shaded side cool with the ambient, and a change of
// lamp re-lights the whole scene.

#include <array>
#include <cstddef>

#include "Color.h"

namespace wxl {

namespace error {
consteval void relief_tilt_must_not_be_negative();
consteval void relief_rise_must_be_within_minus_1_to_1();
consteval void relief_lamp_height_must_be_positive();
consteval void relief_distance_must_not_be_negative();
}  // namespace error

struct relief_helper {
    using Direction = ::wxl::Direction;
    using Light = ::wxl::Light;

    /// The two sloping faces of a relief, named by where they are and not by
    /// how they look: the one on the lamp's side and the one away from it.
    /// This is the order a BevelEffect takes them in when the lamp stands
    /// where it usually does, above and to the left.
    struct Edges {
        Color nearLamp, farFromLamp;
    };

    /// A surface of this colour rising towards the viewer: the face on the
    /// lamp's side turns to the lamp and is lit, the face away from it turns
    /// away and is in the ambient. `tilt` is how far a face's normal leans
    /// from the viewer's axis, across per unit of rise: 0 is a face as flat
    /// as the top, 1 is forty-five degrees, more is steeper. A negative tilt
    /// fails the build.
    static constexpr Edges raised(Color albedo, Light const& lamp, double tilt = 0.7) {
        if (tilt < 0.0) {
            if consteval {
                error::relief_tilt_must_not_be_negative();
            } else {
                tilt = 0.0;
            }
        }
        Direction const lean = leanTowards(lamp.direction, tilt);
        return {Light::illuminate(albedo, lamp, lean),
                Light::illuminate(albedo, lamp, {-lean.x, -lean.y, lean.z})};
    }

    /// A surface of this colour sinking away from the viewer: the wall on the
    /// lamp's side faces away from it and is in the ambient, the far wall
    /// faces the lamp and is lit -- the raised relief with its faces swapped,
    /// which is all that tells a dent from a boss.
    static constexpr Edges sunken(Color albedo, Light const& lamp, double tilt = 0.7) {
        Edges const up = raised(albedo, lamp, tilt);
        return {up.farFromLamp, up.nearLamp};
    }

    /// A flat face of this colour, square to the viewer, under the lamp: the
    /// top of a raised relief or the floor of a sunken one.
    static constexpr Color flat(Color albedo, Light const& lamp) {
        return Light::illuminate(albedo, lamp, {0.0, 0.0, 1.0});
    }

    /// Where a face's lamp side and far side are, in the face's own
    /// coordinates, 0..1 across and down: the start and end of a linear
    /// gradient between two Edges. The axis runs through the middle of the
    /// face along the lamp's direction across the screen; a lamp straight
    /// overhead has none, and then both ends are the middle.
    struct Axis {
        float startX, startY, endX, endY;
    };

    static constexpr Axis across(Light const& lamp) {
        Direction const t = lamp.direction;
        if (overhead(t)) return {0.5f, 0.5f, 0.5f, 0.5f};
        double const length = impl::root(t.x * t.x + t.y * t.y, 2);
        float const x = static_cast<float>(0.5 * t.x / length), y = static_cast<float>(0.5 * t.y / length);
        return {0.5f + x, 0.5f + y, 0.5f - x, 0.5f - y};
    }

    /// The shoulder of a cushion, as rings: a key with a flat top and
    /// rounded edges, seen in profile, is a flat and then an arc that turns
    /// from the top down towards the side. `rise` is how far the arc turns,
    /// as a share of a quarter turn: 0 is a flat key, whose shoulder is the
    /// top's own colour all the way; 1 is the full quarter round, whose outer
    /// edge stands vertical; a negative rise is the same shoulder sunk into
    /// the surface, the dent instead of the cushion. The rings are equal in
    /// width and come outermost first, the way a BevelEffect draws them, each
    /// coloured at the angle the arc has at the ring's middle.
    template <std::size_t N = 6>
    static constexpr std::array<Edges, N> cushion(Color albedo, Light const& lamp, double rise) {
        if (rise < -1.0 || rise > 1.0) {
            if consteval {
                error::relief_rise_must_be_within_minus_1_to_1();
            } else {
                rise = rise < -1.0 ? -1.0 : 1.0;
            }
        }
        double const turn = impl::sine(impl::radians((rise < 0.0 ? -rise : rise) * 90.0));
        std::array<Edges, N> rings{};
        for (std::size_t k = 0; k < N; ++k) {
            // Along the arc the sine of the angle is the distance from the
            // top's edge, so a ring's angle is the arcsine of its place.
            double const place = 1.0 - (k + 0.5) / N;
            double const angle = impl::arcsine(place * turn);
            Edges const up = raised(albedo, lamp, impl::sine(angle) / impl::cosine(angle));
            rings[k] = rise < 0.0 ? Edges{up.farFromLamp, up.nearLamp} : up;
        }
        return rings;
    }

    /// How much of the lamp's light reaches a flat surface `distance` from
    /// the lamp's foot, when the lamp hangs `height` above it, as a share
    /// of the light right under the lamp: the inverse square of the
    /// distance to the lamp and the cosine of the angle it arrives at,
    /// (h² / (h² + d²))^(3/2). Only the ratio of the two matters.
    static constexpr double falloff(double distance, double height) {
        if (height <= 0.0) {
            if consteval {
                error::relief_lamp_height_must_be_positive();
            } else {
                return 0.0;  // a lamp on the surface lights nothing beside it
            }
        }
        if (distance < 0.0) {
            if consteval {
                error::relief_distance_must_not_be_negative();
            } else {
                distance = -distance;
            }
        }
        double const q = height * height / (height * height + distance * distance);
        return q * impl::root(q, 2);
    }

    /// A flat surface of this colour under a lamp that hangs `height` above
    /// it, at `distance` from the lamp's foot. The key falls off with the
    /// distance, the ambient reaches everywhere alike: the tones of a lit
    /// panel, for a radial gradient whose origin is the lamp's foot.
    static constexpr Color under(Color albedo, Light const& lamp, double distance, double height) {
        Light const here {{0.0, 0.0, 1.0}, color_helper::shade(lamp.key, falloff(distance, height)),
                          lamp.ambient};
        return Light::illuminate(albedo, here, {0.0, 0.0, 1.0});
    }

private:
    // The normal of a face that leans towards the lamp by `tilt`: the lamp's
    // direction across the screen, scaled to the tilt, with unit rise. A lamp
    // straight overhead has no direction across the screen, and then every
    // face is the flat one.
    static constexpr Direction leanTowards(Direction towards, double tilt) {
        if (overhead(towards)) return {0.0, 0.0, 1.0};
        double const across = impl::root(towards.x * towards.x + towards.y * towards.y, 2);
        return {towards.x / across * tilt, towards.y / across * tilt, 1.0};
    }

    // A lamp with no direction across the screen worth speaking of: written
    // as towards(_, 90), its cosine of ninety degrees is not a clean zero but
    // rounding, and a direction made of rounding is no direction.
    static constexpr bool overhead(Direction towards) {
        double const across = towards.x * towards.x + towards.y * towards.y;
        return across <= 1e-18 * (across + towards.z * towards.z);
    }
};

}  // namespace wxl
