#pragma once

// What DrawingSurface.cpp knows about losing the device it draws through: which answers of
// the drawing calls name a lost device, and a way to pretend it went. Private to the build
// of wxl and its tests -- the public header says only what an owner does about it
// (DrawingSurface::throwIfDeviceLost, RenderingDeviceWatch).

#include "platform.h"

#include <cstdint>

namespace wxl::impl {

/// Whether a drawing call's answer names a lost device: DXGI's removal and reset, and
/// Direct2D's "recreate the target", which is how Direct2D says either. The one list of
/// them -- the surface's BeginDraw and EndDraw read it, and so does every Direct2D call an
/// application makes inside a paint, through DrawingSurface::throwIfDeviceLost.
///
/// A code alone is not a lost device: DrawingSurface.cpp takes it as one only when the
/// device itself says it was removed (GetDeviceRemovedReason), and a lost-device code from
/// a device that is not lost stops the process.
constexpr bool device_loss_code(std::int32_t result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
           result == D2DERR_RECREATE_TARGET;
}

/// Pretends that the device every DrawingSurface draws through was lost: the replacement
/// then runs as after a real loss, from the system's thread pool to the UI thread -- a new
/// device, every graphics device moved onto it, every RenderingDeviceWatch called. What it
/// cannot pretend is the old device failing: drawing on it goes on succeeding until the
/// replacement. For seeing an application redraw on a machine -- from a debugger's
/// immediate window or a key of a debug build, on the UI thread. Stops the process if no
/// surface has been made yet: there is no device to lose.
void signal_rendering_device_removed();

}  // namespace wxl::impl
