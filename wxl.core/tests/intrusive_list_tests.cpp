#include <gtest/gtest.h>

import std;
import wxl.core;

namespace {

using wxl::core::as_not_null;
using wxl::core::intrusive_list;
using wxl::core::intrusive_list_node;

struct Item : intrusive_list_node<Item> {
    int value;
    explicit Item(int v) : value(v) {}
};

std::vector<int> values(const intrusive_list<Item>& list) {
    std::vector<int> result;
    for (const Item& item : list)
        result.push_back(item.value);
    return result;
}

std::vector<int> backwards(intrusive_list<Item>& list) {
    std::vector<int> result;
    for (auto it = list.end(); it != list.begin();) {
        --it;
        result.push_back(it->value);
    }
    return result;
}

TEST(IntrusiveListTest, DefaultConstructedListIsEmpty) {
    intrusive_list<Item> list;

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.front(), nullptr);
    EXPECT_EQ(list.back(), nullptr);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(IntrusiveListTest, ANodeKnowsWhetherItIsInAList) {
    intrusive_list<Item> list;
    Item a(1);

    EXPECT_FALSE(a.linked());

    list.push_back(as_not_null(&a));
    EXPECT_TRUE(a.linked());

    intrusive_list<Item>::remove(as_not_null(&a));
    EXPECT_FALSE(a.linked());
}

TEST(IntrusiveListTest, PushFrontAddsAtHead) {
    intrusive_list<Item> list;
    Item a(1), b(2);

    list.push_front(as_not_null(&a));
    list.push_front(as_not_null(&b));

    EXPECT_EQ(values(list), (std::vector{2, 1}));
    EXPECT_EQ(list.front()->value, 2);
    EXPECT_EQ(list.back()->value, 1);
}

TEST(IntrusiveListTest, PushBackAddsAtTail) {
    intrusive_list<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    EXPECT_EQ(values(list), (std::vector{1, 2, 3}));
}

TEST(IntrusiveListTest, WalksBothWays) {
    intrusive_list<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    EXPECT_EQ(backwards(list), (std::vector{3, 2, 1}));
}

// The reason this list exists beside intrusive_slist: the node is its own
// handle, so nothing has to be kept from the call that linked it, and taking
// it out asks the list nothing.
TEST(IntrusiveListTest, ANodeLeavesWithoutTheListBeingConsulted) {
    intrusive_list<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    intrusive_list<Item>::remove(as_not_null(&b));

    EXPECT_EQ(values(list), (std::vector{1, 3}));
    EXPECT_EQ(backwards(list), (std::vector{3, 1}));
}

TEST(IntrusiveListTest, RemovingTheOnlyNodeEmptiesTheList) {
    intrusive_list<Item> list;
    Item a(1);

    list.push_back(as_not_null(&a));
    intrusive_list<Item>::remove(as_not_null(&a));

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.front(), nullptr);
    EXPECT_EQ(list.back(), nullptr);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(IntrusiveListTest, RemovingAnUnlinkedNodeDoesNothing) {
    intrusive_list<Item> list;
    Item a(1), loose(2);

    list.push_back(as_not_null(&a));
    intrusive_list<Item>::remove(as_not_null(&loose));

    EXPECT_EQ(values(list), (std::vector{1}));
    EXPECT_FALSE(loose.linked());
}

TEST(IntrusiveListTest, ARemovedNodeCanBeLinkedAgain) {
    intrusive_list<Item> list;
    Item a(1), b(2);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    intrusive_list<Item>::remove(as_not_null(&a));
    list.push_back(as_not_null(&a));

    EXPECT_EQ(values(list), (std::vector{2, 1}));
}

// What the shutdown walk does: it ends each waiting coroutine, and ending one
// takes its node out of the very list being walked. So the step to the next
// node has to be taken before the current one goes.
TEST(IntrusiveListTest, EachNodeCanLeaveWhileTheListIsWalked) {
    intrusive_list<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    std::vector<int> seen;
    for (auto it = list.begin(); it != list.end();) {
        Item& item = *it++;
        seen.push_back(item.value);
        intrusive_list<Item>::remove(as_not_null(&item));
    }

    EXPECT_EQ(seen, (std::vector{1, 2, 3}));
    EXPECT_TRUE(list.empty());
    EXPECT_FALSE(a.linked());
    EXPECT_FALSE(b.linked());
    EXPECT_FALSE(c.linked());
}

TEST(IntrusiveListTest, ClearUnlinksEveryNode) {
    intrusive_list<Item> list;
    Item a(1), b(2);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.clear();

    EXPECT_TRUE(list.empty());
    EXPECT_FALSE(a.linked());
    EXPECT_FALSE(b.linked());
}

TEST(IntrusiveListTest, NodesMoveBetweenListsWithoutACookie) {
    intrusive_list<Item> from, to;
    Item a(1), b(2);

    from.push_back(as_not_null(&a));
    from.push_back(as_not_null(&b));

    intrusive_list<Item>::remove(as_not_null(&a));
    to.push_back(as_not_null(&a));

    EXPECT_EQ(values(from), (std::vector{2}));
    EXPECT_EQ(values(to), (std::vector{1}));
}

}  // namespace
