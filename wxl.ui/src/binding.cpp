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
#include <winrt/Windows.Globalization.NumberFormatting.h>

#include "Object.impl.h"
#include "events.h"
#include <wxl/Members.h>
#include <wxl/Microsoft.UI.Xaml.Controls.EventArgs.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
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

    if (shows) {
        set(control, model.get());
    } else if (edits) {
        // Input alone: the field takes what the control has right now, so the
        // two agree from the first moment and the first change is a change.
        model.set(get(control));
    }

    EventToken token;
    if (edits) {
        token = EventAdder<key>::add(
            control, [&model, get](Control const& sender, EventArgsRef<Args>) { model.set(get(sender)); });
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

// A checked box says so under two events, Checked and Unchecked, so it has two
// guards, and the field's watch keeps both: they come off together with the
// watch. Indeterminate (a third state of a three-state box) is neither: a bool
// field reads it as not checked, as isChecked().value_or(false) does.
void apply_bind(ToggleButton const& control, core::observable<bool>& model,
                bind_direction direction) {
    using Args = typename EventAdder<EventKey::Checked>::template args_t<ToggleButton>;

    bool const shows = direction != bind_direction::input;
    bool const edits = direction != bind_direction::output;

    auto const set = [](ToggleButton const& c, bool v) { c.isChecked(v); };
    auto const get = [](ToggleButton const& c) { return c.isChecked().value_or(false); };
    if (shows) {
        set(control, model.get());
    } else if (edits) {
        model.set(get(control));
    }

    EventToken checked;
    EventToken unchecked;
    if (edits) {
        auto const write = [&model, get](ToggleButton const& sender, EventArgsRef<Args>) { model.set(get(sender)); };
        checked = EventAdder<EventKey::Checked>::add(control, write);
        unchecked = EventAdder<EventKey::Unchecked>::add(control, write);
    }

    handler_guard<EventKey::Checked, ToggleButton> onChecked{control, checked};
    handler_guard<EventKey::Unchecked, ToggleButton> onUnchecked{control, unchecked};
    if (shows) {
        model.watch_for_binding([a = std::move(onChecked), b = std::move(onUnchecked),
                                 set](bool value) noexcept { set(a.control, value); });
    } else {
        model.watch_for_binding([a = std::move(onChecked), b = std::move(onUnchecked)](bool) noexcept {});
    }
}

void apply_bind(ComboBox const& control, core::observable<int>& model, bind_direction direction) {
    bind_pair<EventKey::SelectionChanged>(
        control, model, direction,                                  //
        [](ComboBox const& c) { return c.selectedIndex(); },        // get
        [](ComboBox const& c, int v) { c.selectedIndex(v); });      // set
}

void apply_bind(RadioButtons const& control, core::observable<int>& model,
                bind_direction direction) {
    bind_pair<EventKey::SelectionChanged>(
        control, model, direction,                                     //
        [](RadioButtons const& c) { return c.selectedIndex(); },       // get
        [](RadioButtons const& c, int v) { c.selectedIndex(v); });     // set
}

void apply_bind(Slider const& control, core::observable<double>& model, bind_direction direction) {
    bind_pair<EventKey::ValueChanged>(
        control, model, direction,                          //
        [](Slider const& c) { return c.value(); },          // get
        [](Slider const& c, double v) { c.value(v); });     // set
}

void apply_bind(RatingControl const& control, core::observable<double>& model,
                bind_direction direction) {
    bind_pair<EventKey::ValueChanged>(
        control, model, direction,                                 //
        [](RatingControl const& c) { return c.value(); },          // get
        [](RatingControl const& c, double v) { c.value(v); });     // set
}

void apply_bind(ColorPicker const& control, core::observable<Color>& model,
                bind_direction direction) {
    bind_pair<EventKey::ColorChanged>(
        control, model, direction,                                 //
        [](ColorPicker const& c) { return c.color(); },            // get
        [](ColorPicker const& c, Color v) { c.color(v); });        // set
}

void apply_bind(ToggleMenuFlyoutItem const& control, core::observable<bool>& model,
                bind_direction direction) {
    bind_pair<EventKey::Click>(
        control, model, direction,                                    //
        [](ToggleMenuFlyoutItem const& c) { return c.isChecked(); },  // get
        [](ToggleMenuFlyoutItem const& c, bool v) { c.isChecked(v); });  // set
}

void apply_bind(RadioMenuFlyoutItem const& control, core::observable<bool>& model,
                bind_direction direction) {
    bind_pair<EventKey::Click>(
        control, model, direction,                                    //
        [](RadioMenuFlyoutItem const& c) { return c.isChecked(); },   // get
        [](RadioMenuFlyoutItem const& c, bool v) { c.isChecked(v); });   // set
}

void apply_bind(FlipView const& control, core::observable<int>& model, bind_direction direction) {
    bind_pair<EventKey::SelectionChanged>(
        control, model, direction,                                 //
        [](FlipView const& c) { return c.selectedIndex(); },       // get
        [](FlipView const& c, int v) { c.selectedIndex(v); });     // set
}

void apply_bind(PipsPager const& control, core::observable<int>& model, bind_direction direction) {
    bind_pair<EventKey::SelectedIndexChanged>(
        control, model, direction,                                     //
        [](PipsPager const& c) { return c.selectedPageIndex(); },      // get
        [](PipsPager const& c, int v) { c.selectedPageIndex(v); });    // set
}

void apply_bind(PagerControl const& control, core::observable<int>& model,
                bind_direction direction) {
    bind_pair<EventKey::SelectedIndexChanged>(
        control, model, direction,                                        //
        [](PagerControl const& c) { return c.selectedPageIndex(); },      // get
        [](PagerControl const& c, int v) { c.selectedPageIndex(v); });    // set
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
        // set: the checked text goes to the control as it is -- hstring_param
        // takes u16_text, and no unit is looked at on the way.
        [](TextBox const& c, core::u16_text const& v) { c.text(v); });
}

namespace {

// What the intermediate-value pair leaves on the box: the Loaded handler that
// finds the inner text box, and the TextChanged handler it puts there. Held
// by the field's watch alone, so both come off when the field goes or
// unbind() runs -- while the handlers still name the field by bare address.
struct intermediate_guard : core::sta_refcounted {
    NumberBox box;
    EventToken loaded;
    winrt::Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
    winrt::event_token changed;

    explicit intermediate_guard(NumberBox const& b) : box(b) {}

    ~intermediate_guard() {
        if (input) input.TextChanged(changed);
        if (loaded) EventAdder<EventKey::Loaded>::remove(box, loaded);
    }
};

}  // namespace

// NumberBox commits to Value on Enter, a spin or the focus leaving, and says
// nothing in between; the number as typed is read off the TextBox inside its
// template instead, on every change of its text. The text is parsed by the
// box's own NumberFormatter -- the parser the box will use itself when it
// commits, so the field sees what Value is about to become, in the box's own
// locale. Text that is not a number yet ("-", "1e") parses to nothing, and
// nothing is NaN, the box's own word for an empty field.
//
// The template is applied once the box is in the tree, so the inner box is
// found on Loaded, and once: a box unloaded and loaded again keeps its
// template. The handler holds the parser, not the box -- a handler on the
// inner box holding the outer would be a cycle -- and the field by address,
// which the guard makes safe.
void apply_bind_intermediate_value(NumberBox const& control, core::observable<double>& model) {
    using Args = typename EventAdder<EventKey::Loaded>::template args_t<NumberBox>;
    using winrt::Windows::Globalization::NumberFormatting::INumberParser;
    namespace xaml = winrt::Microsoft::UI::Xaml;

    core::intrusive_ptr<intermediate_guard> guard{new intermediate_guard{control},
                                                  /*add_ref=*/false};
    intermediate_guard* const state = guard.get();

    state->loaded = EventAdder<EventKey::Loaded>::add(
        control, [&model, state](NumberBox const& sender, Args&) {
            if (state->input) return;

            // GetTemplateChild is protected, and the projection puts it on the
            // protected interface rather than on the class.
            xaml::Controls::NumberBox const& box = *Object::Impl::get_typed<NumberBox>(sender);
            auto const input = box.as<xaml::Controls::IControlProtected>()
                                   .GetTemplateChild(L"InputBox")
                                   .try_as<xaml::Controls::TextBox>();
            if (!input) return;
            INumberParser const parser = box.NumberFormatter().as<INumberParser>();

            state->input = input;
            state->changed = input.TextChanged(
                [&model, parser](winrt::Windows::Foundation::IInspectable const& sender,
                                 xaml::Controls::TextChangedEventArgs const&) {
                    auto const number = parser.ParseDouble(sender.as<xaml::Controls::TextBox>().Text());
                    model.set(number ? number.Value() : std::numeric_limits<double>::quiet_NaN());
                });
        });

    model.watch_for_binding([guard = std::move(guard)](double const&) noexcept {});
}

}  // namespace wxl::impl
