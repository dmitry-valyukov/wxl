#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

TEST(Observable, holds_its_value) {
    observable<int> value{7};
    EXPECT_EQ(value.get(), 7);

    observable<bool> flag;  // default-constructed
    EXPECT_EQ(flag.get(), false);
}

TEST(Observable, set_notifies_on_a_real_change) {
    observable<int> value{1};
    int seen = 0;
    int calls = 0;
    value.on_change([&](int const& v) noexcept { ++calls; seen = v; });

    value.set(2);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(seen, 2);
    EXPECT_EQ(value.get(), 2);
}

TEST(Observable, setting_the_same_value_says_nothing) {
    observable<int> value{5};
    int calls = 0;
    value.on_change([&](int const&) noexcept { ++calls; });

    value.set(5);
    EXPECT_EQ(calls, 0);  // no change, no echo -- the rule a two-way binding needs

    value.set(6);
    EXPECT_EQ(calls, 1);
}

TEST(Observable, a_cookie_removes_the_watch) {
    observable<int> value{0};
    int calls = 0;
    cookie_t const cookie = value.on_change([&](int const&) noexcept { ++calls; });

    value.set(1);
    EXPECT_EQ(calls, 1);

    EXPECT_TRUE(value.remove_change(cookie));
    value.set(2);
    EXPECT_EQ(calls, 1);  // no longer watching

    EXPECT_FALSE(value.remove_change(cookie));  // gone already
}

// A field of its model, not a handle over shared state: there is no second
// observable that could be "the same one", so nothing to copy. Two controls
// bound to one switch share it by looking at the same field of the same model.
static_assert(!std::is_copy_constructible_v<observable<int>>);
static_assert(!std::is_copy_assignable_v<observable<int>>);
static_assert(!std::is_move_constructible_v<observable<int>>);

// What a watch owns goes with the watch: a binding's watch owns the control it
// writes to, and destroying the model -- the field with it -- is what lets that
// control go. The probe stands in for the control: it counts its own death.
struct probe {
    int* deaths;

    explicit probe(int& count) noexcept : deaths(&count) {}
    probe(probe&& other) noexcept : deaths(std::exchange(other.deaths, nullptr)) {}
    probe(probe const&) = delete;
    ~probe() {
        if (deaths) ++*deaths;
    }
};

TEST(Observable, watches_die_with_the_field_and_release_what_they_own) {
    int owned = 0;
    int appOwned = 0;
    {
        observable<int> value{0};
        value.watch_for_binding([p = probe{owned}](int const&) noexcept {});
        value.on_change([p = probe{appOwned}](int const&) noexcept {});
        EXPECT_EQ(owned, 0);
        EXPECT_EQ(appOwned, 0);
    }
    EXPECT_EQ(owned, 1);     // the binding's watch, and its control, went with the field
    EXPECT_EQ(appOwned, 1);  // and so did the application's
}

// The bindings hear first, the application after: a watcher saving the model
// runs when the controls already show what it is saving.
TEST(Observable, bindings_hear_before_the_application) {
    observable<int> value{0};
    std::vector<char> order;
    value.on_change([&](int const&) noexcept { order.push_back('a'); });
    value.watch_for_binding([&](int const&) noexcept { order.push_back('b'); });

    value.set(1);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 'b');
    EXPECT_EQ(order[1], 'a');
}

// watch_for_binding is heard just like on_change, but the field keeps the watch:
// there is nothing handed back to remove it with. It is unbind() that drops it,
// which is how a binding's model->control watch (the one that owns the control)
// is cut when the model outlives the window -- and what it owned is let go then.
TEST(Observable, a_binding_watch_is_the_fields_to_drop) {
    observable<int> value{0};
    int calls = 0;
    int owned = 0;
    value.watch_for_binding([&, p = probe{owned}](int const&) noexcept { ++calls; });

    value.set(1);
    EXPECT_EQ(calls, 1);

    value.unbind();
    EXPECT_EQ(owned, 1);  // the control the watch held is released here, not later
    value.set(2);
    EXPECT_EQ(calls, 1);  // the binding watch is gone
}

// unbind() cuts the binding watches and leaves the caller's on_change watches
// alone -- an application saving the model on every change keeps saving after a
// window that bound a control to it has closed.
TEST(Observable, unbind_spares_the_on_change_watches) {
    observable<int> value{0};
    int app = 0;
    int binding = 0;
    value.on_change([&](int const&) noexcept { ++app; });
    value.watch_for_binding([&](int const&) noexcept { ++binding; });

    value.set(1);
    EXPECT_EQ(app, 1);
    EXPECT_EQ(binding, 1);

    value.unbind();
    value.set(2);
    EXPECT_EQ(app, 2);      // still watched
    EXPECT_EQ(binding, 1);  // no longer
}

// unsubscribe_all is the window's teardown named for the models: it unbinds each
// one given, and only those. This is the shape the settings window uses from its
// Closed handler, its model living on after it.
TEST(Observable, unsubscribe_all_unbinds_each_model_named) {
    observable<bool> flag{false};
    observable<int> size{12};
    int flagCalls = 0;
    int sizeCalls = 0;
    int other = 0;
    observable<int> untouched{0};

    flag.watch_for_binding([&](bool const&) noexcept { ++flagCalls; });
    size.watch_for_binding([&](int const&) noexcept { ++sizeCalls; });
    untouched.watch_for_binding([&](int const&) noexcept { ++other; });

    unsubscribe_all(flag, size);

    flag.set(true);
    size.set(13);
    EXPECT_EQ(flagCalls, 0);
    EXPECT_EQ(sizeCalls, 0);

    untouched.set(1);
    EXPECT_EQ(other, 1);  // not named, not unbound
}

TEST(Observable, works_for_nullable_bool) {
    observable<nullable<bool>> tri{nullable<bool>{}};
    int calls = 0;
    nullable<bool> seen;
    tri.on_change([&](nullable<bool> const& v) noexcept { ++calls; seen = v; });

    tri.set(true);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(seen, nullable<bool>{true});

    tri.set(true);
    EXPECT_EQ(calls, 1);  // same value, no notification

    tri.set(nullable<bool>{});
    EXPECT_EQ(calls, 2);
    EXPECT_FALSE(seen.has_value());
}

// A model that sets a field itself and hands out only its read-only observable.
struct Counter {
    observable<int const>& value() { return value_; }
    void bump() { value_.set(value_.get() + 1); }

private:
    observable<int> value_{0};
};

template <class T>
concept settable = requires(T& field) { field.set(1); };

TEST(Observable, readonly_observable_hears_what_the_field_sets) {
    Counter counter;
    observable<int const>& readonly = counter.value();
    int watched = -1;
    int bound = -1;
    readonly.on_change([&](int const& v) noexcept { watched = v; });
    readonly.watch_for_binding([&](int const& v) noexcept { bound = v; });

    counter.bump();
    EXPECT_EQ(readonly.get(), 1);
    EXPECT_EQ(watched, 1);
    EXPECT_EQ(bound, 1);
}

TEST(Observable, readonly_observable_is_neither_set_nor_made_alone) {
    static_assert(settable<observable<int>>);
    static_assert(!settable<observable<int const>>);
    // Only the field is made and destroyed; the read-only observable exists as its base.
    static_assert(!std::is_constructible_v<observable<int const>>);
    static_assert(!std::is_destructible_v<observable<int const>>);
    static_assert(std::is_convertible_v<observable<int>&, observable<int const>&>);
}

TEST(Observable, follows_a_readonly_observable) {
    Counter counter;
    observable<int> doubled;
    doubled.follow(counter.value(), [](int v) { return v * 2; });
    EXPECT_EQ(doubled.get(), 0);

    counter.bump();
    EXPECT_EQ(doubled.get(), 2);
}

}  // namespace
