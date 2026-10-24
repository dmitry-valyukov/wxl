#pragma once

// What the effects share: the scene of a CompositionWindow behind an element, the layers an effect
// hangs among the children of an element's visual, and an attachment kept on its element while
// the element is in the tree.
//
// Included by the effects' .cpp files only, after the projection.

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>

#include <functional>

namespace wxl::impl {

// The scene of a CompositionWindow, reached from an element: the compositor the window's own
// pixels are made on, and the visual its backdrop -- the picture or the colour -- is painted on.
// Children of that visual lie over the backdrop and under the XAML island, which is where an
// effect goes that has to show what the window shows behind an element.
struct scene {
    winrt::Microsoft::UI::Composition::Compositor compositor{nullptr};
    winrt::Microsoft::UI::Composition::ContainerVisual root{nullptr};

    explicit operator bool() const noexcept { return root != nullptr; }
};

// The scene of the window with that id, or an empty one for any window that is not a
// CompositionWindow. Defined in CompositionWindow.cpp, beside the window's state.
scene scene_of(winrt::Microsoft::UI::WindowId window) noexcept;

// A layer an effect hangs among the children of an element's visual, placed by z. XAML draws the
// element into a child of its own, made lazily on the first frames, and that child is zero: a
// layer with a lower z lies under the element's own pixels, a higher one over them, and two layers
// of the same z keep the order they were written in. The z is kept in the layer's property set,
// so the next layer can read where the earlier ones stand.
void insert_layer(winrt::Microsoft::UI::Composition::VisualCollection const& children,
                  winrt::Microsoft::UI::Composition::Visual const& layer, int z);

// Runs `place` once the element's visual has the child XAML draws it into: at once if it is there,
// otherwise on the frame it appears. Some elements (a Border) clear the children when they make
// that child, and a layer put there earlier would go with them.
void when_drawn(
    winrt::Microsoft::UI::Composition::ContainerVisual const& host,
    std::function<void(winrt::Microsoft::UI::Composition::VisualCollection const&)> place);

// The rounding an element gives itself -- a Border, a control, a Grid, a StackPanel or a
// RelativePanel -- so that a layer over it can round the same. Zero for an element that has none.
float corner_of(winrt::Microsoft::UI::Xaml::UIElement const& element);

// Keeps what an effect attaches to an element on it while the element is in the tree: placed on
// Loaded, taken off on Unloaded, and placed at once if the element is loaded already. The
// element's own events hold the handlers, and the handlers hold `attachment` -- a counted pointer
// to it -- which is how the attachment lives as long as the element does. Its remove() takes the
// element when it needs it, to take a handler of its own off.
template <class Attachment>
void attach_while_loaded(winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
                         Attachment const& attachment) {
    using element_t = winrt::Microsoft::UI::Xaml::FrameworkElement;
    element.Loaded([attachment](auto const& sender, auto&&) {
        attachment->place(sender.template as<element_t>());
    });
    element.Unloaded([attachment]([[maybe_unused]] auto const& sender, auto&&) {
        if constexpr (requires(element_t const& fe) { attachment->remove(fe); }) {
            attachment->remove(sender.template as<element_t>());
        } else {
            attachment->remove();
        }
    });
    if (element.IsLoaded()) attachment->place(element);
}

}  // namespace wxl::impl
