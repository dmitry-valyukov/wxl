#pragma once

// The winrt-free face of two-way binding: what Bind{} calls, declared here and
// defined in binding.cpp where the projection is allowed.
//
// One pair per property a control writes itself, with the event that says so.
// This is the only table binding needs: a property the control does not write
// is bound one way, by the setter the generator already emits (bind_property
// in member.h), and has no entry here. The set is hand-written for now -- the
// controls a settings dialog reaches for -- and is where the generator will
// one day emit an entry per such property.
//
// Declared with wrapper types and observables and no winrt, so Bind.h (and the
// application that includes it) never sees the projection.

#include "core.h"

#include "member.h"

namespace wxl {
class ToggleSwitch;
class ComboBox;
class NumberBox;
class TextBox;
}  // namespace wxl

namespace wxl::impl {

/// ToggleSwitch.isOn <-> observable<bool>, under Toggled.
void apply_bind(ToggleSwitch const& control, core::observable<bool>& model);

/// ComboBox.selectedIndex <-> observable<int>, under SelectionChanged -- the
/// chosen row, by position.
void apply_bind(ComboBox const& control, core::observable<int>& model);

/// NumberBox.value <-> observable<int>, under ValueChanged -- the number,
/// rounded to whole.
void apply_bind(NumberBox const& control, core::observable<int>& model);

/// TextBox.text <-> observable<u16_text>, under TextChanged -- validated
/// UTF-16, repaired on the way in.
void apply_bind(TextBox const& control, core::observable<core::u16_text>& model);

// The same pairs by property, for the named form `isOn = Bind{...}`. A
// specialisation is what tells bind_property that this property is the
// control's to write, and so bound both ways; every other (property, control)
// meets the empty primary in member.h and is bound one way.

template <>
struct TwoWayBinder<PropertyKey::IsOn, ToggleSwitch> {
    static void bind(ToggleSwitch const& control, core::observable<bool>& model) {
        apply_bind(control, model);
    }
};

template <>
struct TwoWayBinder<PropertyKey::SelectedIndex, ComboBox> {
    static void bind(ComboBox const& control, core::observable<int>& model) {
        apply_bind(control, model);
    }
};

template <>
struct TwoWayBinder<PropertyKey::Value, NumberBox> {
    static void bind(NumberBox const& control, core::observable<int>& model) {
        apply_bind(control, model);
    }
};

template <>
struct TwoWayBinder<PropertyKey::Text, TextBox> {
    static void bind(TextBox const& control, core::observable<core::u16_text>& model) {
        apply_bind(control, model);
    }
};

}  // namespace wxl::impl
