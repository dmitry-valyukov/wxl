// The binding pairs, the winrt side: a control property <-> an observable
// field, both ways or the one asked for, wired so that nothing holds the model
// and nothing outlives what it points at.
//
// The projection and the Windows headers come first, and with them the standard
// library they pull in: the wxl headers below carry the wxl.core import, and a
// standard header after that import is one MSVC has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "Object.impl.h"
#include "events.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Controls.EventArgs.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"
#include "impl/binding.h"

namespace wxl::impl {
namespace {

// The control-side half of a binding pair, and the reason its handler may
// hold the field by bare address. The guard lives inside the field's own watch
// (bind_pair below), so it goes when the watch does -- with the field, or
// through unbind() -- and takes the handler off the control first. Nothing is
// ever left on a control that could write to a field that is gone.
//
// It owns the control, as the watch it lives in has to anyway: a control is a
// temporary in the description tree, and the watch writes to it.
template <EventKey key, class Control>
struct handler_guard {
    Control control;
    EventToken token;

    handler_guard(Control const& c, EventToken t) : control(c), token(t) {}

    // Moved into the watch's closure once; the source is left with nothing to
    // remove, so that only one of the two ever does.
    handler_guard(handler_guard&& other) noexcept
        : control(std::move(other.control)), token(std::exchange(other.token, EventToken{})) {}

    handler_guard(handler_guard const&) = delete;
    handler_guard& operator=(handler_guard const&) = delete;
    handler_guard& operator=(handler_guard&&) = delete;

    ~handler_guard() {
        if (token) EventAdder<key>::remove(control, token);
    }
};

// The shape shared by every binding pair, whatever the control and the value:
// the event that says the control wrote the property, and two little
// operations for how the property is read and written.
//
//  - `set` writes the field's value into the control. It runs once at the start
//    so the control opens showing the value, and again on every change.
//  - `get` reads the value back out for the control -> field direction.
//
// Asked for one direction, the pair keeps that half alone: input adds the
// handler and writes the control never, output writes the control and hears
// it never. The watch stays either way, with nothing to write for input, since
// it is what holds the guard.
//
// The control -> field handler names the control as its sender and the field
// by address, so it holds neither: the sender is read rather than captured,
// and the address is good for as long as the handler exists, which the guard
// sees to. The field -> control watch owns the control, through the guard. So
// the ownership runs one way -- model -> watch -> control -- and there is no
// cycle to cut: the model goes, its watches go, the handler comes off, the
// control is let go. Only a model that outlives its window needs unbind(),
// and then the guard does the same on the way out.
//
// No re-entry guard: writing the control fires its change event, which writes
// the same value back, and observable::set says nothing when nothing changed.
template <EventKey key, class Control, class T, class Get, class Set>
void bind_pair(Control const& control, core::observable<T>& model, bind_direction direction,
               Get get, Set set) {
    using Args = typename EventAdder<key>::template args_t<Control>;

    bool const shows = direction != bind_direction::input;
    bool const edits = direction != bind_direction::output;

    if (shows) set(control, model.get());

    EventToken token;
    if (edits) {
        token = EventAdder<key>::add(
            control, [&model, get](Control const& sender, Args&) { model.set(get(sender)); });
    }

    handler_guard<key, Control> guard{control, token};
    if (shows) {
        model.watch_for_binding([guard = std::move(guard), set](T const& value) noexcept {
            set(guard.control, value);
        });
    } else {
        model.watch_for_binding([guard = std::move(guard)](T const&) noexcept {});
    }
}

}  // namespace

void apply_bind(ToggleSwitch const& control, core::observable<bool>& model,
                bind_direction direction) {
    bind_pair<EventKey::Toggled>(
        control, model, direction,                          //
        [](ToggleSwitch const& c) { return c.isOn(); },     // get
        [](ToggleSwitch const& c, bool v) { c.isOn(v); });  // set
}

void apply_bind(ComboBox const& control, core::observable<int>& model, bind_direction direction) {
    bind_pair<EventKey::SelectionChanged>(
        control, model, direction,                                  //
        [](ComboBox const& c) { return c.selectedIndex(); },        // get
        [](ComboBox const& c, int v) { c.selectedIndex(v); });      // set
}

void apply_bind(NumberBox const& control, core::observable<int>& model, bind_direction direction) {
    bind_pair<EventKey::ValueChanged>(
        control, model, direction,                                       //
        [](NumberBox const& c) { return static_cast<int>(c.value()); },  // get
        [](NumberBox const& c, int v) { c.value(v); });                  // set
}

// NaN is the box's own word for empty, and it travels as it is: the box
// raises no ValueChanged for NaN over NaN, so the echo of writing one back
// ends there, where observable::set cannot end it (NaN equals nothing).
void apply_bind(NumberBox const& control, core::observable<double>& model,
                bind_direction direction) {
    bind_pair<EventKey::ValueChanged>(
        control, model, direction,                          //
        [](NumberBox const& c) { return c.value(); },       // get
        [](NumberBox const& c, double v) { c.value(v); });  // set
}

void apply_bind(TextBox const& control, core::observable<core::u16_text>& model,
                bind_direction direction) {
    bind_pair<EventKey::TextChanged>(
        control, model, direction,
        // get: the control hands back char16_t units, and nothing on the way
        // guaranteed them -- a paste is whatever the clipboard held. repaired
        // makes them the validated u16_text the model holds, an odd surrogate
        // becoming U+FFFD rather than a broken value. Written back, the
        // repaired text shows in the box; the box's own change event then
        // reads the same text, and the model, seeing no change, ends the
        // echo there.
        [](TextBox const& c) { return core::unicode::repaired(c.text()); },
        // set: the checked text goes to the control as it is -- string_param
        // takes u16_text, and no unit is looked at on the way.
        [](TextBox const& c, core::u16_text const& v) { c.text(v); });
}

}  // namespace wxl::impl
