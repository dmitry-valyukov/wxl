#pragma once

// The winrt-free face of the binding pairs: what Bind{}, BindInput{} and
// BindOutput{} call, declared here and defined in binding.cpp where the
// projection is allowed.
//
// One pair per property a control writes itself, with the event that says so.
// This is the only table binding needs: a property the control does not write
// is bound one way, from the field, by the setter the generator already emits
// (bind_property in member.h), and has no entry here. The set is hand-written
// for now -- the controls a settings dialog reaches for -- and is where the
// generator will one day emit an entry per such property.
//
// A pair runs both ways unless `direction` keeps one: input is the control ->
// field half alone, output the field -> control half alone. A pair whose own
// direction is input has that half only: the control reports the property and
// never shows it, so it takes BindInput and nothing else.
//
// Declared with wrapper types and observables and no winrt, so Bind.h (and the
// application that includes it) never sees the projection.

#include "core.h"

#include "Color.h"
#include "member.h"

namespace wxl {
class ToggleSwitch;
class ToggleButton;
class CheckBox;
class ComboBox;
class NumberBox;
class TextBox;
class RadioButtons;
class Slider;
class RatingControl;
class ColorPicker;
}  // namespace wxl

namespace wxl::impl {

/// ToggleSwitch.isOn <-> observable<bool>, under Toggled.
void apply_bind(ToggleSwitch const& control, core::observable<bool>& model,
                bind_direction direction = bind_direction::both);

/// ToggleButton.isChecked <-> observable<bool>, under Checked and Unchecked --
/// a CheckBox, a RadioButton or a ToggleButton itself. Indeterminate is not
/// a bool and is left alone: a field that has it is a nullable, not this.
void apply_bind(ToggleButton const& control, core::observable<bool>& model,
                bind_direction direction = bind_direction::both);

/// ComboBox.selectedIndex <-> observable<int>, under SelectionChanged -- the
/// chosen row, by position.
void apply_bind(ComboBox const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// NumberBox.value <-> observable<int>, under ValueChanged -- the number,
/// rounded to whole.
void apply_bind(NumberBox const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// NumberBox.value <-> observable<double>, under ValueChanged -- the number
/// as the box holds it, NaN while the box is empty.
void apply_bind(NumberBox const& control, core::observable<double>& model,
                bind_direction direction = bind_direction::both);

/// TextBox.text <-> observable<u16_text>, under TextChanged -- validated
/// UTF-16, repaired on the way in.
void apply_bind(TextBox const& control, core::observable<core::u16_text>& model,
                bind_direction direction = bind_direction::both);

/// RadioButtons.selectedIndex <-> observable<int>, under SelectionChanged --
/// the chosen button, by position, -1 while none is.
void apply_bind(RadioButtons const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// Slider.value <-> observable<double>, under ValueChanged.
void apply_bind(Slider const& control, core::observable<double>& model,
                bind_direction direction = bind_direction::both);

/// RatingControl.value <-> observable<double>, under ValueChanged -- -1 while
/// the control holds no rating.
void apply_bind(RatingControl const& control, core::observable<double>& model,
                bind_direction direction = bind_direction::both);

/// ColorPicker.color <-> observable<Color>, under ColorChanged.
void apply_bind(ColorPicker const& control, core::observable<Color>& model,
                bind_direction direction = bind_direction::both);

/// NumberBox.intermediateValue -> observable<double>: the number as it is
/// being typed, parsed by the box's own NumberFormatter on every change of
/// its text, NaN while the text is not a number yet. Input alone -- the box
/// commits to Value on its own terms, and this is the value before that.
void apply_bind_intermediate_value(NumberBox const& control, core::observable<double>& model);

// The same pairs by property, for the named form `isOn = Bind{...}`. A
// specialisation is what tells bind_property that this property is the
// control's to write, and so bound both ways -- or the one way asked for;
// every other (property, control) meets the empty primary in member.h and
// takes BindOutput alone.

template <>
struct PropertyBinder<PropertyKey::IsOn, ToggleSwitch> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ToggleSwitch const& control, core::observable<bool>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::IsChecked, ToggleButton> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ToggleButton const& control, core::observable<bool>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

// A template member, not a function: converting a CheckBox to its ToggleButton
// base needs the CheckBox complete, and this header only declares it. The body
// is compiled where the binding is written, which has the whole class.
template <>
struct PropertyBinder<PropertyKey::IsChecked, CheckBox> {
    static constexpr bind_direction direction = bind_direction::both;
    template <class Control>
    static void bind(Control const& control, core::observable<bool>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedIndex, ComboBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ComboBox const& control, core::observable<int>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, NumberBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(NumberBox const& control, core::observable<int>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
    static void bind(NumberBox const& control, core::observable<double>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedIndex, RadioButtons> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(RadioButtons const& control, core::observable<int>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, Slider> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(Slider const& control, core::observable<double>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, RatingControl> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(RatingControl const& control, core::observable<double>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::Color, ColorPicker> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ColorPicker const& control, core::observable<Color>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

template <>
struct PropertyBinder<PropertyKey::IntermediateValue, NumberBox> {
    static constexpr bind_direction direction = bind_direction::input;
    static void bind(NumberBox const& control, core::observable<double>& model, bind_direction) {
        apply_bind_intermediate_value(control, model);
    }
};

template <>
struct PropertyBinder<PropertyKey::Text, TextBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(TextBox const& control, core::observable<core::u16_text>& model,
                     bind_direction direction) {
        apply_bind(control, model, direction);
    }
};

}  // namespace wxl::impl
