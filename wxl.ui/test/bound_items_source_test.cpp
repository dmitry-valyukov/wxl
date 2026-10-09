// impl::bound_items_source -- the vector a bound list gives its control -- read through the
// projection the way a control reads it: slots, their places, VectorChanged.
//
// What a binding does is done here without a control: a source over an observable_list, its
// watch on the list. Each item's element is an object of its own -- cppwinrt's vector, which
// needs no runtime -- holding the item's value, so a test can tell which item a slot built.

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "impl/bound_items_source.h"

namespace {

using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::CollectionChange;
using winrt::Windows::Foundation::Collections::IObservableVector;
using winrt::Windows::Foundation::Collections::IVector;
using winrt::Windows::Foundation::Collections::IVectorChangedEventArgs;
using wxl::core::observable_list;
using wxl::impl::bound_source;

using change_heard = std::pair<CollectionChange, uint32_t>;

// The element an item is built as: its value, in an object of its own.
wxl::Object element_of(int value) {
    auto const made = winrt::single_threaded_vector<int32_t>({value});
    return wxl::Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(made)));
}

// Which value an element of element_of holds; -1 for no element.
int value_of(IInspectable const& element) {
    if (!element) return -1;
    return element.as<IVector<int32_t>>().GetAt(0);
}

// A list bound as a control would bind it, and what the control would have heard.
struct bound_list {
    explicit bound_list(std::vector<int> items) : list(wxl::core::sta_vector<int>(items.begin(), items.end())) {
        observable_list<int const>& seen = list;
        bound_source source{&seen, list.size(), [&seen](uint32_t at) { return element_of(seen[at]); }};
        vector = source.source().as_interface<IObservableVector<IInspectable>>();
        list.watch_for_binding(std::move(source));
        vector.VectorChanged([this](IObservableVector<IInspectable> const&, IVectorChangedEventArgs const& args) {
            heard.emplace_back(args.CollectionChange(), args.Index());
        });
    }

    int64_t position(IInspectable const& slot) const {
        return wxl::impl::bound_position(static_cast<observable_list<int const> const*>(&list),
                                         wxl::Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(slot))));
    }

    observable_list<int> list;
    IObservableVector<IInspectable> vector{nullptr};
    std::vector<change_heard> heard;
};

TEST(bound_items_source, reads_the_size_and_a_slot_per_item) {
    bound_list bound{{10, 20, 30}};

    ASSERT_EQ(bound.vector.Size(), 3u);
    IInspectable const slot = bound.vector.GetAt(1);
    EXPECT_EQ(bound.position(slot), 1);
    EXPECT_EQ(value_of(wxl::impl::bound_element(slot)), 20);
    // Asked again, the item is the same object: a control compares items by identity.
    EXPECT_EQ(bound.vector.GetAt(1), slot);
    EXPECT_THROW(bound.vector.GetAt(3), winrt::hresult_out_of_bounds);
}

TEST(bound_items_source, says_what_changed_and_where) {
    bound_list bound{{1, 2, 3}};

    bound.list.insert(1, 15);
    bound.list.erase(0);
    bound.list.replace(1, 99);
    bound.list.assign({7});

    EXPECT_EQ(bound.heard, (std::vector<change_heard>{{CollectionChange::ItemInserted, 1},
                                                      {CollectionChange::ItemRemoved, 0},
                                                      {CollectionChange::ItemChanged, 1},
                                                      {CollectionChange::Reset, 0}}));
    ASSERT_EQ(bound.vector.Size(), 1u);
    EXPECT_EQ(value_of(wxl::impl::bound_element(bound.vector.GetAt(0))), 7);
}

TEST(bound_items_source, a_run_is_told_an_item_at_a_time) {
    bound_list bound{{1, 2}};

    bound.list.insert_range(1, std::vector<int>{5, 6, 7});

    EXPECT_EQ(bound.heard, (std::vector<change_heard>{{CollectionChange::ItemInserted, 1},
                                                      {CollectionChange::ItemInserted, 2},
                                                      {CollectionChange::ItemInserted, 3}}));
    EXPECT_EQ(bound.vector.Size(), 5u);
    EXPECT_EQ(value_of(wxl::impl::bound_element(bound.vector.GetAt(3))), 7);
}

TEST(bound_items_source, a_long_run_is_one_reset) {
    bound_list bound{{1}};

    std::vector<int> many(wxl::impl::list_reset_threshold + 1, 4);
    bound.list.insert_range(0, many);

    EXPECT_EQ(bound.heard, (std::vector<change_heard>{{CollectionChange::Reset, 0}}));
    EXPECT_EQ(bound.vector.Size(), wxl::impl::list_reset_threshold + 2);
}

// What the slots are for: an item the control holds on to leads to its item wherever the
// item has moved, and to nothing once it is gone.
TEST(bound_items_source, a_slot_follows_its_item) {
    bound_list bound{{10, 20, 30}};
    IInspectable const thirty = bound.vector.GetAt(2);

    bound.list.insert(0, 5);
    uint32_t index = 0;
    ASSERT_TRUE(bound.vector.IndexOf(thirty, index));
    EXPECT_EQ(index, 3u);
    EXPECT_EQ(bound.position(thirty), 3);
    EXPECT_EQ(value_of(wxl::impl::bound_element(thirty)), 30);

    bound.list.erase(1, 2);
    EXPECT_EQ(bound.position(thirty), 1);

    bound.list.erase(1);
    EXPECT_EQ(bound.position(thirty), -1);
    EXPECT_FALSE(bound.vector.IndexOf(thirty, index));
    EXPECT_EQ(value_of(wxl::impl::bound_element(thirty)), -1);
}

// A replaced item is another object at that place, so the control shows it anew.
TEST(bound_items_source, a_replaced_item_is_a_new_slot) {
    bound_list bound{{1, 2}};
    IInspectable const before = bound.vector.GetAt(1);

    bound.list.replace(1, 3);

    IInspectable const after = bound.vector.GetAt(1);
    EXPECT_NE(after, before);
    EXPECT_EQ(bound.position(before), -1);
    EXPECT_EQ(bound.position(after), 1);
}

// A slot is an item of its own list only: boundItem on another list finds nothing.
TEST(bound_items_source, a_slot_belongs_to_one_list) {
    bound_list one{{1}};
    bound_list other{{1}};

    IInspectable const slot = one.vector.GetAt(0);
    EXPECT_EQ(one.position(slot), 0);
    EXPECT_EQ(other.position(slot), -1);
    uint32_t index = 0;
    EXPECT_FALSE(other.vector.IndexOf(slot, index));
}

TEST(bound_items_source, is_walked_in_order) {
    bound_list bound{{1, 2, 3}};

    std::vector<int> walked;
    for (IInspectable const& slot : bound.vector) {
        walked.push_back(value_of(wxl::impl::bound_element(slot)));
    }
    EXPECT_EQ(walked, (std::vector<int>{1, 2, 3}));

    std::vector<IInspectable> taken(5);
    EXPECT_EQ(bound.vector.GetMany(1, taken), 2u);
    EXPECT_EQ(taken[0], bound.vector.GetAt(1));
    EXPECT_EQ(bound.vector.GetView().Size(), 3u);
}

// The list is written by its model: the control cannot change it.
TEST(bound_items_source, refuses_to_be_changed) {
    bound_list bound{{1}};

    EXPECT_THROW(bound.vector.Append(nullptr), winrt::hresult_not_implemented);
    EXPECT_THROW(bound.vector.RemoveAt(0), winrt::hresult_not_implemented);
    EXPECT_THROW(bound.vector.Clear(), winrt::hresult_not_implemented);
    EXPECT_EQ(bound.vector.Size(), 1u);
}

// The watch gone -- unbind(), or the list itself -- the source forgets the list: what a
// control still holds leads nowhere and builds nothing, and the control is told nothing.
TEST(bound_items_source, is_disarmed_with_its_watch) {
    bound_list bound{{1, 2}};
    IInspectable const slot = bound.vector.GetAt(0);

    bound.list.unbind();
    bound.list.push_back(3);

    EXPECT_EQ(bound.position(slot), -1);
    EXPECT_EQ(value_of(wxl::impl::bound_element(slot)), -1);
    EXPECT_EQ(bound.vector.Size(), 2u);
    EXPECT_TRUE(bound.heard.empty());
}

// A handler removed is called no more, and the source holds none of them past that.
TEST(bound_items_source, lets_a_handler_go) {
    bound_list bound{{1}};
    int calls = 0;
    auto const token = bound.vector.VectorChanged([&calls](auto const&, auto const&) { ++calls; });

    bound.list.push_back(2);
    bound.vector.VectorChanged(token);
    bound.list.push_back(3);

    EXPECT_EQ(calls, 1);
}

// The count is the object's own: the last reference frees it, and a weak reference -- which
// XAML and the projection both take -- says so.
TEST(bound_items_source, goes_with_its_last_reference) {
    winrt::weak_ref<IObservableVector<IInspectable>> weak;
    winrt::weak_ref<IInspectable> weak_slot;
    IInspectable slot;
    {
        bound_list bound{{1}};
        weak = winrt::make_weak(bound.vector);
        slot = bound.vector.GetAt(0);
        weak_slot = winrt::make_weak(slot);
        EXPECT_EQ(weak.get(), bound.vector);
    }
    EXPECT_FALSE(weak.get());
    // The slot outlives its source and leads nowhere.
    EXPECT_EQ(weak_slot.get(), slot);
    EXPECT_EQ(value_of(wxl::impl::bound_element(slot)), -1);
}

}  // namespace
