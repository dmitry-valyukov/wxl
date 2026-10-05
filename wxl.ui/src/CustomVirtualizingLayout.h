#pragma once

// wxl::CustomVirtualizingLayout -- a layout of an ItemsRepeater written as two functions.
//
//     auto layout = CustomVirtualizingLayout {
//         [](VirtualizingLayoutContext const& context, Size available) { ...; return Size {...}; },
//         [](VirtualizingLayoutContext const& context, Size final)     { ...; return Size {...}; },
//     };
//     ItemsRepeater {layout = layout, ...};
//
// WinUI lets an application derive from VirtualizingLayout and override
// MeasureOverride and ArrangeOverride; that is a class of the application's own
// with a WinRT base, which a declarative UI has no place to write. This is the
// object of that class with the two overrides handed in -- the same contract:
// the first function measures the elements the layout wants on screen (it asks
// the context for them with getOrCreateElementAt, within the realizationRect)
// and returns the size of all the content, the second arranges them and
// returns the size it used.
//
// Whatever the layout keeps from one pass to the next -- the rectangles it
// worked out, the index of the first realized item -- lives in what the
// functions capture; WinUI's own LayoutState of the context is not needed for
// that. After a property the layout reads changes, invalidate() asks the panel
// for a new pass.
//
// The object of WinUI is wxl's own: a VirtualizingLayout by composition (the
// cppwinrt template VirtualizingLayoutT). In the element tree it is
// wxl.CustomVirtualizingLayout.

#include <functional>

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include "geometry.h"

namespace wxl {

class CustomVirtualizingLayout : public Layout {
    using base_t = Layout;

public:
    class Impl;

    using Measure = std::function<Size(VirtualizingLayoutContext const& context, Size available)>;
    using Arrange = std::function<Size(VirtualizingLayoutContext const& context, Size final)>;
    /// The items the layout lays out were added, removed or replaced: whatever it kept about the old
    /// ones (rectangles, the first realized index) is not valid any more.
    using ItemsChanged = std::function<void(VirtualizingLayoutContext const& context)>;

    CustomVirtualizingLayout(Measure measure, Arrange arrange);
    CustomVirtualizingLayout(Measure measure, Arrange arrange, ItemsChanged itemsChanged);

    /// A new measure pass over the panel that holds the layout.
    void invalidate() const;

protected:
    explicit CustomVirtualizingLayout(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
