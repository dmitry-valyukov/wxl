#pragma once

// wxl::Bind, BindInput, BindOutput -- a binding written where the control is
// described.
//
//     struct Calc : core::sta_refcounted {
//         core::observable<std::u16string> entry{u"0"};
//     };
//
//     ToggleSwitch { isOn = Bind{settings.minimizeOnClose} }   // both ways: the control edits
//     ToggleSwitch { Bind{settings.minimizeOnClose} }          // the same, by the data's type
//     CheckBox { isChecked = BindInput{model->done} }          // a checked box writes the field
//     CheckBox { isChecked = BindInput{model->enabled, std::logical_not{}} }  // "disable" over "enabled"
//     Button { isEnabled = BindOutput{model->done, std::logical_not{}} } // shows fn of the field
//     TextBlock { text = BindOutput{calc->entry} }        // from the field: the control shows
//     TextBox { text = BindInput{search.query} }          // into the field: the control writes
//     NumberBox { intermediateValue = BindInput{eq.a} }   // into the field, as typed
//
// Three forms, one shape each, and a property takes the form its shape allows.
//
// Bind runs both ways: the control opens showing the value, follows every
// change, and writes back what is edited on it, by nobody's hand. It takes a
// property the control writes itself -- isOn under Toggled, text under
// TextChanged -- which is a pair in impl/binding.h. A property the control
// only shows has no way back and refuses it, naming BindOutput instead: a
// binding that silently ran one way would look like the other.
//
// BindOutput runs from the field to the control only: the control opens
// showing the value and follows every change, and what is typed into it stays
// there -- a TextBox as a display that can still be selected and copied from.
// It takes every property with a setter, pair or no pair, through the setter
// the generator already emits, and nothing has to be written for it.
//
// BindInput runs from the control to the field only: the field takes each
// value the control writes, and nothing is ever written back -- a query box
// whose field is the search, not the text. It takes a pair, since only a
// property the control writes has an event to read it under; a pair may take
// it and nothing else -- NumberBox's intermediateValue, the number as it is
// being typed, is read off the control and has no setter to show it with.
// The field takes the control's value as the binding is made, and every value
// after; the control is left as the description made it.
//
// Unnamed, `Bind{field}` inside the braces rides the route every unnamed
// argument does -- a callable applied to the object -- and binds the control's
// canonical property by the data's type (observable<bool> on a ToggleSwitch ->
// isOn). Only the pairs have that route, and BindInput and BindOutput take it
// the same way; a binding of any other property names it, because a control
// has several of one type (isEnabled and visibility are both bool) and the
// type alone cannot choose.
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
template <class T, bind_direction way, class Fn = identity_fn>
struct bound_field {
    static constexpr bind_direction direction = way;

    core::observable<T>* model;
    [[no_unique_address]] Fn fn;

    explicit bound_field(core::observable<T>& field) noexcept
        requires std::is_same_v<Fn, identity_fn>
        : model(&field) {}

    bound_field(core::observable<T>& field, Fn function) : model(&field), fn(std::move(function)) {}

    template <class Control>
    void operator()(Control const& control) const {
        if constexpr (std::is_same_v<Fn, identity_fn>) {
            apply_bind(control, *model, direction);
        } else if constexpr (direction == bind_direction::both) {
            bind_mirrored(*model, fn, [&](core::observable<T>& mirror) {
                apply_bind(control, mirror, direction);
            });
        } else if constexpr (direction == bind_direction::input) {
            bind_transformed_input(*model, fn, [&](core::observable<T>& own) {
                apply_bind(control, own, direction);
            });
        } else {
            static_assert(bind_always_false<Fn>,
                          "wxl: a function on BindOutput{} is written with the property named: "
                          "`isEnabled = BindOutput{field, fn}`.");
        }
    }
};

}  // namespace impl

// `Bind{field, fn}` runs both ways through a function that is its own inverse --
// `std::logical_not{}` for a box that says "disable" over a field that says
// "enabled" -- so the field is one and its opposite is never a second field.
template <class T, class Fn>
struct Bind : impl::bound_field<T, impl::bind_direction::both, Fn> {
    using impl::bound_field<T, impl::bind_direction::both, Fn>::bound_field;
};

template <class T>
Bind(core::observable<T>&) -> Bind<T>;

template <class T, class Fn>
Bind(core::observable<T>&, Fn) -> Bind<T, Fn>;

// `BindInput{field, fn}` writes fn of what the control says into the field.
template <class T, class Fn>
struct BindInput : impl::bound_field<T, impl::bind_direction::input, Fn> {
    using impl::bound_field<T, impl::bind_direction::input, Fn>::bound_field;
};

template <class T>
BindInput(core::observable<T>&) -> BindInput<T>;

template <class T, class Fn>
BindInput(core::observable<T>&, Fn) -> BindInput<T, Fn>;

// `BindOutput{field, fn}` shows fn of the field: any function, any result the
// property takes.
template <class T, class Fn>
struct BindOutput : impl::bound_field<T, impl::bind_direction::output, Fn> {
    using impl::bound_field<T, impl::bind_direction::output, Fn>::bound_field;
};

template <class T>
BindOutput(core::observable<T>&) -> BindOutput<T>;

template <class T, class Fn>
BindOutput(core::observable<T>&, Fn) -> BindOutput<T, Fn>;

}  // namespace wxl
