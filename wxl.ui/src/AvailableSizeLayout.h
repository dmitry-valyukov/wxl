#pragma once

// wxl::AvailableSizeLayout -- a layout that tells the model how much room its panel was given.
//
//     struct Model {
//         core::observable<double> pageWidth;
//     };
//
//     LayoutPanel {
//         layout = AvailableSizeLayout {availableWidth = BindInput {model->pageWidth}},
//         page,
//     }
//
//     Grid { margin = BindOutput {model->pageWidth, [](double width) { return width >= 641 ? wide : narrow; }} }
//
// What XAML does with AdaptiveTrigger on the width of the window, wxl does with the width the container was given: a
// page beside a navigation pane has less room than its window, and it is its own room it must fit. The layout writes
// the available size into the fields at the start of every measure pass, before any child is measured, so whatever
// follows the fields -- a property, an attached property, a child replaced by another -- is in place within that same
// pass: one measure for a change of size, and nothing to undo.
//
//   availableWidth    the width the panel was offered by its parent; BindInput only
//   availableHeight   the height it was offered; BindInput only
//
// The children are laid out as in a Grid of one cell: each is measured with the whole of the available size and
// arranged over the whole of the final one.
//
// A threshold is a field that follows the width -- `wide.follow(pageWidth, [](double w) { return w >= 641; })` -- and
// since a field is silent while its value stays the same, what is bound to it runs when the threshold is crossed, not
// on every pixel of a border being dragged.
//
// An available size can be infinite: inside a horizontal StackPanel, or a ScrollViewer that scrolls that way, a panel
// is offered all the width there is. The field then reads infinity -- wider than any threshold -- and does not follow
// the window, because nothing constrains the panel.
//
// The object of WinUI is wxl's own: a NonVirtualizingLayout by composition (the cppwinrt template
// NonVirtualizingLayoutT). In the element tree it is wxl.AvailableSizeLayout.

#include <wxl/Members.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>

namespace wxl {

class AvailableSizeLayout : public Layout {
    using base_t = Layout;

public:
    class Impl;

    AvailableSizeLayout();

    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<AvailableSizeLayout, Setters...>
    explicit AvailableSizeLayout(Setters&&... setters) : AvailableSizeLayout() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

protected:
    explicit AvailableSizeLayout(Impl* impl) noexcept;

    friend class Object::Impl;

private:
    // The pairs behind availableWidth and availableHeight: the field is written from the measure pass until it goes
    // or its bindings are cut.
    static void bind_width(AvailableSizeLayout const& layout, core::observable<double>& field);
    static void bind_height(AvailableSizeLayout const& layout, core::observable<double>& field);

    friend struct impl::PropertyBinder<PropertyKey::AvailableWidth, AvailableSizeLayout>;
    friend struct impl::PropertyBinder<PropertyKey::AvailableHeight, AvailableSizeLayout>;
};

namespace impl {

// The layout reports the two and never takes them: input alone.
template <>
struct PropertyBinder<PropertyKey::AvailableWidth, AvailableSizeLayout> {
    static constexpr bind_direction direction = bind_direction::input;
    static void bind(AvailableSizeLayout const& layout, core::observable<double>& field, bind_direction) {
        AvailableSizeLayout::bind_width(layout, field);
    }
};

template <>
struct PropertyBinder<PropertyKey::AvailableHeight, AvailableSizeLayout> {
    static constexpr bind_direction direction = bind_direction::input;
    static void bind(AvailableSizeLayout const& layout, core::observable<double>& field, bind_direction) {
        AvailableSizeLayout::bind_height(layout, field);
    }
};

}  // namespace impl

}  // namespace wxl
