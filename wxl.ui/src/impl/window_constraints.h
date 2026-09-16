#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../generated/Windows.Graphics.Structs.h"

// What stands behind the window properties WinUI3 has no property for.
//
// A minimum size is kept on the window's *presenter*, not on the window:
// Microsoft.UI.Xaml.Window exposes none of it, AppWindow hands out an
// AppWindowPresenter, and only the overlapped one -- the configuration an
// ordinary window has -- carries the four preferred bounds. That is three
// hops for something a declarative UI says in one word, so wxl declares the
// property itself (see the profile's syntheticMembers) and this is the body
// behind it.
//
// Private: the window arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

// The smallest the user may make the window, in the raw pixels AppWindow
// itself works in. A zero side is left unbounded, which is what the runtime
// reads an unset preference as.
void set_minimum_size(winrt::Microsoft::UI::Xaml::Window const& window, SizeInt32 const& size);

}  // namespace wxl::impl
