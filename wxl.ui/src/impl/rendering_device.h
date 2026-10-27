#pragma once

// impl::rendering_device -- the rendering device every graphics device of wxl draws through, one
// for the process: which answers name it lost, how much it draws at once, its replacement as an
// owner of foreign things on it follows it, and the graphics devices of the one part of wxl that
// composes on the system compositor. Static members alone, over the one store DrawingSurface.cpp
// keeps (impl::rendering_devices). Private to the build of wxl and its tests -- the public headers
// say only what an owner does about it (DrawingSurface::throwIfDeviceLost, RenderingDeviceWatch,
// VirtualDrawingSurface::maxUpdateSide).
//
// After the Windows headers, which bring IUnknown and IInspectable, and after the standard ones:
// the watch below brings wxl.core in.

#include "platform.h"

#include <d3dcommon.h>

#include <cstdint>
#include <functional>

#include "event_watch.h"

struct ID2D1DeviceContext;

namespace wxl::impl {

/// A system compositor (Windows.UI.Composition), by the IUnknown its QueryInterface answers -- the
/// same pointer through whichever interface it is reached. WindowShade's flight goes outside any
/// window, on a DesktopWindowTarget of a bare HWND, so it composes there rather than on WinUI's;
/// the system projection is the caller's, so nothing of it is parsed here.
struct system_compositor {
    ::IUnknown* identity;
};

class rendering_device
{
public:
    rendering_device() = delete;

    /// Whether a drawing call's answer names a lost device: DXGI's removal and reset, and
    /// Direct2D's "recreate the target", which is how Direct2D says either. The one list of them
    /// -- the surface's BeginDraw and EndDraw read it, and so does every Direct2D call an
    /// application makes inside a paint, through DrawingSurface::throwIfDeviceLost.
    ///
    /// A code alone is not a lost device: DrawingSurface.cpp takes it as one only when the device
    /// itself says it was removed (GetDeviceRemovedReason), and a lost-device code from a device
    /// that is not lost stops the process.
    static constexpr bool is_device_loss_code(std::int32_t result) noexcept {
        return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
               result == D2DERR_RECREATE_TARGET;
    }

    /// The longest side of a 2D texture at a Direct3D feature level, from the table of
    /// "Direct3D feature levels" (Max Texture Dimension): 16384 from 11_0 up, 8192 at 10_0 and
    /// 10_1, 4096 at 9_3, 2048 at 9_1 and 9_2.
    static constexpr std::int32_t max_texture_side(D3D_FEATURE_LEVEL level) noexcept {
        if (level >= D3D_FEATURE_LEVEL_11_0) return 16384;
        if (level >= D3D_FEATURE_LEVEL_10_0) return 8192;
        if (level >= D3D_FEATURE_LEVEL_9_3) return 4096;
        return 2048;
    }

    /// The pixels an allocation of a composition surface is padded with in its texture: two of
    /// gutters and one "white pixel". So WinUI says of its own surfaces, taking them off the
    /// texture side (DCompTreeHost::GetMaxTextureSize; CCoreServices::GetMaxTextureSize answers
    /// 2048 - 3 when it cannot ask), and Microsoft's VirtualSurfaces sample, composition without
    /// XAML, clamps every BeginDraw update to 2048 - 3 ("3 is the buffer").
    static constexpr std::int32_t composition_padding = 3;

    /// The longest side of one update of a composition surface: an update is drawn into one
    /// texture of the device (BeginDraw hands out one target at one offset), and composition
    /// keeps composition_padding of its side.
    static constexpr std::int32_t max_update_side(D3D_FEATURE_LEVEL level) noexcept {
        return max_texture_side(level) - composition_padding;
    }

    /// Sets the event the device sets when it is removed: the replacement then runs as after a
    /// real loss, from the system's thread pool to the UI thread -- a new device, every graphics
    /// device moved onto it, every RenderingDeviceWatch called. For a device under wxl's surfaces
    /// that wxl does not watch, found lost by its owner -- Win2D's, under TextureCache -- and for
    /// seeing an application redraw on a machine, from a debugger's immediate window or a key of
    /// a debug build; on the UI thread. What it cannot pretend is the old device failing: drawing
    /// on it goes on succeeding until the replacement. Stops the process if no surface has been
    /// made yet: there is no device to lose. Once the UI thread's queue has begun to shut down it
    /// does nothing: the device is no longer replaced.
    static void signal_removed();

    /// How many times the device has been replaced. What a holder of things made on another
    /// device under wxl's surfaces -- Win2D's, under TextureCache -- compares, to learn that its
    /// things are empty and it has to follow.
    static std::uint64_t replacements() noexcept;

    /// Makes the device if there is none yet, so that a loss of the adapter under it is heard:
    /// for TextureCache, whose own device wxl does not watch. On the UI thread.
    static void ensure();

    /// The system compositor's graphics device on the device in use, made the first time it is
    /// asked for and kept: wxl holds it for good and moves it onto every new device. Borrowed --
    /// the caller takes a reference of its own.
    static ::IInspectable* system_graphics_device(system_compositor compositor);

    /// DrawingSurface::draw for a surface of that graphics device, given as its IUnknown: false
    /// when nothing was drawn -- no paint, or the device is lost or new and its watches not called
    /// yet, and system_device_watch is called once the new one stands -- and any other failure
    /// thrown.
    static bool draw_system_surface(::IUnknown* surface,
                                    std::function<void(ID2D1DeviceContext*)> const& paint);
};

/// The pair of calls a system_device_watch subscribes through, as rendering_device_replaced
/// (DrawingSurface.h) is for a RenderingDeviceWatch.
struct system_rendering_device_replaced {
    template <class>
    using args_t = Object;

    static EventToken add(system_compositor compositor, EventHandler<Object> handler);
    static bool remove(system_compositor compositor, EventToken& token);
};

/// RenderingDeviceWatch for an owner of surfaces on a system compositor.
template <auto Listener>
using system_device_watch =
    event_watch<system_compositor, system_rendering_device_replaced, Listener>;

}  // namespace wxl::impl
