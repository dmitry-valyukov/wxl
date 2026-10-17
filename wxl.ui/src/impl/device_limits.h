#pragma once

// What DrawingSurface.cpp knows about how much a device draws at once: the longest side of
// a texture at the device's feature level, less what composition keeps of it for itself.
// Private to the build of wxl and its tests -- the public header says only
// VirtualDrawingSurface::maxUpdateSide.

#include "platform.h"

#include <d3dcommon.h>

#include <cstdint>

namespace wxl::impl {

/// The longest side of a 2D texture at a Direct3D feature level, from the table of
/// "Direct3D feature levels" (Max Texture Dimension): 16384 from 11_0 up, 8192 at 10_0 and
/// 10_1, 4096 at 9_3, 2048 at 9_1 and 9_2.
constexpr std::int32_t max_texture_side(D3D_FEATURE_LEVEL level) noexcept {
    if (level >= D3D_FEATURE_LEVEL_11_0) return 16384;
    if (level >= D3D_FEATURE_LEVEL_10_0) return 8192;
    if (level >= D3D_FEATURE_LEVEL_9_3) return 4096;
    return 2048;
}

/// The pixels an allocation of a composition surface is padded with in its texture: two of
/// gutters and one "white pixel". So WinUI says of its own surfaces, taking them off the
/// texture side (DCompTreeHost::GetMaxTextureSize; CCoreServices::GetMaxTextureSize answers
/// 2048 - 3 when it cannot ask), and Microsoft's VirtualSurfaces sample, composition without
/// XAML, cuts every BeginDraw update to 2048 - 3 ("3 is the buffer").
constexpr std::int32_t composition_padding = 3;

/// The longest side of one update of a composition surface: an update is drawn into one
/// texture of the device (BeginDraw hands out one target at one offset), and composition
/// keeps composition_padding of its side.
constexpr std::int32_t max_update_side(D3D_FEATURE_LEVEL level) noexcept {
    return max_texture_side(level) - composition_padding;
}

}  // namespace wxl::impl
