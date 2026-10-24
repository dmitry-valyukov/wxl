#include <gtest/gtest.h>

#include <crtdbg.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

struct Thing {
    int value = 7;
};

struct Derived : Thing {};

class Counted : public refcounted
{
public:
    using refcounted::ref_count;

    int value = 3;

protected:
    ~Counted() override = default;
};

TEST(NotNullTest, hands_back_what_it_was_given) {
    Thing thing;
    const not_null<Thing> p = as_not_null(&thing);

    EXPECT_EQ(p.get(), &thing);
    EXPECT_EQ(p.get(), &thing);
    EXPECT_EQ(p->value, 7);
    EXPECT_EQ(&*p, &thing);
}

// The reason as_not_null() exists: the type comes from the pointer instead of
// being written out at the call.
TEST(NotNullTest, as_not_null_deduces_the_type) {
    Thing thing;

    static_assert(std::is_same_v<decltype(as_not_null(&thing)), not_null<Thing>>);
    EXPECT_EQ(as_not_null(&thing).get(), &thing);
}

// Converting where the pointers convert: a not_null<Derived> is a
// not_null<Thing>, and stays non-null on the way.
TEST(NotNullTest, converts_where_the_pointers_do) {
    Derived derived;
    const not_null<Thing> p = as_not_null(&derived);

    EXPECT_EQ(p.get(), static_cast<Thing*>(&derived));
}

// What the runtime's component map is keyed on, so both halves of the ordering
// have to be there -- operator!=, <, <= and > are synthesized from these two.
TEST(NotNullTest, compares_by_address) {
    Thing first;
    Thing second;

    const not_null<Thing> a = as_not_null(&first);
    const not_null<Thing> b = as_not_null(&second);

    EXPECT_EQ(a, as_not_null(&first));
    EXPECT_NE(a, b);
    EXPECT_EQ(a < b, &first < &second);

    std::map<not_null<Thing>, int> by_pointer;

    by_pointer.emplace(a, 1);
    by_pointer.emplace(b, 2);

    EXPECT_EQ(by_pointer.at(a), 1);
    EXPECT_EQ(by_pointer.at(b), 2);
}

// Over `const void`: an address and nothing else, so there is no operator* to dereference
// it with.
TEST(NotNullTest, an_opaque_handle_over_const_void) {
    Thing thing;
    const not_null<const void> handle{&thing};

    EXPECT_EQ(handle.get(), static_cast<const void*>(&thing));
    EXPECT_EQ(handle, not_null<const void>{&thing});
}

// The intrusive_ptr specialization adopts the reference the object is born
// with rather than adding a second one.
TEST(NotNullTest, over_an_intrusive_ptr_it_adopts_the_first_reference) {
    const not_null<intrusive_ptr<Counted>> p{new Counted};

    EXPECT_EQ(p->value, 3);
    EXPECT_EQ(p->ref_count(), 1u);

    const intrusive_ptr<Counted> shared = p;

    EXPECT_EQ(p->ref_count(), 2u);
    EXPECT_EQ(shared.get(), p.get().get());
}

// A null pointer here is a bug in the caller, not input to be reported back,
// so it takes the process down in every build. The report goes to stderr
// rather than to a dialog, which in a test run would be a hang.
void die_on_null() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    Thing* nothing = nullptr;

    (void)as_not_null(nothing);
}

TEST(NotNullDeathTest, a_null_pointer_takes_the_process_down) {
    EXPECT_DEATH(die_on_null(), "");
}

}  // namespace
