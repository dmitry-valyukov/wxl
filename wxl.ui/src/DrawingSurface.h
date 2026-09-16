#pragma once

#include "generated/Microsoft.UI.Composition.h"

// wxl::DrawingSurface -- a rectangle of pixels the application draws itself
// and the compositor shows.
//
// Everything a XAML control puts on the screen is drawn by the framework.
// This is the other case: the application has drawing of its own to do -- a
// page of a book, glyph by glyph -- and needs somewhere to put it that the
// compositor will then move, turn and fade like any other visual.
//
// Getting there is a chain of four links, and every one of them is interop
// rather than metadata: a D3D11 device, a D2D device on top of it, a
// CompositionGraphicsDevice made from that through ICompositorInterop, and
// BeginDraw on the surface, which hands out an ID2D1DeviceContext. None of
// it can be said in a profile, and all of it is the same four links in every
// application -- which is why it is here instead of in one.
//
// What is left for the application is the last link alone:
//
//     DrawingSurface page{compositor, {pixelWidth, pixelHeight}};
//     page.draw([&](ID2D1DeviceContext* context) { ...your D2D... });
//
//     SpriteVisual sprite = compositor.createSpriteVisual();
//     sprite.brush(page.brush());
//     sprite.size({width, height});
//     ElementCompositionPreview::setElementChildVisual(element, sprite);
//
// Pixels, not DIPs. A surface is allocated in real pixels, so its size is
// the element's size times XamlRoot::rasterizationScale() and the drawing
// scales by the same factor -- without that the page is soft at 150%, which
// is the whole reason the scale is in the rich profile.
//
// A wrapper, not an owner: copying one gives a second handle to the same
// surface, the way every composition object behaves.

// Declared, not included. d2d1_1.h is the application's business, and a
// pointer to an incomplete type is all a std::function signature needs --
// so an application that only animates surfaces never parses Direct2D.
struct ID2D1DeviceContext;

namespace wxl {

class DrawingSurface {
public:
    /// Allocates a surface of that many pixels.
    ///
    /// A side of zero or less is allowed and yields a surface nothing can be
    /// drawn into: that is what an element which has not been measured yet
    /// is, and refusing it would only move the check into every caller.
    DrawingSurface(Compositor const& compositor, SizeInt32 sizePixels);

    SizeInt32 size() const;

    /// The same surface at another size. Cheaper than a new one and, more to
    /// the point, every brush already showing it goes on showing it.
    void resize(SizeInt32 sizePixels);

    /// Draws the whole surface.
    ///
    /// The context arrives cleared to transparent and with its transform
    /// already set for wherever in the shared atlas this surface landed --
    /// forgetting that offset is the classic way to draw a page that lands
    /// on top of somebody else's.
    void draw(std::function<void(ID2D1DeviceContext*)> const& paint) const;

    /// A brush showing this surface, for a sprite visual to wear.
    ///
    /// Made fresh on each call, because a brush is cheap and holding one
    /// here would be a second thing to keep in step with resize().
    CompositionSurfaceBrush brush() const;

private:
    Compositor compositor_;
    CompositionDrawingSurface surface_;
};

}  // namespace wxl
