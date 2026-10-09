#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

// Stands in for the control a binding's watch owns: it counts its own death, so a test can
// tell when the watch let go of it.
struct probe {
    int* deaths;

    explicit probe(int& count) noexcept : deaths(&count) {}
    probe(probe&& other) noexcept : deaths(std::exchange(other.deaths, nullptr)) {}
    probe(probe const&) = delete;
    ~probe() {
        if (deaths) ++*deaths;
    }
};

// The case a scope is for: an element is built, its bindings are collected, and when the
// element is given back the scope takes them off the fields, which stay.
TEST(BindingScope, cuts_the_binding_watches_made_inside_it) {
    observable<int> number{0};
    observable<bool> flag{false};
    int seen = -1;
    int owned = 0;

    binding_scope scope;
    EXPECT_TRUE(scope.empty());
    scope.collect([&] {
        number.watch_for_binding([&seen, p = probe{owned}](int const& v) noexcept { seen = v; });
        flag.watch_for_binding([p = probe{owned}](bool const&) noexcept {});
    });
    EXPECT_FALSE(scope.empty());

    number.set(1);
    EXPECT_EQ(seen, 1);  // a collected watch is a watch like any other

    scope.cut();
    EXPECT_TRUE(scope.empty());
    EXPECT_EQ(owned, 2);  // what the watches held went with them, at once

    number.set(2);
    EXPECT_EQ(seen, 1);
}

// The scope is open while the build runs and only then; what the build returns comes back.
TEST(BindingScope, collect_opens_the_scope_for_the_build_and_returns_its_result) {
    binding_scope scope;
    EXPECT_EQ(binding_scope::current(), nullptr);

    int const result = scope.collect([&] {
        EXPECT_EQ(binding_scope::current(), &scope);
        return 42;
    });
    EXPECT_EQ(result, 42);
    EXPECT_EQ(binding_scope::current(), nullptr);
}

TEST(BindingScope, a_build_that_throws_closes_the_scope_all_the_same) {
    binding_scope scope;
    EXPECT_THROW(scope.collect([]() -> int { throw std::runtime_error("build failed"); }),
                 std::runtime_error);
    EXPECT_EQ(binding_scope::current(), nullptr);
}

// Only binding watches made while it is open: one made before, and the application's own
// watches made inside, stay where they are when the scope is cut.
TEST(BindingScope, collects_only_binding_watches_made_while_open) {
    observable<int> number{0};
    int before = 0;
    int app = 0;
    int inside = 0;

    number.watch_for_binding([&before](int const&) noexcept { ++before; });
    binding_scope scope;
    scope.collect([&] {
        number.on_change([&app](int const&) noexcept { ++app; });
        number.watch_for_binding([&inside](int const&) noexcept { ++inside; });
    });

    scope.cut();
    number.set(1);
    EXPECT_EQ(before, 1);
    EXPECT_EQ(app, 1);
    EXPECT_EQ(inside, 0);
}

// The field goes first -- an item model dropped from the list while its element is still
// shown. Its watches leave the scope as they die; the scope has nothing left to cut.
TEST(BindingScope, a_field_dying_first_leaves_the_scope_nothing_to_cut) {
    int owned = 0;
    binding_scope scope;
    {
        observable<int> number{0};
        scope.collect([&] {
            number.watch_for_binding([p = probe{owned}](int const&) noexcept {});
        });
        EXPECT_FALSE(scope.empty());
    }
    EXPECT_EQ(owned, 1);
    EXPECT_TRUE(scope.empty());
    scope.cut();  // and cutting it now touches nothing that is gone
}

// The scope goes first: its watches come off the field with it, and the field goes on
// telling everyone else.
TEST(BindingScope, a_scope_going_first_takes_its_watches_with_it) {
    observable<int> number{0};
    int owned = 0;
    int app = 0;
    number.on_change([&app](int const&) noexcept { ++app; });
    {
        binding_scope scope;
        scope.collect([&] {
            number.watch_for_binding([p = probe{owned}](int const&) noexcept {});
        });
    }
    EXPECT_EQ(owned, 1);

    number.set(1);
    EXPECT_EQ(app, 1);
    number.unbind();  // nothing collected is left on the field to trip over
}

// unbind() is the field's own way to drop binding watches, and the collected ones go too:
// the scope hears of it.
TEST(BindingScope, unbind_drops_collected_watches_out_of_their_scope) {
    observable<int> number{0};
    binding_scope scope;
    scope.collect([&] { number.watch_for_binding([](int const&) noexcept {}); });

    number.unbind();
    EXPECT_TRUE(scope.empty());
}

// An element built inside an element: the inner build's bindings belong to the inner scope
// alone, and after it the outer scope collects again.
TEST(BindingScope, nested_scopes_collect_apart) {
    observable<int> number{0};
    std::vector<char> heard;

    binding_scope outer;
    binding_scope inner;
    outer.collect([&] {
        number.watch_for_binding([&heard](int const&) noexcept { heard.push_back('a'); });
        inner.collect([&] {
            EXPECT_EQ(binding_scope::current(), &inner);
            number.watch_for_binding([&heard](int const&) noexcept { heard.push_back('b'); });
        });
        EXPECT_EQ(binding_scope::current(), &outer);
        number.watch_for_binding([&heard](int const&) noexcept { heard.push_back('c'); });
    });
    EXPECT_EQ(binding_scope::current(), nullptr);

    outer.cut();
    EXPECT_FALSE(inner.empty());
    number.set(1);
    EXPECT_EQ(heard, (std::vector<char>{'b'}));

    inner.cut();
    number.set(2);
    EXPECT_EQ(heard, (std::vector<char>{'b'}));
}

// Cut and opened again: the scope of a container that shows one item, then another.
TEST(BindingScope, collects_again_after_a_cut) {
    observable<int> number{0};
    int first = 0;
    int second = 0;

    binding_scope scope;
    scope.collect([&] { number.watch_for_binding([&first](int const&) noexcept { ++first; }); });
    scope.cut();
    scope.collect([&] { number.watch_for_binding([&second](int const&) noexcept { ++second; }); });

    number.set(1);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

// A list rebuilt from inside a watcher of the very field its elements are bound to: the
// scope is cut while that field fires. The watches it takes off are not called by the fire
// under way, and the walk goes on to the end.
TEST(BindingScope, cut_while_the_field_fires) {
    observable<int> number{0};
    binding_scope scope;
    std::vector<char> heard;
    int owned = 0;

    number.watch_for_binding([&](int const&) noexcept {
        heard.push_back('r');  // the list's own watch: it rebuilds
        scope.cut();
    });
    scope.collect([&] {
        number.watch_for_binding([&heard, p = probe{owned}](int const&) noexcept { heard.push_back('e'); });
        number.watch_for_binding([&heard, p = probe{owned}](int const&) noexcept { heard.push_back('e'); });
    });
    number.on_change([&heard](int const&) noexcept { heard.push_back('a'); });

    number.set(1);
    EXPECT_EQ(heard, (std::vector<char>{'r', 'a'}));
    EXPECT_EQ(owned, 2);
    EXPECT_TRUE(scope.empty());
}

// A collected watch that cuts its own scope from inside its call: it finishes the call, is
// freed when it returns, and the watch after it is not called.
TEST(BindingScope, a_watch_cutting_its_own_scope) {
    observable<int> number{0};
    binding_scope scope;
    std::vector<char> heard;
    int owned = 0;
    int ownedWhileRunning = -1;

    scope.collect([&] {
        number.watch_for_binding([&, p = probe{owned}](int const&) noexcept {
            heard.push_back('1');
            scope.cut();
            ownedWhileRunning = owned;
        });
        number.watch_for_binding([&heard, p = probe{owned}](int const&) noexcept { heard.push_back('2'); });
    });

    number.set(1);
    EXPECT_EQ(heard, (std::vector<char>{'1'}));
    EXPECT_EQ(ownedWhileRunning, 1);  // the second went at once, the first was still running
    EXPECT_EQ(owned, 2);
    EXPECT_TRUE(scope.empty());
}

// A list's binding watches are collected the same way as a field's.
TEST(BindingScope, collects_the_watches_of_a_list) {
    observable_list<int> numbers;
    int heard = 0;

    binding_scope scope;
    scope.collect([&] {
        numbers.watch_for_binding([&heard](list_change const&) noexcept { ++heard; });
        numbers.count().watch_for_binding([&heard](std::uint32_t const&) noexcept { ++heard; });
    });
    numbers.push_back(1);
    EXPECT_EQ(heard, 2);

    scope.cut();
    numbers.push_back(2);
    EXPECT_EQ(heard, 2);
}

}  // namespace
