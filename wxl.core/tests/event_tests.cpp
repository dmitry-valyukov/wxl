#include <gtest/gtest.h>

#include <crtdbg.h>

import std;
import wxl.core;



namespace {

using wxl::core::as_not_null;
using wxl::core::cookie_t;
using wxl::core::event;
using wxl::core::event_mt;

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
    e(w);

    EXPECT_EQ(seen, (std::vector<int>{3, 30}));
}

TEST(EventTest, CallbacksFireInRegistrationOrder) {
    event<void(Widget&)> e;
    std::vector<int> order;

    e.add([&order](Widget&) noexcept { order.push_back(1); });
    e.add([&order](Widget&) noexcept { order.push_back(2); });
    e.add([&order](Widget&) noexcept { order.push_back(3); });

    Widget w;
    e(w);

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
    e(w);
    EXPECT_EQ(seen, (std::vector<int>{7}));

    // And the cookie names it just the same, so the event owns it and frees it.
    EXPECT_TRUE(e.remove(cookie));
    e(w);
    EXPECT_EQ(seen, (std::vector<int>{7}));
}

// A move-only callable cannot go through a std::function at all, so this is the proof that
// nothing on the way makes a copy of what it is given.
TEST(EventTest, AMoveOnlyCallableCanBeAdded) {
    event<void()> e;
    auto counter = std::make_unique<int>(0);
    int* const seen = counter.get();

    e.add([held = std::move(counter)]() noexcept { ++*held; });

    e();
    e();
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
    e(w);

    EXPECT_EQ(calls, (std::vector<int>{2}));
}

TEST(EventTest, AnEventCanTakeNoArgumentsAtAll) {
    // The shape a plain notification has: nothing is passed, and in particular there is no
    // sender argument the event insists on.
    event<void()> e;
    int calls = 0;

    e.add([&calls]() noexcept { ++calls; });
    e.add([&calls]() noexcept { ++calls; });

    e();

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
    e(w, 42, std::string("hello"));

    EXPECT_EQ(seen_n, 42);
    EXPECT_EQ(seen_s, "hello");
}

TEST(EventTest, EveryCallbackGetsTheSameByValueArgument) {
    // Nothing is moved out of the call's arguments: they belong to all the callbacks, and
    // handing the first one the only copy would leave the second with a husk.
    event<void(std::string)> e;
    std::vector<std::string> seen;

    e.add([&seen](std::string s) noexcept { seen.push_back(std::move(s)); });
    e.add([&seen](std::string s) noexcept { seen.push_back(std::move(s)); });

    e(std::string("hello"));

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

    e();

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
    one();

    EXPECT_EQ(from_one, 0);
    EXPECT_EQ(from_two, 1);

    // The cookie went with its callback: it means nothing here now, and cancels there.
    EXPECT_FALSE(one.remove(cookie));
    EXPECT_TRUE(two.remove(cookie));

    two();

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

    e(4);
    EXPECT_EQ(seen, (std::vector<int>{4, 40}));

    EXPECT_TRUE(e.remove(cookie));
    e(1);
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
    marked();

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

// What the tests below share: a callback that records its number, and a slot to keep the
// cookie of one that has to find itself. A cookie names a node and is not assignable, so it
// is emplaced once it exists.
using order_t = std::vector<int>;
using slot_t = std::optional<cookie_t>;

// Counts its own deaths, once per object: a moved-from one dies silently. Captured by a
// callback, it tells when the event let go of that callback.
struct death_probe {
    int* deaths;
    explicit death_probe(int& count) noexcept : deaths(&count) {}
    death_probe(death_probe&& other) noexcept : deaths(std::exchange(other.deaths, nullptr)) {}
    ~death_probe() {
        if (deaths) ++*deaths;
    }
};

// Runs a piece of code from its destructor, once: what a callback's destructor does when it
// reaches back into the event.
struct on_death {
    std::function<void()>* body;
    explicit on_death(std::function<void()>& fn) noexcept : body(&fn) {}
    on_death(on_death&& other) noexcept : body(std::exchange(other.body, nullptr)) {}
    ~on_death() {
        if (body && *body) (*body)();
    }
};

using noexcept_callback = event<void() noexcept>::func_t;

// A callback takes itself off in the middle of the walk: it finishes its own call, the walk
// goes on to the end, and the next fire no longer meets it.
TEST(EventTest, ACallbackRemovingItselfDuringFireLetsTheWalkFinish) {
    event<void()> e;
    order_t order;
    slot_t self;

    e.add([&order]() noexcept { order.push_back(1); });
    self.emplace(e.add([&order, &e, &self]() noexcept {
        order.push_back(2);
        EXPECT_TRUE(e.remove(*self));
        order.push_back(20);  // still running, on a node that is not freed under it
    }));
    e.add([&order]() noexcept { order.push_back(3); });

    e();
    EXPECT_EQ(order, (order_t{1, 2, 20, 3}));

    order.clear();
    e();
    EXPECT_EQ(order, (order_t{1, 3}));
    EXPECT_FALSE(e.remove(*self));
}

// What a removed callback held goes when it returns from the call that removed it -- not
// before, since its code is still running, and not later.
TEST(EventTest, ACallbackRemovedDuringItsOwnCallIsDestroyedWhenItReturns) {
    struct probe {
        int* deaths;
        explicit probe(int& count) noexcept : deaths(&count) {}
        probe(probe&& other) noexcept : deaths(std::exchange(other.deaths, nullptr)) {}
        ~probe() {
            if (deaths) ++*deaths;
        }
    };

    event<void()> e;
    int deaths = 0;
    int seenInside = -1;
    slot_t self;
    self.emplace(e.add([&e, &self, &deaths, &seenInside, p = probe{deaths}]() noexcept {
        static_cast<void>(e.remove(*self));
        seenInside = deaths;
    }));

    e();
    EXPECT_EQ(seenInside, 0);
    EXPECT_EQ(deaths, 1);
    EXPECT_FALSE(static_cast<bool>(e));
}

// One callback removes another that has not had its turn yet: that one is not called, by
// this fire or any later one.
TEST(EventTest, ACallbackRemovedBeforeItsTurnIsNotCalled) {
    event<void()> e;
    order_t order;
    slot_t third;

    e.add([&order, &e, &third]() noexcept {
        order.push_back(1);
        EXPECT_TRUE(e.remove(*third));
    });
    e.add([&order]() noexcept { order.push_back(2); });
    third.emplace(e.add([&order]() noexcept { order.push_back(3); }));

    e();
    EXPECT_EQ(order, (order_t{1, 2}));
}

// The callback right after the one doing the removing, and the last one: the two places
// where the walk keeps a pointer of its own.
TEST(EventTest, RemovingTheNextAndTheLastCallbackDuringFire) {
    event<void()> e;
    order_t order;
    slot_t second;
    slot_t last;

    e.add([&]() noexcept {
        order.push_back(1);
        static_cast<void>(e.remove(*second));
        static_cast<void>(e.remove(*last));
    });
    second.emplace(e.add([&order]() noexcept { order.push_back(2); }));
    e.add([&order]() noexcept { order.push_back(3); });
    last.emplace(e.add([&order]() noexcept { order.push_back(4); }));

    e();
    EXPECT_EQ(order, (order_t{1, 3}));
}

// Removing one that has been called already changes nothing for this fire.
TEST(EventTest, RemovingAnEarlierCallbackDuringFireLeavesTheWalkAlone) {
    event<void()> e;
    order_t order;
    slot_t first;

    first.emplace(e.add([&order]() noexcept { order.push_back(1); }));
    e.add([&order, &e, &first]() noexcept {
        order.push_back(2);
        static_cast<void>(e.remove(*first));
    });
    e.add([&order]() noexcept { order.push_back(3); });

    e();
    EXPECT_EQ(order, (order_t{1, 2, 3}));

    order.clear();
    e();
    EXPECT_EQ(order, (order_t{2, 3}));
}

// A callback added while the event fires waits for the next fire, wherever the walk is --
// even when it is added by the last callback, or added and then taken off again.
TEST(EventTest, ACallbackAddedDuringFireWaitsForTheNextOne) {
    event<void()> e;
    order_t order;
    bool added = false;
    slot_t gone;

    e.add([&]() noexcept {
        order.push_back(1);
        if (added) return;
        added = true;
        gone.emplace(e.add([&order]() noexcept { order.push_back(98); }));
        e.add([&order]() noexcept { order.push_back(4); });
        static_cast<void>(e.remove(*gone));
        e.add([&order]() noexcept { order.push_back(5); });
    });
    e.add([&order]() noexcept { order.push_back(2); });
    bool addedLast = false;
    e.add([&]() noexcept {
        order.push_back(3);
        if (addedLast) return;
        addedLast = true;
        e.add([&order]() noexcept { order.push_back(6); });
    });

    e();
    EXPECT_EQ(order, (order_t{1, 2, 3}));

    order.clear();
    e();
    EXPECT_EQ(order, (order_t{1, 2, 3, 4, 5, 6}));
}

// clear() from inside a callback: nothing after it is called, and the callback doing it
// outlives its own removal until it returns.
TEST(EventTest, ClearDuringFireEndsTheWalk) {
    event<void()> e;
    order_t order;

    e.add([&order]() noexcept { order.push_back(1); });
    e.add([&order, &e]() noexcept {
        order.push_back(2);
        e.clear();
        order.push_back(20);
    });
    e.add([&order]() noexcept { order.push_back(3); });

    e();
    EXPECT_EQ(order, (order_t{1, 2, 20}));
    EXPECT_FALSE(static_cast<bool>(e));

    e();
    EXPECT_EQ(order, (order_t{1, 2, 20}));
}

// A callback fires the event again. The inner fire calls what the list holds when it
// starts -- including one added by the outer walk before it -- and a callback the inner
// walk removes is not called by the outer one either.
TEST(EventTest, AFireInsideAFireSeesTheListAsItIsThen) {
    event<void(int)> e;
    std::vector<std::pair<int, int>> calls;  // (depth, callback)
    slot_t victim;
    bool added = false;

    e.add([&](int depth) noexcept {
        calls.emplace_back(depth, 1);
        if (depth == 0 && !added) {
            added = true;
            e.add([&calls](int d) noexcept { calls.emplace_back(d, 9); });
        }
    });
    e.add([&](int depth) noexcept {
        calls.emplace_back(depth, 2);
        if (depth == 0) {
            e(1);
        } else {
            static_cast<void>(e.remove(*victim));
        }
    });
    victim.emplace(e.add([&calls](int d) noexcept { calls.emplace_back(d, 3); }));

    e(0);
    EXPECT_EQ(calls, (std::vector<std::pair<int, int>>{{0, 1}, {0, 2}, {1, 1}, {1, 2}, {1, 9}}));
}

// The same callback running in an outer and an inner fire removes itself in the inner one:
// it is freed once, by the outer walk, when the outer call returns.
TEST(EventTest, ACallbackRemovingItselfInAnInnerFireIsFreedByTheOuterOne) {
    event<void(int)> e;
    order_t order;
    slot_t self;

    self.emplace(e.add([&](int depth) noexcept {
        order.push_back(depth);
        if (depth == 0) {
            e(1);
            order.push_back(10);
        } else {
            EXPECT_TRUE(e.remove(*self));
        }
    }));
    e.add([&order](int depth) noexcept { order.push_back(100 + depth); });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 101, 10, 100}));
    EXPECT_FALSE(e.remove(*self));
}

// The same callback is current in three walks and removes itself in the innermost: it is
// freed once, by the outermost, when its outermost call returns -- not while any of the
// three is still running.
TEST(EventTest, ACallbackCurrentInThreeWalksIsFreedOnceByTheOutermost) {
    event<void(int)> e;
    order_t order;
    slot_t self;
    int deaths = 0;

    self.emplace(e.add([&, p = death_probe{deaths}](int depth) noexcept {
        order.push_back(depth);
        if (depth < 2) {
            e(depth + 1);
            order.push_back(10 + depth);
            EXPECT_EQ(deaths, 0) << "freed while its call at depth " << depth << " ran";
        } else {
            EXPECT_TRUE(e.remove(*self));
        }
    }));
    e.add([&order](int depth) noexcept { order.push_back(100 + depth); });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 2, 102, 11, 101, 10, 100}));
    EXPECT_EQ(deaths, 1);
    EXPECT_FALSE(e.remove(*self));
}

// A callback current in an outer and an inner walk is removed by a *third* one in the inner
// walk: it is freed when its outer call returns, before the outer walk goes on.
TEST(EventTest, ACallbackCurrentInTwoWalksRemovedByAThirdIsFreedAfterItsOuterCall) {
    event<void(int)> e;
    order_t order;
    slot_t a;
    int deaths = 0;

    a.emplace(e.add([&, p = death_probe{deaths}](int depth) noexcept {
        order.push_back(depth);
        if (depth == 0) {
            e(1);
            order.push_back(10);
            EXPECT_EQ(deaths, 0);
        }
    }));
    e.add([&](int depth) noexcept {
        order.push_back(100 + depth);
        if (depth == 1)
            EXPECT_TRUE(e.remove(*a));
        else
            EXPECT_EQ(deaths, 1) << "the outer call returned, so it is gone by now";
    });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 101, 10, 100}));
    EXPECT_EQ(deaths, 1);
}

// The outer walk's end -- the first callback added while it ran -- is removed by an inner
// walk: the end moves on (to nothing, here), and the next add sets it again. The removed one
// is never called; the one added after it waits for the next fire.
TEST(EventTest, TheEndOfAnOuterWalkRemovedByAnInnerOneMovesOn) {
    event<void(int)> e;
    order_t order;
    slot_t x;

    e.add([&](int depth) noexcept {
        order.push_back(depth);
        if (depth != 0) return;
        x.emplace(e.add([&order](int d) noexcept { order.push_back(70 + d); }));
        e(1);
        e.add([&order](int d) noexcept { order.push_back(80 + d); });
    });
    e.add([&](int depth) noexcept {
        order.push_back(100 + depth);
        if (depth == 1) EXPECT_TRUE(e.remove(*x));
    });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 101, 100}));

    order.clear();
    e(5);
    EXPECT_EQ(order, (order_t{5, 105, 85}));
}

// A callback added from an inner walk stops the outer walk too -- the end is set on every
// walk under way, not only the innermost -- and waits for the next fire.
TEST(EventTest, AnAddInAnInnerWalkStopsTheOuterWalkToo) {
    event<void(int)> e;
    order_t order;
    bool added = false;

    e.add([&](int depth) noexcept {
        order.push_back(depth);
        if (depth == 0) e(1);
    });
    e.add([&](int depth) noexcept {
        order.push_back(100 + depth);
        if (depth == 1 && !added) {
            added = true;
            e.add([&order](int d) noexcept { order.push_back(70 + d); });
        }
    });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 101, 100}));

    order.clear();
    e(5);
    EXPECT_EQ(order, (order_t{5, 105, 75}));
}

// clear() and then add() from one callback: the walk ends with the clear, and what was added
// after it waits for the next fire.
TEST(EventTest, ClearThenAddFromOneCallbackEndsTheWalkAndKeepsTheNewOne) {
    event<void()> e;
    order_t order;

    e.add([&order]() noexcept { order.push_back(1); });
    e.add([&]() noexcept {
        order.push_back(2);
        e.clear();
        e.add([&order]() noexcept { order.push_back(4); });
        order.push_back(20);
    });
    e.add([&order]() noexcept { order.push_back(3); });

    e();
    EXPECT_EQ(order, (order_t{1, 2, 20}));
    EXPECT_TRUE(static_cast<bool>(e));

    order.clear();
    e();
    EXPECT_EQ(order, (order_t{4}));
}

// clear() from an inner walk: the inner current goes when its call returns, the outer
// current when *its* call returns -- neither while running, and each once.
TEST(EventTest, ClearInAnInnerWalkFreesEachCurrentWhenItsOwnCallReturns) {
    event<void(int)> e;
    order_t order;
    int deathsA = 0;
    int deathsB = 0;

    e.add([&, p = death_probe{deathsA}](int depth) noexcept {
        order.push_back(depth);
        if (depth != 0) return;
        e(1);
        order.push_back(10);
        EXPECT_EQ(deathsA, 0);
        EXPECT_EQ(deathsB, 1);
    });
    e.add([&, p = death_probe{deathsB}](int depth) noexcept {
        order.push_back(100 + depth);
        if (depth != 1) return;
        e.clear();
        EXPECT_EQ(deathsA, 0);
        EXPECT_EQ(deathsB, 0);
    });

    e(0);
    EXPECT_EQ(order, (order_t{0, 1, 101, 10}));
    EXPECT_EQ(deathsA, 1);
    EXPECT_EQ(deathsB, 1);
    EXPECT_FALSE(static_cast<bool>(e));
}

// The node the walk would both reach next and stop at: added, so it is the end, then the
// one before it removed, so it is next as well; then removed itself. Nothing is called
// twice or skipped, and what is added after it waits.
TEST(EventTest, RemovingTheNodeTheWalkWouldBothReachAndStopAt) {
    event<void()> e;
    order_t order;
    slot_t b;
    slot_t x;

    e.add([&]() noexcept {
        order.push_back(1);
        if (x) return;
        x.emplace(e.add([&order]() noexcept { order.push_back(9); }));
        EXPECT_TRUE(e.remove(*b));
        EXPECT_TRUE(e.remove(*x));
        e.add([&order]() noexcept { order.push_back(4); });
    });
    b.emplace(e.add([&order]() noexcept { order.push_back(2); }));

    e();
    EXPECT_EQ(order, (order_t{1}));

    order.clear();
    e();
    EXPECT_EQ(order, (order_t{1, 4}));
}

// A node built into its subscriber: a member deriving from the event's callback type, whose
// release() does nothing. The event links it, calls it and lets go of it, and never
// destroys it -- the subscriber does, as an ordinary member. Here it counts what is done
// to it.
class built_in final : public event<void() noexcept>::func_t
{
public:
    std::function<void()> body;
    int calls = 0;
    int released = 0;

    void operator()() noexcept override {
        ++calls;
        if (body) body();
    }

    void release() noexcept override { ++released; }
};

TEST(EventTest, ANodeBuiltIntoItsSubscriberIsAddedFiredAndRemovedWithoutBeingDestroyed) {
    event<void() noexcept> e;
    built_in node;

    const cookie_t cookie = e.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&node));
    e();
    EXPECT_EQ(node.calls, 1);

    EXPECT_TRUE(e.remove(cookie));
    EXPECT_EQ(node.released, 1);
    EXPECT_FALSE(static_cast<bool>(e));

    e();
    EXPECT_EQ(node.calls, 1);

    // Its link was cleared on the way out, so it may stand again.
    e.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&node));
    e();
    EXPECT_EQ(node.calls, 2);
    EXPECT_TRUE(e.remove(cookie));
    EXPECT_EQ(node.released, 2);
}

// Taking itself out from inside its own call: the walk goes on to the next callback, and
// lets go of the node once its call has returned -- once.
TEST(EventTest, ANodeBuiltIntoItsSubscriberMayLeaveFromItsOwnCall) {
    event<void() noexcept> e;
    built_in node;
    int after = 0;

    const cookie_t cookie = e.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&node));
    e.add([&after]() noexcept { ++after; });

    node.body = [&] {
        EXPECT_TRUE(e.remove(cookie));
        EXPECT_EQ(node.released, 0) << "let go of while its own call ran";
    };

    e();
    EXPECT_EQ(node.calls, 1);
    EXPECT_EQ(node.released, 1);
    EXPECT_EQ(after, 1);

    e();
    EXPECT_EQ(node.calls, 1);
    EXPECT_EQ(after, 2);
}

// clear() and the event's own end let go of a built-in node as they do of any other, and
// the node, being its subscriber's, survives both.
TEST(EventTest, ClearAndTheEventsEndLetGoOfABuiltInNodeWithoutDestroyingIt) {
    built_in node;
    int made_calls = 0;

    {
        event<void() noexcept> e;
        e.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&node));
        e.add([&made_calls]() noexcept { ++made_calls; });

        e.clear();
        EXPECT_EQ(node.released, 1);

        e();
        EXPECT_EQ(node.calls, 0);
        EXPECT_EQ(made_calls, 0);

        e.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&node));
    }

    EXPECT_EQ(node.released, 2);
    EXPECT_EQ(node.calls, 0);
}

// What happens once -- a cancellation -- fires through a local event: the callbacks are
// swapped into it, fired there, and let go of with it. A subscriber that takes itself out
// of the first event afterwards finds it gone, and remove() says so; the node may stand in
// another event at once.
TEST(EventTest, AOneTimeFireThroughALocalEventLetsGoOfBuiltInNodes) {
    event<void() noexcept> state;
    built_in first, second;

    const cookie_t first_cookie =
        state.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&first));
    state.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&second));

    {
        event<void() noexcept> once;
        once.swap(state);
        once();
    }

    EXPECT_EQ(first.calls, 1);
    EXPECT_EQ(second.calls, 1);
    EXPECT_EQ(first.released, 1);
    EXPECT_EQ(second.released, 1);
    EXPECT_FALSE(static_cast<bool>(state));
    EXPECT_FALSE(state.remove(first_cookie));

    state.add(wxl::core::as_not_null<event<void() noexcept>::func_t>(&first));
    state();
    EXPECT_EQ(first.calls, 2);
    EXPECT_EQ(second.calls, 1);
    EXPECT_TRUE(state.remove(first_cookie));
}

// A built-in node takes itself out from inside its own call and stands again before the
// call has returned. That is a mistake in the subscriber -- the event lets go of the node,
// and clears its link, only once the call returns -- so it takes the process down in a
// debug build, to stderr rather than to a dialog that would hang the run.
void stand_again_from_inside_the_call() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    event<void() noexcept> e;
    built_in node;
    const cookie_t cookie = e.add(as_not_null<noexcept_callback>(&node));

    node.body = [&] {
        EXPECT_TRUE(e.remove(cookie));
        e.add(as_not_null<noexcept_callback>(&node));
    };
    e();
}

TEST(EventDeathTest, ABuiltInNodeMayNotStandAgainFromInsideTheCallThatRemovedIt) {
    EXPECT_DEBUG_DEATH(stand_again_from_inside_the_call(), "");
}

// While clear() lets go of what it emptied, one callback's destructor asks the event to
// remove another: the answer is false -- it is out of the list -- but the event has not let
// go of that node yet, and does so afterwards. A subscriber with a built-in node therefore
// lives until its release(), not just while remove() would find it.
TEST(EventTest, DuringClearRemoveSaysGoneBeforeTheNodeIsLetGoOf) {
    event<void() noexcept> e;
    built_in second;
    slot_t second_cookie;
    int released_when_asked = -1;
    bool answer = true;

    std::function<void()> reach_back = [&] {
        answer = e.remove(*second_cookie);
        released_when_asked = second.released;
    };
    e.add([r = on_death{reach_back}]() noexcept {});
    second_cookie.emplace(e.add(as_not_null<noexcept_callback>(&second)));

    e.clear();
    EXPECT_FALSE(answer);
    EXPECT_EQ(released_when_asked, 0);
    EXPECT_EQ(second.released, 1);
}

// A callback's destructor, run while the event itself is being destroyed, subscribes to it:
// the event ends only once it has let go of that one too.
TEST(EventTest, ANodeAddedFromADestructorWhileTheEventEndsIsLetGoOf) {
    built_in late;
    std::function<void()> add_late;
    {
        event<void() noexcept> e;
        add_late = [&] { e.add(as_not_null<noexcept_callback>(&late)); };
        e.add([r = on_death{add_late}]() noexcept {});
    }
    EXPECT_EQ(late.released, 1);
    EXPECT_EQ(late.calls, 0);
}

// A built-in node whose release() puts it back and takes it out again -- from inside the
// release() the walk gives it once its call has returned. The walk no longer names it as
// the callback being called by then, so the second removal lets go of it at once, and the
// walk goes on to the next callback without mistaking it for a removed one.
class standing_again final : public noexcept_callback
{
public:
    event<void() noexcept>* owner = nullptr;
    int calls = 0;
    int released = 0;

    void operator()() noexcept override {
        ++calls;
        if (calls == 1) (void)owner->remove(cookie_t{static_cast<noexcept_callback*>(this)});
    }

    void release() noexcept override {
        if (++released != 1) return;
        owner->add(as_not_null<noexcept_callback>(this));
        (void)owner->remove(cookie_t{static_cast<noexcept_callback*>(this)});
    }
};

TEST(EventTest, ANodeTakingItselfOutAgainFromItsReleaseDoesNotCostTheNextCallbackItsLife) {
    event<void() noexcept> e;
    standing_again first;
    built_in second;
    first.owner = &e;

    e.add(as_not_null<noexcept_callback>(&first));
    e.add(as_not_null<noexcept_callback>(&second));

    e();
    EXPECT_EQ(first.calls, 1);
    EXPECT_EQ(first.released, 2);
    EXPECT_EQ(second.calls, 1);
    EXPECT_EQ(second.released, 0) << "let go of while still in the list";
    EXPECT_TRUE(static_cast<bool>(e));

    e();
    EXPECT_EQ(second.calls, 2);
}

// The heap-allocated spelling walks the same way: a callback removing itself from inside its
// call, one added meanwhile waiting, and a swap into a local event for a one-time fire.
TEST(EventTest, EventMtWalksLikeEvent) {
    event_mt<void(int)> e;
    order_t order;
    slot_t self;

    self.emplace(e.add([&](int n) noexcept {
        order.push_back(n);
        EXPECT_TRUE(e.remove(*self));
        e.add([&order](int m) noexcept { order.push_back(m * 10); });
    }));
    e.add([&order](int n) noexcept { order.push_back(n + 1); });

    e(1);
    EXPECT_EQ(order, (order_t{1, 2}));

    event_mt<void(int)> once;
    once.swap(e);
    EXPECT_FALSE(static_cast<bool>(e));
    once(3);
    EXPECT_EQ(order, (order_t{1, 2, 4, 30}));
}

}  // namespace
