#pragma once

// wxl::Bind -- a binding written where the control is described.
//
//     struct Calc : core::sta_refcounted {
//         core::observable<std::u16string> entry{u"0"};
//     };
//
//     TextBlock { text = Bind{calc->entry} }              // one way: the field shows
//     ToggleSwitch { isOn = Bind{settings.minimizeOnClose} }   // two ways: the control edits
//     ToggleSwitch { Bind{settings.minimizeOnClose} }          // the same, by the data's type
//
// Named, `property = Bind{field}` binds that property to the field: the control
// opens showing the value and follows every change, by nobody's hand. Which way
// the binding runs is the property's own nature, not a mode to choose. A
// property the control writes itself -- isOn under Toggled, text under
// TextChanged -- has a two-way pair in impl/binding.h and is bound both ways,
// the way WPF defaults TextBox.Text to two-way and TextBlock.Text to one; a
// one-way binding into a control that edits would lose the edit to the next
// write. Every other settable property is bound one way, through the setter
// the generator already emits, and needs no pair written for it.
//
// Unnamed, `Bind{field}` inside the braces rides the route every unnamed
// argument does -- a callable applied to the object -- and binds the control's
// canonical property by the data's type (observable<bool> on a ToggleSwitch ->
// isOn). Only the two-way pairs have that route; a one-way binding names its
// property, because a control has several of one type (isEnabled and
// visibility are both bool) and the type alone cannot choose.
//
// Bind holds the field by address and never owns it. The field is a member of a
// model that outlives the description, and the binding is a watch inside that
// field: it owns the control, the control holds nothing back, and both go when
// the field goes.

#include "core.h"

#include "impl/binding.h"

namespace wxl {

template <class T>
struct Bind {
    core::observable<T>* model;

    explicit Bind(core::observable<T>& field) noexcept : model(&field) {}

    template <class Control>
    void operator()(Control const& control) const {
        impl::apply_bind(control, *model);
    }
};

template <class T>
Bind(core::observable<T>&) -> Bind<T>;

}  // namespace wxl
