// impl::list_mirror over a plain vector: every change of an observable_list, done to the
// vector, leaves it equal to the list, and each call is one item away from the last, the
// way a control listening to a WinRT vector expects to be told.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "impl/list_mirror.h"

namespace {

using wxl::core::list_change;
using wxl::core::observable_list;
using wxl::impl::list_mirror;

// A view of a list: the items it shows, read from the list as the mirror asks, and every
// call it was given, in order.
struct vector_view {
    observable_list<int const> const* list;
    std::vector<int> items;
    std::vector<std::string> calls;

    uint32_t size() const { return static_cast<uint32_t>(items.size()); }

    void insert(uint32_t at) {
        items.insert(items.begin() + at, (*list)[at]);
        calls.push_back("insert " + std::to_string(at));
    }

    void erase(uint32_t at) {
        items.erase(items.begin() + at);
        calls.push_back("erase " + std::to_string(at));
    }

    void replace(uint32_t at) {
        items[at] = (*list)[at];
        calls.push_back("replace " + std::to_string(at));
    }

    void reset(uint32_t size) {
        items.assign(list->begin(), list->end());
        EXPECT_EQ(size, items.size());
        calls.push_back("reset " + std::to_string(size));
    }
};

static_assert(wxl::impl::list_sink<vector_view>);

std::vector<int> items_of(observable_list<int> const& list) {
    return {list.begin(), list.end()};
}

// A view that starts as the list is, and follows it from here on.
struct mirrored {
    explicit mirrored(observable_list<int>& source, uint32_t threshold = wxl::impl::list_reset_threshold)
        : list(source), view{&source, items_of(source), {}} {
        list.on_change([this, threshold](list_change const& change) noexcept { list_mirror{view, threshold}(change); });
    }

    observable_list<int>& list;
    vector_view view;
};

TEST(list_mirror, an_insertion_is_an_item_at_a_time_in_order) {
    observable_list<int> list{{1, 2, 3}};
    mirrored m{list};

    list.insert_range(1, std::vector<int>{7, 8, 9});

    EXPECT_EQ(m.view.items, items_of(list));
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"insert 1", "insert 2", "insert 3"}));
}

TEST(list_mirror, one_item_pushed_is_one_insertion_at_the_end) {
    observable_list<int> list{{1, 2}};
    mirrored m{list};

    list.push_back(3);

    EXPECT_EQ(m.view.items, items_of(list));
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"insert 2"}));
}

TEST(list_mirror, an_erasure_takes_out_at_the_same_place) {
    observable_list<int> list{{1, 2, 3, 4, 5}};
    mirrored m{list};

    list.erase(1, 3);

    EXPECT_EQ(m.view.items, (std::vector<int>{1, 5}));
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"erase 1", "erase 1", "erase 1"}));
}

TEST(list_mirror, a_replacement_is_one_call_in_place) {
    observable_list<int> list{{1, 2, 3}};
    mirrored m{list};

    list.replace(2, 30);

    EXPECT_EQ(m.view.items, (std::vector<int>{1, 2, 30}));
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"replace 2"}));
}

TEST(list_mirror, an_assignment_and_a_clear_are_resets) {
    observable_list<int> list{{1, 2, 3}};
    mirrored m{list};

    list.assign({4, 5});
    list.clear();

    EXPECT_TRUE(m.view.items.empty());
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"reset 2", "reset 0"}));
}

// Every step a control is told of is one item away from the step before: the size it
// reads inside a call is the size it last knew, plus or minus one.
TEST(list_mirror, each_call_is_one_item_from_the_last) {
    observable_list<int> list{{1, 2, 3}};
    vector_view view{&list, items_of(list), {}};
    std::vector<uint32_t> sizes;
    struct counting {
        vector_view* view;
        std::vector<uint32_t>* sizes;
        uint32_t size() const { return view->size(); }
        void insert(uint32_t at) { view->insert(at); sizes->push_back(view->size()); }
        void erase(uint32_t at) { view->erase(at); sizes->push_back(view->size()); }
        void replace(uint32_t at) { view->replace(at); }
        void reset(uint32_t size) { view->reset(size); }
    } sink{&view, &sizes};
    list.on_change([&sink](list_change const& change) noexcept { list_mirror{sink}(change); });

    list.insert_range(0, std::vector<int>{10, 11, 12});
    list.erase(2, 3);

    EXPECT_EQ(sizes, (std::vector<uint32_t>{4, 5, 6, 5, 4, 3}));
    EXPECT_EQ(view.items, items_of(list));
}

// Above the threshold a run is cheaper as one reset than as an item at a time.
TEST(list_mirror, a_long_run_is_one_reset) {
    observable_list<int> list{{1, 2}};
    mirrored m{list, 2};

    list.insert_range(1, std::vector<int>{7, 8});
    list.insert_range(0, std::vector<int>{4, 5, 6});
    list.erase(0, 3);

    EXPECT_EQ(m.view.items, items_of(list));
    EXPECT_EQ(m.view.calls, (std::vector<std::string>{"insert 1", "insert 2", "reset 7", "reset 4"}));
}

}  // namespace
