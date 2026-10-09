#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

using changes_t = std::vector<list_change>;

template <class T>
std::vector<T> items_of(observable_list<T const>& list) {
    return std::vector<T>(list.begin(), list.end());
}

TEST(ObservableList, starts_empty_or_with_the_items_given) {
    observable_list<int> none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.size(), 0u);
    EXPECT_EQ(none.count().get(), 0u);

    observable_list<int> some{sta_vector<int>{4, 5, 6}};
    EXPECT_EQ(some.size(), 3u);
    EXPECT_EQ(some.count().get(), 3u);
    EXPECT_EQ(some[1], 5);
    EXPECT_EQ(items_of<int>(some), (std::vector<int>{4, 5, 6}));
    EXPECT_EQ(some.get().size(), 3u);
}

// Every change says what it was and where, and is heard with the list already changed: a
// view repeats it with one operation of its own.
TEST(ObservableList, each_change_says_what_and_where) {
    observable_list<int> list;
    changes_t heard;
    std::vector<std::vector<int>> seen;
    list.on_change([&](list_change const& change) noexcept {
        heard.push_back(change);
        seen.push_back(items_of<int>(list));
    });

    list.push_back(1);
    list.push_back(2);
    list.insert(0, 0);
    list.insert_range(2, std::vector<int>{10, 11});
    list.erase(1, 2);
    list.replace(0, 7);
    list.assign({3, 4, 5, 6});
    list.erase(3);
    list.clear();

    EXPECT_EQ(heard, (changes_t{
                         {list_change::inserted, 0, 1},
                         {list_change::inserted, 1, 1},
                         {list_change::inserted, 0, 1},
                         {list_change::inserted, 2, 2},
                         {list_change::erased, 1, 2},
                         {list_change::replaced, 0, 1},
                         {list_change::reset, 0, 4},
                         {list_change::erased, 3, 1},
                         {list_change::reset, 0, 0},
                     }));
    EXPECT_EQ(seen, (std::vector<std::vector<int>>{
                        {1},
                        {1, 2},
                        {0, 1, 2},
                        {0, 1, 10, 11, 2},
                        {0, 11, 2},
                        {7, 11, 2},
                        {3, 4, 5, 6},
                        {3, 4, 5},
                        {},
                    }));
}

// What leaves the list as it was says nothing -- the rule that keeps a view from rebuilding
// for an echo, the same as observable::set's.
TEST(ObservableList, a_change_to_the_same_says_nothing) {
    observable_list<int> list{sta_vector<int>{1, 2}};
    int calls = 0;
    list.on_change([&calls](list_change const&) noexcept { ++calls; });

    list.replace(1, 2);
    list.assign({1, 2});
    list.erase(0, 0);
    list.insert_range(1, std::vector<int>{});
    EXPECT_EQ(calls, 0);

    list.clear();
    EXPECT_EQ(calls, 1);
    list.clear();
    list.assign({});
    EXPECT_EQ(calls, 1);
}

// The bindings hear first, then the size changes, then the application: by the time a
// watcher saving the model runs, the views and the "nothing here yet" line show the change.
TEST(ObservableList, bindings_then_count_then_the_application) {
    observable_list<int> list;
    std::vector<char> order;
    list.on_change([&](list_change const&) noexcept { order.push_back('a'); });
    list.count().on_change([&](std::uint32_t const&) noexcept { order.push_back('n'); });
    list.watch_for_binding([&](list_change const&) noexcept { order.push_back('b'); });

    list.push_back(1);
    EXPECT_EQ(order, (std::vector<char>{'b', 'n', 'a'}));

    order.clear();
    list.replace(0, 2);  // the size stays, and its field says nothing
    EXPECT_EQ(order, (std::vector<char>{'b', 'a'}));
}

// The size as a field of its own: bound like any other, heard only when the size moves.
TEST(ObservableList, count_is_a_field_that_follows_the_size) {
    observable_list<int> list;
    std::vector<std::uint32_t> sizes;
    list.count().watch_for_binding([&](std::uint32_t const& n) noexcept { sizes.push_back(n); });

    list.push_back(1);
    list.insert_range(0, std::vector<int>{2, 3});
    list.replace(0, 9);
    list.assign({4, 5, 6});
    list.erase(0);
    list.clear();
    EXPECT_EQ(sizes, (std::vector<std::uint32_t>{1, 3, 2, 0}));
    EXPECT_EQ(list.count().get(), 0u);
}

TEST(ObservableList, a_cookie_removes_the_watch) {
    observable_list<int> list;
    int calls = 0;
    cookie_t const cookie = list.on_change([&calls](list_change const&) noexcept { ++calls; });

    list.push_back(1);
    EXPECT_TRUE(list.remove_change(cookie));
    list.push_back(2);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(list.remove_change(cookie));
}

// Follows a field: the items are computed from it now and on every change, as one reset.
TEST(ObservableList, follows_an_observable) {
    observable<int> limit{2};
    observable_list<int> list;
    changes_t heard;
    list.follow(limit, [](int n) {
        sta_vector<int> items;
        for (int i = 0; i < n; ++i) items.push_back(i);
        return items;
    });
    list.on_change([&heard](list_change const& change) noexcept { heard.push_back(change); });
    EXPECT_EQ(items_of<int>(list), (std::vector<int>{0, 1}));

    limit.set(3);
    EXPECT_EQ(items_of<int>(list), (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(heard, (changes_t{{list_change::reset, 0, 3}}));
}

// Follows another list -- a filter over it -- and a field beside it.
TEST(ObservableList, follows_another_list) {
    observable_list<int> all;
    observable<int> above{0};
    observable_list<int> chosen;
    chosen.follow(all, above, [](std::span<int const> items, int floor) {
        sta_vector<int> kept;
        for (int v : items) {
            if (v > floor) kept.push_back(v);
        }
        return kept;
    });
    EXPECT_TRUE(chosen.empty());

    all.assign({1, 5, 2, 7});
    EXPECT_EQ(items_of<int>(chosen), (std::vector<int>{1, 5, 2, 7}));

    above.set(2);
    EXPECT_EQ(items_of<int>(chosen), (std::vector<int>{5, 7}));

    all.push_back(9);
    all.erase(1);
    EXPECT_EQ(items_of<int>(chosen), (std::vector<int>{7, 9}));
}

// A model hands out its list read-only: the view reads it, watches it and binds to it, and
// only the model changes it.
struct Search {
    observable_list<int const>& hits() { return hits_; }
    void found(int hit) { hits_.push_back(hit); }

private:
    observable_list<int> hits_;
};

template <class List>
concept writable = requires(List& list) {
    list.push_back(1);
    list.insert(0, 1);
    list.erase(0);
    list.replace(0, 1);
    list.assign({});
    list.clear();
};

TEST(ObservableList, a_readonly_list_is_seen_and_not_written) {
    static_assert(writable<observable_list<int>>);
    static_assert(!writable<observable_list<int const>>);
    static_assert(!std::is_constructible_v<observable_list<int const>>);
    static_assert(!std::is_destructible_v<observable_list<int const>>);
    static_assert(std::is_convertible_v<observable_list<int>&, observable_list<int const>&>);
    static_assert(!std::is_copy_constructible_v<observable_list<int>>);
    static_assert(!std::is_move_constructible_v<observable_list<int>>);
    static_assert(!std::is_copy_assignable_v<observable_list<int>>);

    Search search;
    observable_list<int const>& hits = search.hits();
    int bound = 0;
    std::uint32_t counted = 0;
    hits.watch_for_binding([&bound](list_change const&) noexcept { ++bound; });
    hits.count().on_change([&counted](std::uint32_t const& n) noexcept { counted = n; });

    search.found(3);
    EXPECT_EQ(bound, 1);
    EXPECT_EQ(counted, 1u);
    EXPECT_EQ(hits[0], 3);

    int sum = 0;
    for (int hit : hits) sum += hit;
    EXPECT_EQ(sum, 3);
}

struct probe {
    int* deaths;

    explicit probe(int& count) noexcept : deaths(&count) {}
    probe(probe&& other) noexcept : deaths(std::exchange(other.deaths, nullptr)) {}
    probe(probe const&) = delete;
    ~probe() {
        if (deaths) ++*deaths;
    }
};

// Every watch goes with the list, and what it held with it -- the bindings' and the
// application's, the size field's too.
TEST(ObservableList, watches_die_with_the_list) {
    int owned = 0;
    {
        observable_list<int> list;
        list.watch_for_binding([p = probe{owned}](list_change const&) noexcept {});
        list.on_change([p = probe{owned}](list_change const&) noexcept {});
        list.count().watch_for_binding([p = probe{owned}](std::uint32_t const&) noexcept {});
        EXPECT_EQ(owned, 0);
    }
    EXPECT_EQ(owned, 3);
}

// unbind() and unsubscribe_all cut a list's binding watches as they do a field's, and leave
// the application's.
TEST(ObservableList, unbind_cuts_the_binding_watches_alone) {
    observable_list<int> list;
    observable<int> field{0};
    int bound = 0;
    int app = 0;
    int fieldBound = 0;
    list.watch_for_binding([&bound](list_change const&) noexcept { ++bound; });
    list.on_change([&app](list_change const&) noexcept { ++app; });
    field.watch_for_binding([&fieldBound](int const&) noexcept { ++fieldBound; });

    unsubscribe_all(list, field);
    list.push_back(1);
    field.set(1);
    EXPECT_EQ(bound, 0);
    EXPECT_EQ(app, 1);
    EXPECT_EQ(fieldBound, 0);
}

// An item that changes in part while it is shown: an item with a field of its own, held by
// pointer. The field changes and the list says nothing; the item goes with the list's last
// pointer to it.
struct Card : sta_refcounted {
    explicit Card(int& deaths) : deaths_(&deaths) {}
    ~Card() override { ++*deaths_; }

    observable<int> progress{0};

private:
    int* deaths_;
};

TEST(ObservableList, items_with_fields_of_their_own) {
    int deaths = 0;
    observable_list<intrusive_ptr<Card>> cards;
    int listChanges = 0;
    int progress = -1;
    cards.on_change([&listChanges](list_change const&) noexcept { ++listChanges; });

    cards.push_back(intrusive_ptr<Card>{new Card{deaths}, false});
    cards[0]->progress.watch_for_binding([&progress](int const& v) noexcept { progress = v; });

    cards[0]->progress.set(40);
    EXPECT_EQ(progress, 40);
    EXPECT_EQ(listChanges, 1);  // the push_back alone

    cards.replace(0, cards[0]);  // the same pointer: nothing to say
    EXPECT_EQ(listChanges, 1);

    cards.erase(0);
    EXPECT_EQ(deaths, 1);
    EXPECT_EQ(listChanges, 2);
}

}  // namespace
