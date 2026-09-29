#pragma once

// A layer an effect hangs among the children of an element's visual, placed
// by z. XAML draws the element into a child of its own, made lazily on the
// first frames, and that child is zero: a layer with a lower z lies under
// the element's own pixels, a higher one over them, and two layers of the
// same z keep the order they were written in. The z is kept in the layer's
// property set, so the next layer can read where the earlier ones stand.
//
// Included by the effects' .cpp files only, after the projection.

#include <winrt/Microsoft.UI.Composition.h>

#include <functional>

namespace wxl::impl {

void insert_layer(winrt::Microsoft::UI::Composition::VisualCollection const& children,
                  winrt::Microsoft::UI::Composition::Visual const& layer, int z);

// Runs `place` once the element's visual has the child XAML draws it into:
// at once if it is there, otherwise on the frame it appears. Some elements
// (a Border) clear the children when they make that child, and a layer put
// there earlier would go with them.
void when_drawn(winrt::Microsoft::UI::Composition::ContainerVisual const& host,
                std::function<void(winrt::Microsoft::UI::Composition::VisualCollection const&)> place);

}  // namespace wxl::impl
