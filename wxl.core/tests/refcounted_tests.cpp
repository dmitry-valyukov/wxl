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


// make_refcounted over a type that counts its own references: the type is made as
// it is, and the pointer adopts the reference the object is born with.

namespace {

// Counted by one of the bases, made with arguments, with the destructor protected
// the way a counted type has it, and the count opened up for the tests to read.
template <class Counter>
class self_counted : public Counter
{
public:
    self_counted(int initial, int* destructions) : value(initial), destructions_(destructions) {}

    using Counter::ref_count;

    int value;

protected:
    ~self_counted() override { ++*destructions_; }

private:
    int* const destructions_;
};

// The shape of an application object: final, so nothing can be mixed into it,
// on the pool, not copyable, and counted by one base among others.
class actions
{
public:
    virtual void act() = 0;

protected:
    ~actions() = default;
};

class app_shaped final : public sta_refcounted, private noncopyable, private actions
{
public:
    explicit app_shaped(int* destructions) : destructions_(destructions) {}

    ~app_shaped() override { ++*destructions_; }

    using sta_refcounted::ref_count;

private:
    void act() override {}

    int* const destructions_;
};

// Whether make_refcounted<T> takes these arguments, asked where a failed
// substitution is an answer rather than an error.
template <class T, class... Args>
concept can_make = requires { make_refcounted<T>(std::declval<Args>()...); };

template <class Counter>
class MakeRefcountedCountingItselfTest : public ::testing::Test
{
};

class counter_name
{
public:
    template <class Counter>
    static std::string GetName(int) {
        if constexpr (std::same_as<Counter, sta_refcounted>)
            return "sta_refcounted";
        else if constexpr (std::same_as<Counter, refcounted>)
            return "refcounted";
        else
            return "refcounted_mt";
    }
};

using counters = ::testing::Types<sta_refcounted, refcounted, refcounted_mt>;

}  // namespace

// What a member or a parameter spells is the very type make_refcounted returns:
// for a type that counts itself the plain intrusive_ptr<T>, final or not, and for
// any other a pointer to T with the count and the pool mixed in.
static_assert(std::same_as<decltype(make_refcounted<app_shaped>(nullptr)),
                           refcounted_ptr<app_shaped>>);
static_assert(std::same_as<refcounted_ptr<app_shaped>, intrusive_ptr<app_shaped>>);
static_assert(std::same_as<decltype(make_refcounted<counted_value>()),
                           refcounted_ptr<counted_value>>);
static_assert(std::derived_from<refcounted_ptr<counted_value>::element_type, counted_value>);
static_assert(std::derived_from<refcounted_ptr<counted_value>::element_type, sta_refcounted>);

// The constraint asks for what make_refcounted does, so arguments the constructor
// does not take are turned down for either kind, rather than failing inside.
static_assert(can_make<self_counted<refcounted>, int, int*>);
static_assert(!can_make<self_counted<refcounted>>);
static_assert(!can_make<self_counted<refcounted_mt>, const char*, int*>);
static_assert(can_make<counted_value, int>);
static_assert(!can_make<counted_value, std::nullptr_t>);

TYPED_TEST_SUITE(MakeRefcountedCountingItselfTest, counters, counter_name);

// Made as it is: the pointer is to the type itself, and the destructor, which
// nobody outside can call, is no obstacle -- only the count deletes the object.
TYPED_TEST(MakeRefcountedCountingItselfTest, IsMadeAsItIs)
{
    using made = self_counted<TypeParam>;

    static_assert(std::same_as<decltype(make_refcounted<made>(0, nullptr)), intrusive_ptr<made>>);
    static_assert(std::same_as<refcounted_ptr<made>, intrusive_ptr<made>>);
    static_assert(!std::is_destructible_v<made>);

    int destructions = 0;
    const intrusive_ptr<made> held = make_refcounted<made>(7, &destructions);
    EXPECT_EQ(7, held->value);
}

// The reference it is born with is the pointer's, and none is added: one
// reference, one count, and the object goes once, with the last of them.
TYPED_TEST(MakeRefcountedCountingItselfTest, IsBornWithOneReferenceAndGoesWithTheLast)
{
    int destructions = 0;
    {
        const auto held = make_refcounted<self_counted<TypeParam>>(7, &destructions);
        EXPECT_EQ(1u, held->ref_count());

        {
            const auto second = held;
            EXPECT_EQ(2u, held->ref_count());
            EXPECT_EQ(held.get(), second.get());
        }

        EXPECT_EQ(1u, held->ref_count());
        EXPECT_EQ(0, destructions);
    }
    EXPECT_EQ(1, destructions);
}

TEST(MakeRefcountedTest, AFinalTypeCountingItselfIsMadeToo)
{
    int destructions = 0;
    {
        const auto app = make_refcounted<app_shaped>(&destructions);
        EXPECT_EQ(1u, app->ref_count());
        EXPECT_EQ(0, destructions);
    }
    EXPECT_EQ(1, destructions);
}

// An rvalue arrives as an rvalue -- a move-only argument gets through -- and an
// lvalue as the very object, which the constructor can write to.
TEST(MakeRefcountedTest, TheArgumentsReachTheConstructorAsTheyCame)
{
    class with_payload : public refcounted
    {
    public:
        with_payload(std::unique_ptr<int> given, int& seen) : payload(std::move(given)) {
            seen = *payload;
        }

        std::unique_ptr<int> payload;
    };

    int seen = 0;
    const auto held = make_refcounted<with_payload>(std::make_unique<int>(5), seen);
    EXPECT_EQ(5, *held->payload);
    EXPECT_EQ(5, seen);
}

#ifdef _DEBUG

// An sta_refcounted type brings its operator new along, and make_refcounted goes
// through it: the probe is the debug CRT heap, as above.
TEST(MakeRefcountedTest, AnStaRefcountedTypeComesFromThePool)
{
    int destructions = 0;
    _CrtMemState before{}, after{}, difference{};

    const auto warm = make_refcounted<self_counted<sta_refcounted>>(0, &destructions);

    _CrtMemCheckpoint(&before);
    const auto held = make_refcounted<self_counted<sta_refcounted>>(1, &destructions);
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(0, _CrtMemDifference(&difference, &before, &after))
        << "the object came from the CRT heap rather than from sta_memory_pool";
}

#endif
