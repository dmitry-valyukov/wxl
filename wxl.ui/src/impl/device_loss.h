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

/// Sets the event the device every DrawingSurface draws through sets when it is removed: the
/// replacement then runs as after a real loss, from the system's thread pool to the UI thread --
/// a new device, every graphics device moved onto it, every RenderingDeviceWatch called. For a
/// device under wxl's surfaces that wxl does not watch, found lost by its owner -- Win2D's, under
/// TextureCache -- and for seeing an application redraw on a machine, from a debugger's
/// immediate window or a key of a debug build; on the UI thread. What it cannot pretend is the
/// old device failing: drawing on it goes on succeeding until the replacement. Stops the
/// process if no surface has been made yet: there is no device to lose. Once the UI thread's
/// queue has begun to shut down it does nothing: the device is no longer replaced.
void signal_rendering_device_removed();

/// How many times the device every DrawingSurface draws through has been replaced. What a
/// holder of things made on another device under wxl's surfaces -- Win2D's, under
/// TextureCache -- compares, to learn that its things are empty and it has to follow.
std::uint64_t rendering_device_replacements() noexcept;

/// Makes that device if there is none yet, so that a loss of the adapter under it is heard:
/// for TextureCache, whose own device wxl does not watch. On the UI thread.
void ensure_rendering_device();

}  // namespace wxl::impl
