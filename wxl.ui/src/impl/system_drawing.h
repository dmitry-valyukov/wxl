#pragma once

// What DrawingSurface.cpp does for the one part of wxl that composes on the system compositor
// (Windows.UI.Composition) rather than WinUI's: WindowShade, whose flight goes outside any window,
// on a DesktopWindowTarget of a bare HWND. Its graphics device is made on the rendering device
// every DrawingSurface draws through and moved with them on a replacement, its surfaces are drawn
// the way DrawingSurface::draw draws, and its owner hears the replacement through a watch of the
// same kind as RenderingDeviceWatch.
//
// The compositor is named by its COM identity, which the caller holds; the system projection is
// the caller's, so nothing of it is parsed here. Private to the build of wxl and its tests, after
// the Windows headers, which bring IUnknown and IInspectable.

#include <functional>

#include "rendering_device.h"

struct ID2D1DeviceContext;

namespace wxl::impl {

/// A system compositor, by the IUnknown its QueryInterface answers -- the same pointer through
/// whichever interface it is reached.
struct system_compositor {
    ::IUnknown* identity;
};

/// The compositor's graphics device on the rendering device in use, made the first time it is
/// asked for and kept: wxl holds it for good and moves it onto every new device. Borrowed -- the
/// caller takes a reference of its own.
::IInspectable* system_graphics_device(system_compositor compositor);

/// DrawingSurface::draw for a surface of that graphics device, given as its IUnknown: false when
/// nothing was drawn -- no paint, or the device is lost, and the watch below is called once the new
/// one stands -- and any other failure thrown.
bool draw_system_surface(::IUnknown* surface, std::function<void(ID2D1DeviceContext*)> const& paint);

/// The pair of calls a system_device_watch subscribes through, as rendering_device_replaced is
/// for a RenderingDeviceWatch.
struct system_rendering_device_replaced {
    static EventToken add(system_compositor compositor, EventHandler<Object> handler);
    static bool remove(system_compositor compositor, EventToken token);
};

/// RenderingDeviceWatch for an owner of surfaces on a system compositor.
template <auto Listener>
using system_device_watch = device_watch<system_compositor, system_rendering_device_replaced, Listener>;

}  // namespace wxl::impl
