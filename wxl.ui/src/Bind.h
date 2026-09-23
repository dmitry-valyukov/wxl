#pragma once

// wxl::Bind, BindInput, BindOutput -- a binding written where the control is
// described.
//
//     struct Calc : core::sta_refcounted {
//         core::observable<std::u16string> entry{u"0"};
//     };
//
//     TextBlock { text = Bind{calc->entry} }              // one way: the field shows
//     ToggleSwitch { isOn = Bind{settings.minimizeOnClose} }   // two ways: the control edits
//     ToggleSwitch { Bind{settings.minimizeOnClose} }          // the same, by the data's type
//     TextBox { text = BindInput{search.query} }          // the control writes the field, and that is all
//     TextBox { text = BindOutput{search.hint} }          // the field writes the control, and that is all
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
// BindInput and BindOutput are the two halves of that, for when the property's
// nature is not what is wanted. BindOutput runs from the field to the control
// only: the control opens showing the value and follows every change, and what
// is typed into it stays there -- a TextBox as a display that can still be
// selected and copied from. It takes every settable property, pair or no pair.
// BindInput runs from the control to the field only: the field takes each
// value the control writes, and nothing is ever written back -- a query box
// whose field is the search, not the text. It needs the pair, since only a
// property the control writes has an event to read it under, and names one
// without a pair to a compile error. The control opens as the description
// left it, and the field keeps its value until the control's first change.
//
// Unnamed, `Bind{field}` inside the braces rides the route every unnamed
// argument does -- a callable applied to the object -- and binds the control's
// canonical property by the data's type (observable<bool> on a ToggleSwitch ->
// isOn). Only the two-way pairs have that route, and BindInput and BindOutput
// take it the same way; a one-way binding of any other property names it,
// because a control has several of one type (isEnabled and visibility are
// both bool) and the type alone cannot choose.
//
// A binding holds the field by address and never owns it. The field is a
// member of a model that outlives the description, and the binding is a watch
// inside that field: it owns the control, the control holds nothing back, and
// both go when the field goes.

#include "core.h"

#include "impl/binding.h"

namespace wxl {

namespace impl {

// What the three spellings share: the field by address, and the direction,
// applied to whatever control the braces are building.
template <class T, bind_direction way>
struct bound_field {
    static constexpr bind_direction direction = way;

    core::observable<T>* model;

    explicit bound_field(core::observable<T>& field) noexcept : model(&field) {}

    template <class Control>
    void operator()(Control const& control) const {
        apply_bind(control, *model, direction);
    }
};

}  // namespace impl

template <class T>
struct Bind : impl::bound_field<T, impl::bind_direction::both> {
    using impl::bound_field<T, impl::bind_direction::both>::bound_field;
};

template <class T>
Bind(core::observable<T>&) -> Bind<T>;

template <class T>
struct BindInput : impl::bound_field<T, impl::bind_direction::input> {
    using impl::bound_field<T, impl::bind_direction::input>::bound_field;
};

template <class T>
BindInput(core::observable<T>&) -> BindInput<T>;

template <class T>
struct BindOutput : impl::bound_field<T, impl::bind_direction::output> {
    using impl::bound_field<T, impl::bind_direction::output>::bound_field;
};

template <class T>
BindOutput(core::observable<T>&) -> BindOutput<T>;

}  // namespace wxl
