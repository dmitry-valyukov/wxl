#pragma once

// wxl::CustomLayout -- a layout written as two functions.
//
//     auto layout = CustomLayout {
//         [](Collection<UIElement> const& children, Size available) { ...; return Size {...}; },
//         [](Collection<UIElement> const& children, Size final)     { ...; return Size {...}; },
//     };
//     LayoutPanel {layout = layout, ...children};
//
// WinUI lets an application derive from NonVirtualizingLayout and override
// MeasureOverride and ArrangeOverride; that is a class of the application's
// own with a WinRT base, which a declarative UI has no place to write. This is
// the object of that class with the two overrides handed in: the first
// function measures the children (UIElement::measure) and returns the size it
// wants, the second arranges them (UIElement::arrange) and returns the size it
// used -- the same contract as the two overrides, with the children as a
// collection instead of a context.
//
// Whatever the layout reads besides the children -- a spacing, a column count
// -- lives in what the functions capture. After it changes, invalidate()
// asks the panel for a new pass.
//
// The object of WinUI is wxl's own: a NonVirtualizingLayout by composition
// (the cppwinrt template NonVirtualizingLayoutT). In the element tree it is
// wxl.CustomLayout.

#include <functional>

#include "Collection.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include "geometry.h"

namespace wxl {

class CustomLayout : public Layout {
    using base_t = Layout;

public:
    class Impl;

    using Measure = std::function<Size(Collection<UIElement> const& children, Size available)>;
    using Arrange = std::function<Size(Collection<UIElement> const& children, Size final)>;

    CustomLayout(Measure measure, Arrange arrange);

    /// A new measure pass over the panel that holds the layout.
    void invalidate() const;

protected:
    explicit CustomLayout(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
