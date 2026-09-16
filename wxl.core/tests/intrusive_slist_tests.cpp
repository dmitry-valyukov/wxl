#include <gtest/gtest.h>

import std;
import wxl.core;



namespace {

using wxl::core::as_not_null;
using wxl::core::cookie_t;
using wxl::core::intrusive_slist;
using wxl::core::intrusive_slist_node;

struct Item : intrusive_slist_node<Item> {
    int value;
    explicit Item(int v) : value(v) {}
};

std::vector<int> values(const intrusive_slist<Item>& list) {
    std::vector<int> result;
    for (const Item& item : list)
        result.push_back(item.value);
    return result;
}

TEST(IntrusiveSlistTest, DefaultConstructedListIsEmpty) {
    intrusive_slist<Item> list;

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.front(), nullptr);
    EXPECT_EQ(list.back(), nullptr);
    EXPECT_EQ(list.begin(), list.end());
}

TEST(IntrusiveSlistTest, PushFrontAddsAtHead) {
    intrusive_slist<Item> list;
    Item a(1), b(2);

    list.push_front(as_not_null(&a));
    list.push_front(as_not_null(&b));

    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.front(), &b);
    EXPECT_EQ(list.back(), &a);
    EXPECT_EQ(values(list), (std::vector<int>{2, 1}));
}

TEST(IntrusiveSlistTest, PushBackAddsAtTail) {
    intrusive_slist<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    EXPECT_EQ(list.front(), &a);
    EXPECT_EQ(list.back(), &c);
    EXPECT_EQ(values(list), (std::vector<int>{1, 2, 3}));
}

TEST(IntrusiveSlistTest, MixingPushFrontAndPushBack) {
    intrusive_slist<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    list.push_front(as_not_null(&b));
    list.push_back(as_not_null(&c));

    EXPECT_EQ(values(list), (std::vector<int>{2, 1, 3}));
}

TEST(IntrusiveSlistTest, RemoveUnlinksTheIdentifiedNode) {
    intrusive_slist<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    cookie_t cookie_b = list.push_back(as_not_null(&b));
    list.push_back(as_not_null(&c));

    Item* removed = list.remove(cookie_b);

    EXPECT_EQ(removed, &b);
    EXPECT_EQ(values(list), (std::vector<int>{1, 3}));
}

TEST(IntrusiveSlistTest, RemoveHeadUpdatesFront) {
    intrusive_slist<Item> list;
    Item a(1), b(2);

    cookie_t cookie_a = list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));

    list.remove(cookie_a);

    EXPECT_EQ(list.front(), &b);
}

TEST(IntrusiveSlistTest, RemoveTailUpdatesBack) {
    intrusive_slist<Item> list;
    Item a(1), b(2);

    list.push_back(as_not_null(&a));
    cookie_t cookie_b = list.push_back(as_not_null(&b));

    list.remove(cookie_b);

    EXPECT_EQ(list.back(), &a);
}

TEST(IntrusiveSlistTest, RemoveUnknownCookieReturnsNullptrAndLeavesListIntact) {
    intrusive_slist<Item> list;
    Item a(1), b(2);

    list.push_back(as_not_null(&a));
    cookie_t foreign_cookie{as_not_null(&b)};  // never linked into `list`

    EXPECT_EQ(list.remove(foreign_cookie), nullptr);
    EXPECT_EQ(values(list), (std::vector<int>{1}));
}

// The tail the list keeps has to survive the node holding it being unlinked,
// whether that leaves a shorter list or no list at all.
/// @{
TEST(IntrusiveSlistTest, PushBackAfterTheTailWasRemoved) {
    intrusive_slist<Item> list;
    Item a(1), b(2), c(3);

    list.push_back(as_not_null(&a));
    cookie_t cookie_b = list.push_back(as_not_null(&b));
    list.remove(cookie_b);

    list.push_back(as_not_null(&c));

    EXPECT_EQ(values(list), (std::vector<int>{1, 3}));
    EXPECT_EQ(list.back(), &c);
}

TEST(IntrusiveSlistTest, PushBackAfterTheListWasEmptied) {
    intrusive_slist<Item> list;
    Item a(1), b(2);

    cookie_t cookie_a = list.push_back(as_not_null(&a));
    list.remove(cookie_a);

    list.push_back(as_not_null(&b));

    EXPECT_EQ(list.front(), &b);
    EXPECT_EQ(list.back(), &b);
    EXPECT_EQ(values(list), (std::vector<int>{2}));
}
/// @}

TEST(IntrusiveSlistTest, RemovingTheLastNodeMakesTheListEmptyAgain) {
    intrusive_slist<Item> list;
    Item a(1);

    cookie_t cookie_a = list.push_back(as_not_null(&a));
    list.remove(cookie_a);

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.begin(), list.end());
}

// Swapping exchanges the whole list, tail included: appending to either one afterwards has
// to land at the end of what it now holds, not of what it used to.
/// @{
TEST(IntrusiveSlistTest, SwapExchangesTheContentsOfTwoLists) {
    intrusive_slist<Item> one, two;
    Item a(1), b(2), c(3);

    one.push_back(as_not_null(&a));
    two.push_back(as_not_null(&b));

    one.swap(two);

    EXPECT_EQ(values(one), (std::vector<int>{2}));
    EXPECT_EQ(values(two), (std::vector<int>{1}));

    one.push_back(as_not_null(&c));

    EXPECT_EQ(values(one), (std::vector<int>{2, 3}));
    EXPECT_EQ(one.back(), &c);
}

TEST(IntrusiveSlistTest, SwapWithAnEmptyListMovesEverythingOut) {
    intrusive_slist<Item> list, empty;
    Item a(1), b(2);

    list.push_back(as_not_null(&a));
    list.push_back(as_not_null(&b));

    list.swap(empty);

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.front(), nullptr);
    EXPECT_EQ(list.back(), nullptr);
    EXPECT_EQ(list.begin(), list.end());
    EXPECT_EQ(values(empty), (std::vector<int>{1, 2}));
}
/// @}

}  // namespace
