#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <span>

#include <wxl/Microsoft.UI.Composition.h>

#include "impl/rendering_device.h"

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
//
// One surface is one texture, and a texture has a longest side the device
// allows -- 16384 pixels at best. Something longer, a long text in a
// scroller, is a VirtualDrawingSurface (below): the boundaries of the whole,
// pixels only for the parts drawn, each part drawn within one texture.
//
// The device can be lost -- a driver update, a GPU reset after a timeout, a
// session moved to another adapter -- and that is an ordinary event, not a
// failure: the visual tree stands, the pixels of every surface do not, and
// neither does anything the application made from a context of the old
// device (a bitmap, a brush, an effect, a context of its own). wxl replaces
// the device on its own; draw() meanwhile skips and answers false -- a
// Direct2D call inside the paint that the lost device refused ends the paint
// through throwIfDeviceLost -- and a RenderingDeviceWatch tells the owner
// when to drop what it made and draw again. The watch is called from a turn
// of the UI thread's queue of its own, never from inside a call of the
// application's -- one that makes a surface while the device is lost
// included -- and until it has been called, draw() goes on answering false:
//
//     class Page {
//         void deviceReplaced() noexcept { bitmap_ = nullptr; redraw(); }
//         ...
//         wxl::RenderingDeviceWatch<&Page::deviceReplaced> deviceWatch_;
//     };
//
//     Page::Page(Compositor const& compositor) : deviceWatch_{compositor, *this} {}
//
// A replacement moves the graphics device of every compositor a surface was
// made on, so such a compositor is not closed before the UI thread's queue
// begins to shut down -- XAML closes its own then. From that moment the device
// is no longer replaced and no watch is called: a loss on the way out leaves
// draw() answering false.

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
    ///
    /// False when nothing was drawn: there is nothing to draw on (a side of
    /// zero, no paint), or the device is lost -- then `paint` did not run, or
    /// was ended by DeviceLost, or ran to the end and its pixels went with the
    /// device -- or it is new and its watches have not been called yet; every
    /// RenderingDeviceWatch is called once the new one stands. Any other
    /// failure of the drawing throws.
    bool draw(std::function<void(ID2D1DeviceContext*)> const& paint) const;

    /// What ends a paint whose Direct2D call the lost device refused: thrown by
    /// throwIfDeviceLost, caught by draw(), which then answers false. Outside
    /// a paint nobody catches it, and the process stops naming it.
    class DeviceLost : public std::exception {
    public:
        char const* what() const noexcept override { return "wxl: the rendering device is lost"; }
    };

    /// For the answer of a Direct2D call made inside a paint, before the
    /// caller decides what a failure means: throws DeviceLost when the answer
    /// names a lost device and the device confirms it was removed. An answer
    /// that names a lost device from a device that is not lost stops the
    /// process -- draw() would skip for ever, and no replacement would come.
    /// Any other answer returns.
    ///
    /// `result` is the HRESULT the Direct2D or DXGI call returned, spelled as
    /// the 32-bit integer it is so that this header need not parse a Windows
    /// one: a DrawingSurface speaks Direct2D anyway -- the paint is handed an
    /// ID2D1DeviceContext.
    static void throwIfDeviceLost(std::int32_t result);

    /// A brush showing this surface, for a sprite visual to wear.
    ///
    /// Made fresh on each call, because a brush is cheap and holding one
    /// here would be a second thing to keep in step with resize().
    CompositionSurfaceBrush brush() const;

private:
    Compositor compositor_;
    CompositionDrawingSurface surface_;
};

/// A surface taller or wider than any texture of the device, drawn and kept in parts --
/// the platform's CompositionVirtualDrawingSurface: a long document in a scroller, of which
/// only what is near the view needs pixels.
///
/// Nothing is allocated until a part is drawn, and only what is drawn holds memory; trim()
/// gives back what is no longer near the view. A part that was never drawn, or was trimmed,
/// shows nothing.
///
/// The rest is DrawingSurface's: pixels, not DIPs; a wrapper, not an owner; the same device,
/// so the same loss -- draw() answers false on a lost device, a Direct2D call inside the
/// paint hands its answer to DrawingSurface::throwIfDeviceLost, and a RenderingDeviceWatch
/// on the compositor is called once the new device stands. Every pixel goes with the old
/// device: the owner draws again what is near the view.
class VirtualDrawingSurface {
public:
    /// The longest side a virtual surface may have, 2^24 pixels: the remarks of
    /// CompositionVirtualDrawingSurface set it from the float precision of the coordinates,
    /// and IDCompositionDevice::CreateVirtualSurface names it as the largest width and height.
    static constexpr std::int32_t maxSide = 1 << 24;

    /// Allocates a surface of that many pixels -- the boundaries, not the memory. A side of
    /// zero or less is a surface nothing can be drawn into, as with DrawingSurface; a side
    /// over maxSide stops the process.
    VirtualDrawingSurface(Compositor const& compositor, SizeInt32 sizePixels);

    SizeInt32 size() const;

    /// New boundaries for the same surface: what was drawn outside them is given back, and
    /// every brush already showing it goes on showing it. A side over maxSide stops the
    /// process.
    void resize(SizeInt32 sizePixels);

    /// The longest side of one draw(): an update is drawn into one texture of the device,
    /// whose side the device's feature level bounds, and composition keeps three pixels of
    /// that side for itself (gutters and a white pixel) -- 16381 pixels from 11_0 up, 8189 at
    /// 10_x, 4093 at 9_3, 2045 at 9_1 and 9_2. The device's answer, so it may change when the
    /// device is replaced: read it again in the RenderingDeviceWatch listener.
    std::int32_t maxUpdateSide() const;

    /// Draws one part of the surface, `updatePixels` cut to the surface's boundaries.
    ///
    /// The context arrives cleared to transparent within the part, clipped to it, and with
    /// its transform set so that the paint draws in the coordinates of the whole surface:
    /// a line at y = 40000 is drawn at y = 40000, whatever part of the texture the
    /// compositor handed out for it.
    ///
    /// The part, once cut, must be no longer than maxUpdateSide() on either side; a longer
    /// one stops the process. The limit is the caller's to keep -- splitting the part here
    /// would hide a caller who thinks the device draws more at once than it does.
    ///
    /// False when nothing was drawn: the part is empty once cut, there is no paint, or the
    /// device is lost or new and its watches have not been called yet -- as
    /// DrawingSurface::draw says. Any other failure throws.
    bool draw(RectInt32 updatePixels, std::function<void(ID2D1DeviceContext*)> const& paint) const;

    /// Keeps the pixels of these parts and gives back the memory of every other: what is
    /// left out shows nothing until it is drawn again. Nothing to keep is every pixel
    /// given back. Not from inside a paint.
    void trim(std::span<RectInt32 const> keepPixels) const;

    /// A brush showing this surface, for a sprite visual to wear: made fresh on each call,
    /// as DrawingSurface::brush is.
    CompositionSurfaceBrush brush() const;

private:
    Compositor compositor_;
    CompositionDrawingSurface surface_;
};

namespace impl {

/// The pair of calls a RenderingDeviceWatch subscribes through: the handlers of one
/// compositor's graphics device -- the one every DrawingSurface on it is allocated from.
/// wxl calls them itself once every graphics device has the new rendering device. Tokens
/// come from the counter the window's events share, and remove() answers whether the token
/// was there.
struct rendering_device_replaced {
    static EventToken add(Compositor const& compositor, EventHandler<Object> handler);
    static bool remove(Compositor const& compositor, EventToken token);
};

}  // namespace impl

/// Holds the subscription of a member of its owner to the replacement of the device every
/// DrawingSurface of one compositor draws through: the listener -- a noexcept member that
/// takes nothing -- is called once the new device stands, and is where the owner drops
/// every bitmap, brush, effect and context it made from the old one and draws its
/// surfaces again. A member of the owner, after what the listener touches; attached
/// with the compositor and the owner from the start or by attach() once, taken off by its
/// own end. Not copied, not moved.
template <auto Listener>
using RenderingDeviceWatch = impl::device_watch<Compositor, impl::rendering_device_replaced, Listener>;

}  // namespace wxl
