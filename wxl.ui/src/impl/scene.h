#pragma once

// The scene of a CompositionWindow, reached from an element: the compositor
// the window's own pixels are made on, and the visual its backdrop -- the
// picture or the colour -- is painted on. Children of that visual lie over the
// backdrop and under the XAML island, which is where an effect goes that has
// to show what the window shows behind an element.
//
// Included by the effects' .cpp files only, after the projection.

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.h>

namespace wxl::impl {

struct scene {
    winrt::Microsoft::UI::Composition::Compositor compositor{nullptr};
    winrt::Microsoft::UI::Composition::ContainerVisual root{nullptr};

    explicit operator bool() const noexcept { return root != nullptr; }
};

// The scene of the window with that id, or an empty one for any window that
// is not a CompositionWindow.
scene scene_of(winrt::Microsoft::UI::WindowId window) noexcept;

}  // namespace wxl::impl
