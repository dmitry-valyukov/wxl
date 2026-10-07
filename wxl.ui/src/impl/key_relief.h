#pragma once

// The shape of a 3D key, as pictures the compositor lights: a body raised
// over the panel, with the facets of a rim along its edge and a face inside
// them, a dish or a dome. The shape is written once as a normal map -- a
// direction per pixel -- and a SceneLightingEffect reads it; the rest are
// masks cut from the same formula: the whole key, the face without the rim,
// and the shadow the edge of the panel's opening throws on a key pressed
// below the panel. Everything is in layout units; the surface may be any
// density, and the picture fills it.
//
// Included by Button3DEffect.cpp after the projection; the lighting lab in
// sandbox/ draws its keys with the same functions.

#include "../DrawingSurface.h"

namespace wxl::impl {

struct KeyRelief {
    double width = 0.0;
    double height = 0.0;
    double corner = 0.0;   // rounding of the outline
    double rim = 0.0;      // width of the rim
    double slope = 1.0;    // tangent of the rim's slope, outwards
    bool rounded = false;  // the rim a quarter circle rather than a flat chamfer
    double emboss = 0.0;   // the face: a dish below zero, flat, a dome
    double smooth = 0.0;   // width of the band at the join of face and rim where one slope turns into the other
    double shiftX = 0.0;   // how far the inner part -- the face and the inner edge of the rim --
    double shiftY = 0.0;   // is moved right and down; the outer edge of the rim stays
};

// A pressed key in the panel's opening: the body has gone down, and the
// points of the rim that are now below the panel are hidden from the
// distant lamp by the edge of the opening. Ambient light is not touched.
struct HoleShadow {
    double depth = 0.0;      // how far the outer edge of the rim is below the panel
    double azimuth = 0.0;    // the lamp: where the light comes from, degrees; 0 to the right, 90 up the screen
    double elevation = 0.0;  // the lamp: above the horizon, degrees
    double share = 0.0;      // the lamp's share of the light on a flat spot; ambient is the rest
    double amount = 1.0;     // how much weaker than full the shadow is: one takes the lamp's whole share
    double soft = 0.0;       // width of the penumbra along the shadow's edge
};

// The normal map of the key, over the whole surface. Green grows up the
// screen, as in Microsoft's sample maps.
void draw_key_map(DrawingSurface const& surface, KeyRelief const& key);

// The mask of the face: white without the rim. Its edge runs along the join
// of face and rim and is blurred over the same `smooth` as the slope in the
// map, but never under one pixel of the surface.
void draw_face_mask(DrawingSurface const& surface, KeyRelief const& key);

// The mask of the opening's shadow: black with the share of light the shadow
// takes for alpha, clear where the light reaches. The face counts as flat at
// the height of the rim's inner edge.
void draw_hole_shadow(DrawingSurface const& surface, KeyRelief const& key, HoleShadow const& shadow);

// A white rounded rectangle filling the surface: the mask of the whole key.
// The radius is in pixels of the surface.
void draw_shape(DrawingSurface const& surface, float radius);

}  // namespace wxl::impl
