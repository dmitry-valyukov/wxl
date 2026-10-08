#pragma once

// wxl::ItemContainerPreset -- what an ItemContainerStyle says, as a preset worn by every container of a list.
//
//     GridView {
//         itemContainerStyle = Preset {margin = Thickness {0, 0, 12, 12}, horizontalContentAlignment = HorizontalAlignment::Stretch},
//         ...
//     }
//
// A ListView or GridView makes a container (ListViewItem, GridViewItem) for each item that comes into view, and a
// Style of XAML is how the application dresses those containers it never wrote. The style of wxl is a preset, and
// this one is worn by each container once, as the control makes it -- with the values, bindings and handlers the
// preset holds, the way the braces of a control written by hand would. A container the control reuses for another
// item keeps what it was dressed with.
//
// The preset is worn by the container as a SelectorItem: what a container has in common with every other -- the
// margin, the alignment of its content, the padding, the brushes.

#include "core.h"
#include "impl/member.h"

namespace wxl {

class SelectorItem;

class ItemContainerPreset {
public:
    using dress_t = core::function<void(SelectorItem const&)>;

    template <typename... Setters>
    ItemContainerPreset(Preset<Setters...> preset) : dress_{[preset](SelectorItem const& container) { preset(container); }} {}

    void operator()(SelectorItem const& container) const { dress_(container); }

    // The function itself, for whoever keeps it where a preset may be absent (core::nullable<dress_t>).
    dress_t const& dress() const noexcept { return dress_; }

private:
    dress_t dress_;
};

}  // namespace wxl
