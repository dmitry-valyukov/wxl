// impl::settle_focus and impl::settle_offset -- the rules of isFocused and
// verticalOffset, over an element made of plain fields.
//
// A plain executable rather than a gtest, as app_zoom_test is: state_pair.h
// reaches wxl.core through core.h (include, then import), and gtest's standard
// headers cannot share a translation unit with that import. The result is the
// exit code.
//
// The field is a real observable, watched the way binding.cpp watches it, so
// what is checked includes the field set again from inside its own watch: the
// application hears where the element ended, never a value it could not take.
//
// Checked: a request made out of the tree waits for Loaded; refused focus
// returns the field to false, and the next request is a change again; false on
// a focused element reads true; focus moving away is reported, not taken back;
// an offset past the extent, below zero or not a number is corrected in the
// field before the viewer is asked, and the viewer is asked once; a refused
// jump and an offset already reached leave the field at the offset now.

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include "impl/state_pair.h"

namespace {

using wxl::core::observable;

int failures = 0;

void check(bool ok, char const* what) {
    if (!ok) {
        std::fprintf(stderr, "state_pair_test: FAILED -- %s\n", what);
        ++failures;
    }
}

// An element whose focus is a flag. Focus() is taken only in the tree and only
// by a focusable element, as XAML takes it.
struct fake_element {
    bool in_tree = false;
    bool focusable = true;
    mutable bool has_focus = false;
    mutable int attempts = 0;

    bool loaded() const { return in_tree; }
    bool focused() const { return has_focus; }
    void focus() const {
        ++attempts;
        has_focus = in_tree && focusable;
    }
};

// A viewer whose jump lands later, as ChangeView's does: scroll_to takes the
// offset asked, and arrive() is the view coming to rest and ViewChanged with it.
struct fake_viewer {
    bool in_tree = true;
    bool refuses = false;
    double scrollable = 500.0;
    mutable double at = 0.0;
    mutable double going = 0.0;
    mutable int jumps = 0;

    bool loaded() const { return in_tree; }
    double offset() const { return at; }
    double extent() const { return scrollable; }
    bool scroll_to(double offset) const {
        ++jumps;
        if (refuses) return false;
        going = offset;
        return true;
    }

    void arrive(observable<double>& field) const {
        at = going;
        field.set(at);
    }
};

// Both ways, as binding.cpp binds: the watch first, then the field settled once.
template <class Element>
void bind_focus(Element const& element, observable<bool>& field) {
    field.watch_for_binding(
        [&element, &field](bool) noexcept { wxl::impl::settle_focus(element, field); });
    wxl::impl::settle_focus(element, field);
}

template <class Viewer>
void bind_offset(Viewer const& viewer, observable<double>& field) {
    field.watch_for_binding(
        [&viewer, &field](double const&) noexcept { wxl::impl::settle_offset(viewer, field); });
    wxl::impl::settle_offset(viewer, field);
}

template <class T>
void record(observable<T>& field, std::vector<T>& heard) {
    field.on_change([&heard](T const& value) noexcept { heard.push_back(value); });
}

}  // namespace

int main() {
    {
        fake_element element;
        observable<bool> focused{true};
        bind_focus(element, focused);
        check(element.attempts == 0 && focused.get(), "out of the tree: the request waits");

        element.in_tree = true;
        wxl::impl::settle_focus(element, focused);  // Loaded
        check(element.attempts == 1 && element.has_focus && focused.get(),
              "Loaded serves the request");
    }

    {
        fake_element element{.in_tree = true, .focusable = false};
        observable<bool> focused;
        std::vector<bool> heard;
        bind_focus(element, focused);
        record(focused, heard);

        focused.set(true);
        check(element.attempts == 1 && !focused.get(), "refused focus: the field reads false");
        check(!heard.empty() && !heard.back(), "refused focus: the application hears false last");

        element.focusable = true;
        focused.set(true);
        check(element.attempts == 2 && element.has_focus && focused.get(),
              "after a refusal the next request is a change, and is served");
    }

    {
        fake_element element{.in_tree = true};
        observable<bool> focused;
        std::vector<bool> heard;
        bind_focus(element, focused);
        focused.set(true);
        record(focused, heard);

        focused.set(false);
        check(element.has_focus && focused.get(), "false on a focused element reads true");
        check(!heard.empty() && heard.back(), "the application hears true last");

        wxl::impl::settle_focus(element, focused);  // Loaded again
        check(element.attempts == 1, "a field agreeing with the element asks nothing");

        // LostFocus: the report writes what the element says, and nothing settles it back.
        element.has_focus = false;
        focused.set(element.focused());
        check(element.attempts == 1 && !focused.get(), "focus moving away is reported, not fought");
    }

    {
        fake_viewer viewer{.in_tree = false};
        observable<double> offset{300.0};
        bind_offset(viewer, offset);
        check(viewer.jumps == 0 && offset.get() == 300.0, "out of the tree: the offset waits");

        viewer.in_tree = true;
        wxl::impl::settle_offset(viewer, offset);  // Loaded
        check(viewer.jumps == 1 && viewer.going == 300.0, "Loaded scrolls to it");
        viewer.arrive(offset);
        check(viewer.jumps == 1 && offset.get() == 300.0, "the view at rest agrees with the field");
    }

    {
        fake_viewer viewer;
        observable<double> offset;
        std::vector<double> heard;
        bind_offset(viewer, offset);
        record(offset, heard);

        offset.set(900.0);
        check(viewer.jumps == 1 && viewer.going == 500.0 && offset.get() == 500.0,
              "past the extent: cut to it, and the viewer asked once");
        bool never_past = true;
        for (double const value : heard) never_past = never_past && value == 500.0;
        check(!heard.empty() && never_past, "past the extent: the application hears the cut value");
        viewer.arrive(offset);

        offset.set(-20.0);
        check(viewer.jumps == 2 && viewer.going == 0.0 && offset.get() == 0.0, "below zero: zero");
        viewer.arrive(offset);

        viewer.at = 200.0;
        offset.set(200.0);  // ViewChanged after a pan
        check(viewer.jumps == 2, "the offset reported by the viewer asks nothing");

        offset.set(std::numeric_limits<double>::quiet_NaN());
        check(viewer.jumps == 2 && offset.get() == 200.0,
              "not a number: the field reads the offset now");

        viewer.at = 500.0;
        offset.set(500.0);
        offset.set(800.0);
        check(viewer.jumps == 2 && offset.get() == 500.0,
              "at the end already: no jump, the field stays");

        viewer.refuses = true;
        offset.set(100.0);
        check(viewer.jumps == 3 && offset.get() == 500.0,
              "a refused jump: the field reads the offset now");
    }

    if (failures == 0) std::puts("state_pair_test: all passed");
    return failures == 0 ? 0 : 1;
}
