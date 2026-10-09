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
// Two pairs bind state the control takes only on its own terms -- isFocused
// and a ScrollViewer's verticalOffset (impl/state_pair.h). They have no
// setter: the field asks, the control does what it can, and the field says
// what it did. So they take Bind or BindInput, never BindOutput, which would
// leave the field saying what was asked.
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
class ToggleMenuFlyoutItem;
class RadioMenuFlyoutItem;
class FlipView;
class PipsPager;
class PagerControl;
class UIElement;
class ScrollViewer;
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

/// ToggleMenuFlyoutItem.isChecked <-> observable<bool>, under Click -- the item
/// flips itself when it is chosen, and says nothing else.
void apply_bind(ToggleMenuFlyoutItem const& control, core::observable<bool>& model,
                bind_direction direction = bind_direction::both);

/// RadioMenuFlyoutItem.isChecked <-> observable<bool>, under Click. The group
/// unchecks the others without a click of theirs: a field per item that must
/// follow its group reads the group's choice from one field instead.
void apply_bind(RadioMenuFlyoutItem const& control, core::observable<bool>& model,
                bind_direction direction = bind_direction::both);

/// FlipView.selectedIndex <-> observable<int>, under SelectionChanged -- the page
/// shown, by position, -1 while none is.
void apply_bind(FlipView const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// PipsPager.selectedPageIndex <-> observable<int>, under SelectedIndexChanged.
void apply_bind(PipsPager const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// PagerControl.selectedPageIndex <-> observable<int>, under SelectedIndexChanged.
void apply_bind(PagerControl const& control, core::observable<int>& model,
                bind_direction direction = bind_direction::both);

/// NumberBox.intermediateValue -> observable<double>: the number as it is
/// being typed, parsed by the box's own NumberFormatter on every change of
/// its text, NaN while the text is not a number yet. Input alone -- the box
/// commits to Value on its own terms, and this is the value before that.
void apply_bind_intermediate_value(NumberBox const& control, core::observable<double>& model);

/// UIElement.isFocused <-> observable<bool>, under GotFocus and LostFocus: whether
/// the element itself has the focus. True asks for it -- an element out of the
/// tree is asked when it is loaded -- and the field then says whether the
/// element took it; false is no request, since XAML gives focus up only by
/// giving it to another element, so a focused element reads true again. A
/// control that hands its focus to a part of its template reads false.
void apply_bind_focus(UIElement const& control, core::observable<bool>& model,
                      bind_direction direction = bind_direction::both);

/// ScrollViewer.verticalOffset <-> observable<double>, under ViewChanged once the
/// view has settled. A value scrolls the viewer there without animation, cut to
/// the content's extent; a viewer out of the tree is scrolled when it is
/// loaded. The field says where the viewer is, or is going: the offset it was
/// cut to, or the one it stays at when the viewer refuses.
void apply_bind_vertical_offset(ScrollViewer const& control, core::observable<double>& model,
                                bind_direction direction = bind_direction::both);

// The same pairs by property, for the named form `isOn = Bind{...}`. A
// specialisation is what tells bind_property that this property is the
// control's to write, and so bound both ways -- or the one way asked for;
// every other (property, control) meets the empty primary in member.h and
// takes BindOutput alone.

template <>
struct PropertyBinder<PropertyKey::IsOn, ToggleSwitch> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ToggleSwitch const& control, core::observable<bool>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::IsChecked, ToggleButton> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ToggleButton const& control, core::observable<bool>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
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
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedIndex, ComboBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ComboBox const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, NumberBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(NumberBox const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
    static void bind(NumberBox const& control, core::observable<double>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedIndex, RadioButtons> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(RadioButtons const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, Slider> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(Slider const& control, core::observable<double>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::Value, RatingControl> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(RatingControl const& control, core::observable<double>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::Color, ColorPicker> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ColorPicker const& control, core::observable<Color>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::IsChecked, ToggleMenuFlyoutItem> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ToggleMenuFlyoutItem const& control, core::observable<bool>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::IsChecked, RadioMenuFlyoutItem> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(RadioMenuFlyoutItem const& control, core::observable<bool>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedIndex, FlipView> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(FlipView const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedPageIndex, PipsPager> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(PipsPager const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::SelectedPageIndex, PagerControl> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(PagerControl const& control, core::observable<int>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::IntermediateValue, NumberBox> {
    static constexpr bind_direction direction = bind_direction::input;
    static void bind(NumberBox const& control, core::observable<double>& model, bind_direction) {
        apply_bind_intermediate_value(control, model);
    }
};

// Every element has a focus, so the pair is one partial specialisation over
// whatever derives from UIElement rather than one per class. The constraint is
// checked where a binding is written, which has the control's whole class; the
// conversion to UIElement happens there too.
template <class Element>
    requires std::derived_from<Element, UIElement>
struct PropertyBinder<PropertyKey::IsFocused, Element> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(UIElement const& control, core::observable<bool>& model,
                     bind_direction asked) {
        apply_bind_focus(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::VerticalOffset, ScrollViewer> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(ScrollViewer const& control, core::observable<double>& model,
                     bind_direction asked) {
        apply_bind_vertical_offset(control, model, asked);
    }
};

template <>
struct PropertyBinder<PropertyKey::Text, TextBox> {
    static constexpr bind_direction direction = bind_direction::both;
    static void bind(TextBox const& control, core::observable<core::u16_text>& model,
                     bind_direction asked) {
        apply_bind(control, model, asked);
    }
};

}  // namespace wxl::impl
