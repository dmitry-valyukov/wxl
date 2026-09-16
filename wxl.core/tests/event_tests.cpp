#include <gtest/gtest.h>

import std;
import wxl.core;



namespace {

using wxl::core::cookie_t;
using wxl::core::event;

struct Widget {
    int value = 0;
};

TEST(EventTest, DefaultConstructedEventIsEmpty) {
    event<void(Widget&)> e;
    EXPECT_FALSE(static_cast<bool>(e));
}

TEST(EventTest, AddedCallbackBecomesTruthy) {
    event<void(Widget&)> e;
    e.add([](Widget&) noexcept {});
    EXPECT_TRUE(static_cast<bool>(e));
}

TEST(EventTest, FireInvokesAllCallbacksWithTheObject) {
    event<void(Widget&)> e;
    std::vector<int> seen;

    e.add([&seen](Widget& w) noexcept { seen.push_back(w.value); });
    e.add([&seen](Widget& w) noexcept { seen.push_back(w.value * 10); });

    Widget w{3};
    e.fire(w);

    EXPECT_EQ(seen, (std::vector<int>{3, 30}));
}

TEST(EventTest, CallbacksFireInRegistrationOrder) {
    event<void(Widget&)> e;
    std::vector<int> order;

    e.add([&order](Widget&) noexcept { order.push_back(1); });
    e.add([&order](Widget&) noexcept { order.push_back(2); });
    e.add([&order](Widget&) noexcept { order.push_back(3); });

    Widget w;
    e.fire(w);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

// The other add(): the callback is made outside the event and handed over ready. This is
// what component::subscribe_on_started() does, so that what its own caller wrote goes into
// the node directly instead of being copied into a std::function on the way.
TEST(EventTest, ACallbackMadeOutsideIsTakenOverAsAnyOther) {
    event<void(Widget&)> e;
    std::vector<int> seen;

    using callback = event<void(Widget&)>::func_t;
    const cookie_t cookie =
        e.add(callback::create([&seen](Widget& w) noexcept { seen.push_back(w.value); }));

    Widget w{7};
    e.fire(w);
    EXPECT_EQ(seen, (std::vector<int>{7}));

    // And the cookie names it just the same, so the event owns it and frees it.
    EXPECT_TRUE(e.remove(cookie));
    e.fire(w);
    EXPECT_EQ(seen, (std::vector<int>{7}));
}

// A move-only callable cannot go through a std::function at all, so this is the proof that
// nothing on the way makes a copy of what it is given.
TEST(EventTest, AMoveOnlyCallableCanBeAdded) {
    event<void()> e;
    auto counter = std::make_unique<int>(0);
    int* const seen = counter.get();

    e.add([held = std::move(counter)]() noexcept { ++*held; });

    e.fire();
    e.fire();
    EXPECT_EQ(2, *seen);
}

TEST(EventTest, RemoveStopsFutureFiresAndReportsWhetherItWasFound) {
    event<void(Widget&)> e;
    std::vector<int> calls;

    auto cookie = e.add([&calls](Widget&) noexcept { calls.push_back(1); });
    e.add([&calls](Widget&) noexcept { calls.push_back(2); });

    EXPECT_TRUE(e.remove(cookie));
    EXPECT_FALSE(e.remove(cookie));  // already removed

    Widget w;
    e.fire(w);

    EXPECT_EQ(calls, (std::vector<int>{2}));
}

TEST(EventTest, AnEventCanTakeNoArgumentsAtAll) {
    // The shape a plain notification has: nothing is passed, and in particular there is no
    // sender argument the event insists on.
    event<void()> e;
    int calls = 0;

    e.add([&calls]() noexcept { ++calls; });
    e.add([&calls]() noexcept { ++calls; });

    e.fire();

    EXPECT_EQ(calls, 2);
}

TEST(EventTest, ForwardsExtraArguments) {
    event<void(Widget&, int, std::string)> e;
    int seen_n = 0;
    std::string seen_s;

    e.add([&](Widget&, int n, std::string s) noexcept {
        seen_n = n;
        seen_s = std::move(s);
    });

    Widget w;
    e.fire(w, 42, std::string("hello"));

    EXPECT_EQ(seen_n, 42);
    EXPECT_EQ(seen_s, "hello");
}

TEST(EventTest, EveryCallbackGetsTheSameByValueArgument) {
    // Nothing is moved out of fire()'s arguments: they belong to all the callbacks, and
    // handing the first one the only copy would leave the second with a husk.
    event<void(std::string)> e;
    std::vector<std::string> seen;

    e.add([&seen](std::string s) noexcept { seen.push_back(std::move(s)); });
    e.add([&seen](std::string s) noexcept { seen.push_back(std::move(s)); });

    e.fire(std::string("hello"));

    EXPECT_EQ(seen, (std::vector<std::string>{"hello", "hello"}));
}

TEST(EventTest, ClearRemovesEveryCallback) {
    event<void()> e;
    int calls = 0;

    const cookie_t cookie = e.add([&calls]() noexcept { ++calls; });
    e.add([&calls]() noexcept { ++calls; });

    e.clear();

    EXPECT_FALSE(static_cast<bool>(e));
    EXPECT_FALSE(e.remove(cookie));  // gone with the rest

    e.fire();

    EXPECT_EQ(calls, 0);
}

TEST(EventTest, SwapExchangesTheCallbacksAndTheirCookies) {
    event<void()> one;
    event<void()> two;
    int from_one = 0;
    int from_two = 0;

    const cookie_t cookie = one.add([&from_one]() noexcept { ++from_one; });
    two.add([&from_two]() noexcept { ++from_two; });

    one.swap(two);
    one.fire();

    EXPECT_EQ(from_one, 0);
    EXPECT_EQ(from_two, 1);

    // The cookie went with its callback: it means nothing here now, and cancels there.
    EXPECT_FALSE(one.remove(cookie));
    EXPECT_TRUE(two.remove(cookie));

    two.fire();

    EXPECT_EQ(from_one, 0);
}

// The signature may be spelled with noexcept -- event<Sig> and event<Sig noexcept> are two
// spellings of the same event, offered so a type built on one (observable) can make the
// noexcept contract visible in its own declaration. The behaviour is identical.
TEST(EventTest, TheSignatureMayCarryNoexcept) {
    event<void(int) noexcept> e;
    std::vector<int> seen;

    const cookie_t cookie = e.add([&seen](int n) noexcept { seen.push_back(n); });
    e.add([&seen](int n) noexcept { seen.push_back(n * 10); });

    e.fire(4);
    EXPECT_EQ(seen, (std::vector<int>{4, 40}));

    EXPECT_TRUE(e.remove(cookie));
    e.fire(1);
    EXPECT_EQ(seen, (std::vector<int>{4, 40, 10}));
}

// The two spellings share one implementation, so a callback list is a callback list
// whichever way its event was written: swapping across the spellings moves the callbacks
// just as it does within one.
TEST(EventTest, TheTwoSpellingsShareTheSameMachinery) {
    event<void()> plain;
    event<void() noexcept> marked;
    int from_plain = 0;

    plain.add([&from_plain]() noexcept { ++from_plain; });
    plain.swap(marked);
    marked.fire();

    EXPECT_EQ(from_plain, 1);
    EXPECT_FALSE(static_cast<bool>(plain));
}

TEST(EventTest, DestructorFreesCallbacksThatWereNeverExplicitlyRemoved) {
    // Nothing to assert directly, but this must run cleanly (no crash, no
    // leak under a debug CRT heap / ASan) to prove event::~event() cleans up
    // every remaining callback on its own.
    event<void(Widget&)> e;
    e.add([](Widget&) noexcept {});
    e.add([](Widget&) noexcept {});
}

}  // namespace
