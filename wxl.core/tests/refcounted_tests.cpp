#include <gtest/gtest.h>

#include <crtdbg.h>


import std;
import wxl.core;


using namespace wxl::core;

namespace {

template <typename T>
class test_object : public T
{
public:
    test_object(bool * const desctructor_flag)
        : desctructor_flag_(desctructor_flag)
    {}

protected:
    ~test_object() override {
        *desctructor_flag_ = true;
    };

private:
    bool * const desctructor_flag_;
};

typedef wxl::core::intrusive_ptr<test_object<refcounted>> test_object_st_ptr;
typedef wxl::core::intrusive_ptr<test_object<refcounted_mt>> test_object_mt_ptr;
}

TEST(RefcountedTest, refcounted_st_test)
{
    bool desctructor_flag = false;

    {
        // Adopts the implicit first reference (refcounted starts at ref_count() == 1).
        test_object_st_ptr ptr(new test_object<refcounted>(&desctructor_flag), false);
        test_object_st_ptr ptr1(ptr.get());
        test_object_st_ptr ptr2(ptr.get());

        EXPECT_TRUE(!desctructor_flag);
    }

    EXPECT_TRUE(desctructor_flag);
}

TEST(RefcountedTest, refcounted_mt_test)
{
    bool desctructor_flag = false;

    {
        // Adopts the implicit first reference (refcounted_mt starts at ref_count() == 1).
        test_object_mt_ptr ptr(new test_object<refcounted_mt>(&desctructor_flag), false);
        test_object_mt_ptr ptr1(ptr.get());
        test_object_mt_ptr ptr2(ptr.get());

        // Takes references from another thread while this one does the same.
        std::jthread other([obj = ptr.get()] {
            test_object_mt_ptr ptr1(obj);
            test_object_mt_ptr ptr2(obj);
        });

        test_object_mt_ptr ptr3(ptr.get());
        test_object_mt_ptr ptr4(ptr.get());

        other.join();

        EXPECT_TRUE(!desctructor_flag);
    }

    EXPECT_TRUE(desctructor_flag);
}

// release_only_ptr: the field-shaped smart pointer that takes one reference and gives it
// back, and can do nothing else -- no copy, no move, no second reference.
TEST(ReleaseOnlyPtrTest, TakesTheReferenceItIsGivenAndReleasesIt)
{
    bool destroyed = false;
    {
        const release_only_ptr<test_object<refcounted>> owner(new test_object<refcounted>(&destroyed));
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}

TEST(ReleaseOnlyPtrTest, AKeeperOutlivesTheOwnerWithoutTheOwnerKnowing)
{
    bool destroyed = false;
    intrusive_ptr<test_object<refcounted>> keeper;
    {
        const release_only_ptr<test_object<refcounted>> owner(new test_object<refcounted>(&destroyed));
        keeper = intrusive_ptr<test_object<refcounted>>(owner.get());
    }
    EXPECT_FALSE(destroyed);
    keeper = intrusive_ptr<test_object<refcounted>>();
    EXPECT_TRUE(destroyed);
}

// The not_null specialization over it: the same ownership, with the null ruled out by
// the type. This is how component holds its state.
TEST(ReleaseOnlyPtrTest, NotNullOverItOwnsTheSameWay)
{
    bool destroyed = false;
    {
        const not_null<release_only_ptr<test_object<refcounted>>> owner(
            new test_object<refcounted>(&destroyed));
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}


// make_refcounted: the count mixed into the type itself, so what comes back
// points at a T and behaves like one.

namespace {

struct counted_value
{
    counted_value() = default;

    explicit counted_value(int value, bool* destroyed = nullptr)
        : value(value), destroyed(destroyed) {}

    // A copy carries the value and not the flag: the flag belongs to the
    // object that was made with it, so that a copy going away cannot be
    // mistaken for the original going away.
    counted_value(const counted_value& other) : value(other.value) {}

    counted_value& operator=(const counted_value& other) {
        value = other.value;
        return *this;
    }

    ~counted_value() {
        if (destroyed != nullptr)
            *destroyed = true;
    }

    int value = 0;
    bool* destroyed = nullptr;
};

int read(const counted_value& value) { return value.value; }

}  // namespace

TEST(MakeRefcountedTest, PointsAtTheTypeItself)
{
    const auto held = make_refcounted<counted_value>(7);

    static_assert(std::derived_from<std::remove_reference_t<decltype(*held)>, counted_value>);

    EXPECT_EQ(7, held->value);
    EXPECT_EQ(7, (*held).value);
    EXPECT_EQ(7, read(*held));  // binds to a counted_value const& as it stands
}

TEST(MakeRefcountedTest, DefaultConstructsWhenNothingIsPassed)
{
    const auto token = make_refcounted<counted_value>();
    EXPECT_EQ(0, token->value);
}

// The form this was written for: the value is assigned through the pointer
// after the fact, which is what a subscription token needs.
TEST(MakeRefcountedTest, TheValueIsAssignedThroughTheStar)
{
    const auto token = make_refcounted<counted_value>();

    *token = counted_value{42};
    EXPECT_EQ(42, token->value);
}

TEST(MakeRefcountedTest, EveryHolderSeesTheSameValue)
{
    const auto writer = make_refcounted<counted_value>(1);
    const auto reader = writer;

    writer->value = 5;
    EXPECT_EQ(5, reader->value);
    EXPECT_EQ(writer.get(), reader.get());
}

TEST(MakeRefcountedTest, TheValueGoesWithTheLastHolder)
{
    bool destroyed = false;
    {
        const auto owner = make_refcounted<counted_value>(1, &destroyed);
        {
            const auto second = owner;
            EXPECT_FALSE(destroyed);
        }
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}

#ifdef _DEBUG

// And it comes from the pool, like everything else wxl allocates per
// subscription. The probe is the debug CRT heap, as in function_tests.
TEST(MakeRefcountedTest, ComesFromTheStaPool)
{
    _CrtMemState before{}, after{}, difference{};

    // The size class is warmed first: the pool takes a page from the ordinary
    // heap when it has no room left, and that is the page's cost, not the
    // object's.
    const auto warm = make_refcounted<std::array<char, 64>>();

    _CrtMemCheckpoint(&before);
    const auto held = make_refcounted<std::array<char, 64>>();
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(0, _CrtMemDifference(&difference, &before, &after))
        << "the object came from the CRT heap rather than from sta_memory_pool";
}

#endif

// Copying copies the value into a fresh object with a count of its own -- the
// count belongs to the object, not to the value.
TEST(MakeRefcountedTest, ACopyIsTheValueInANewObjectWithItsOwnCount)
{
    bool first_gone = false;
    const auto first = make_refcounted<counted_value>(3, &first_gone);

    {
        const auto second = make_refcounted<counted_value>(*first);
        EXPECT_EQ(3, second->value);
        EXPECT_NE(first.get(), second.get());

        second->value = 4;
        EXPECT_EQ(3, first->value);

        // And it does not matter whether the source is const.
        const counted_value& source = *first;
        const auto third = make_refcounted<counted_value>(source);
        EXPECT_EQ(3, third->value);
    }

    EXPECT_FALSE(first_gone);
}

TEST(MakeRefcountedTest, AssigningOneToAnotherMovesTheValueAndNotTheCount)
{
    bool left_gone = false;
    bool right_gone = false;

    const auto left = make_refcounted<counted_value>(1, &left_gone);
    const auto right = make_refcounted<counted_value>(2, &right_gone);
    const auto keeper = right;

    *left = *right;

    EXPECT_EQ(2, left->value);
    EXPECT_FALSE(left_gone);
    EXPECT_FALSE(right_gone);
}
