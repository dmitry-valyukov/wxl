#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

/// A count that keeps a tally of the links it counts, so that a test sees a link go. Derived
/// from each of the three bases a link is counted by, and counting the same way.
template <class Base>
struct tallied : Base {
    tallied() noexcept { ++alive; }
    ~tallied() override { --alive; }

    static inline int alive = 0;
};

template <class Count>
struct linked_owner : weak_linked<linked_owner<Count>, Count> {
};

template <class Count>
class WeakLinkTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_EQ(tallied<Count>::alive, 0); }
};

using counts = ::testing::Types<sta_refcounted, refcounted, refcounted_mt>;
TYPED_TEST_SUITE(WeakLinkTest, counts);

}  // namespace

TYPED_TEST(WeakLinkTest, gives_the_owner_while_it_lives) {
    using count_t = tallied<TypeParam>;
    linked_owner<count_t> made;

    EXPECT_EQ(made.weak_link()->get(), &made);
    EXPECT_EQ(count_t::alive, 1);
}

// A holder keeps the link, not the owner: once the owner is gone the link answers null and is
// still there for the holder to ask.
TYPED_TEST(WeakLinkTest, outlives_the_owner_and_answers_null) {
    using count_t = tallied<TypeParam>;
    intrusive_ptr<weak_link<linked_owner<count_t>, count_t>> held;
    {
        linked_owner<count_t> made;
        held = made.weak_link();
        EXPECT_EQ(held->get(), &made);
    }

    EXPECT_EQ(held->get(), nullptr);
    EXPECT_EQ(count_t::alive, 1);
}

TYPED_TEST(WeakLinkTest, goes_with_its_last_holder) {
    using count_t = tallied<TypeParam>;
    intrusive_ptr<weak_link<linked_owner<count_t>, count_t>> first;
    {
        linked_owner<count_t> made;
        first = made.weak_link();
    }
    auto second = first;

    first = nullptr;
    EXPECT_EQ(count_t::alive, 1);
    second = nullptr;
    EXPECT_EQ(count_t::alive, 0);
}

// With no holder but the owner, the link goes with the owner.
TYPED_TEST(WeakLinkTest, goes_with_the_owner_when_nobody_holds_it) {
    using count_t = tallied<TypeParam>;
    {
        linked_owner<count_t> made;
        EXPECT_EQ(count_t::alive, 1);
    }
    EXPECT_EQ(count_t::alive, 0);
}

// The aliases name the two counts a link is usually counted by.
static_assert(std::is_same_v<sta_weak_link<int>, weak_link<int, sta_refcounted>>);
static_assert(std::is_same_v<sta_weak_linked<int>, weak_linked<int, sta_refcounted>>);
static_assert(std::is_same_v<weak_link_mt<int>, weak_link<int, refcounted_mt>>);
static_assert(std::is_same_v<weak_linked_mt<int>, weak_linked<int, refcounted_mt>>);

// An owner that counts its own references keeps its link beside its count.
TEST(WeakLinkAliasTest, an_owner_counted_on_its_own_keeps_its_link_beside_its_count) {
    struct counted_owner final : sta_refcounted, sta_weak_linked<counted_owner> {
    };

    intrusive_ptr<sta_weak_link<counted_owner>> held;
    {
        auto const made = make_refcounted<counted_owner>();
        held = made->weak_link();
        EXPECT_EQ(held->get(), made.get());
    }
    EXPECT_EQ(held->get(), nullptr);
}

// The case the atomic count is for: a copy of the link let go of on another thread.
TEST(WeakLinkAliasTest, a_copy_goes_on_another_thread) {
    struct shared_owner : weak_linked_mt<shared_owner> {
    };

    intrusive_ptr<weak_link_mt<shared_owner>> held;
    {
        shared_owner made;
        held = made.weak_link();
        std::jthread([copy = held] { EXPECT_NE(copy->get(), nullptr); }).join();
    }
    EXPECT_EQ(held->get(), nullptr);
}
