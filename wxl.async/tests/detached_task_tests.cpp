#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;
using wxl::core::intrusive_ptr;
using wxl::core::refcounted_mt;
using wxl::core::sta_refcounted;

namespace {

// What the failure handler saw, since it is a bare function pointer and has
// nowhere of its own to put it.
std::exception_ptr reported;

// Puts the handler back on the way out, so one test cannot decide what the
// next one does with its exceptions.
class DetachedTaskTest : public ::testing::Test
{
protected:
    void SetUp() override {
        reported = {};
        previous_ = on_detached_task_failure();
        on_detached_task_failure() = [](std::exception_ptr error) noexcept { reported = error; };
    }

    void TearDown() override { on_detached_task_failure() = previous_; }

private:
    detached_task_failure_handler previous_ = nullptr;
};

// Something whose destruction can be seen from outside: what a coroutine
// holds has to be let go of, whichever way the coroutine ends.
struct Trace {
    bool* released;

    explicit Trace(bool* flag) noexcept : released(flag) {}
    Trace(const Trace&) = delete;
    ~Trace() { *released = true; }
};

detached_task runs_through(int& sink) {
    sink = 1;
    co_return;
}

detached_task waits_then_finishes(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};

    struct capture {
        std::coroutine_handle<>* out;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
        void await_resume() const noexcept {}
    };

    co_await capture{&out};
}

detached_task cancelled_at_once() {
    throw operation_canceled_exception{};
    co_return;
}

detached_task fails_at_once(std::string_view what) {
    throw std::runtime_error(std::string(what));
    co_return;
}

TEST_F(DetachedTaskTest, RunsWhereItIsCalled) {
    int sink = 0;

    runs_through(sink);

    EXPECT_EQ(sink, 1);
    EXPECT_FALSE(reported);
}

// The whole difference from task: nothing came back to hold, and the frame is
// gone by the time the body has finished. What proves it from outside is that
// what the frame held was let go of.
TEST_F(DetachedTaskTest, ReleasesItselfWhenTheBodyEnds) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes(waiter, &released);

    ASSERT_TRUE(waiter);
    EXPECT_FALSE(released);

    waiter.resume();

    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

// Cancellation is how a self-owning coroutine is told that what it waits for is
// never coming, so it is the ordinary end and not a failure.
TEST_F(DetachedTaskTest, CancellationIsNotAFailure) {
    cancelled_at_once();

    EXPECT_FALSE(reported);
}

// Anything else has nowhere to go -- there is no caller left -- so it goes to
// the handler the application named.
TEST_F(DetachedTaskTest, EverythingElseGoesToTheHandler) {
    fails_at_once("no such file");

    ASSERT_TRUE(reported);

    try {
        std::rethrow_exception(reported);
        FAIL() << "the reported exception was empty";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "no such file");
    }
}

TEST_F(DetachedTaskTest, ADestroyedFrameStillLetsGoOfWhatItHeld) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes(waiter, &released);
    ASSERT_TRUE(waiter);

    // The other way a suspended coroutine can end: not resumed but taken
    // apart. Its locals are destroyed all the same, which is what makes RAII
    // in a coroutine mean anything.
    waiter.destroy();

    EXPECT_TRUE(released);
}

// The same two checks, through an awaiter whose await_resume() can throw --
// the shape of every real awaitable in wxl, since a result, an error and a
// cancellation all arrive through it. MSVC x64 under /O2 with a synchronous
// exception model skips the locals' destructors of a self-releasing coroutine
// when nothing in its body can throw (bugs/coro-report.md), which is what
// turns the two tests above red in Release; a throwing awaiter is outside
// that condition, so these two say whether production coroutines are covered
// by their awaitables alone.
struct ThrowingCapture {
    std::coroutine_handle<>* out;
    std::exception_ptr error;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const {
        if (error) std::rethrow_exception(error);
    }
};

detached_task waits_then_finishes_throwing(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};

    co_await ThrowingCapture{&out};
}

TEST_F(DetachedTaskTest, ReleasesItselfWhenTheBodyEndsThroughAThrowingAwaiter) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes_throwing(waiter, &released);

    ASSERT_TRUE(waiter);
    EXPECT_FALSE(released);

    waiter.resume();

    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

TEST_F(DetachedTaskTest, ADestroyedFrameLetsGoOfWhatItHeldThroughAThrowingAwaiter) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes_throwing(waiter, &released);
    ASSERT_TRUE(waiter);

    waiter.destroy();

    EXPECT_TRUE(released);
}

// A first argument that counts its own references is held by the frame. Every
// coroutine below waits on ThrowingCapture, the shape of a real awaitable, so
// that the cases mean the same thing in Release as in Debug.

// What a counted object saw of its own life, kept outside it so that it can
// still be read once the object is gone.
struct Notes {
    int deaths = 0;
    int resumed = 0;
};

// An object that counts its own references, with either counter. Its member
// coroutines use `this` after the wait, which is what the frame holds the
// object for.
template <class Counter>
class Counted : public Counter
{
public:
    explicit Counted(Notes& notes) noexcept : notes_(notes) {}

    ~Counted() override { ++notes_.deaths; }

    using Counter::ref_count;

    void touch() const noexcept { ++notes_.resumed; }

    detached_task waits(std::coroutine_handle<>& out, std::exception_ptr error = {}) {
        co_await ThrowingCapture{&out, error};
        touch();
    }

    detached_task waits_const(std::coroutine_handle<>& out) const {
        co_await ThrowingCapture{&out};
        touch();
    }

private:
    Notes& notes_;
};

// One that starts waiting in its constructor, before anybody has adopted the
// reference it was born with.
template <class Counter>
class StartsWaiting : public Counted<Counter>
{
public:
    StartsWaiting(Notes& notes, std::coroutine_handle<>& out) : Counted<Counter>(notes) {
        this->waits(out);
    }
};

// An object that does not count its references. Its member coroutine has the
// object as its first argument, so a counted one passed next is not held.
struct Plain {
    template <class Object>
    detached_task waits_with(Object& object, std::coroutine_handle<>& out) {
        co_await ThrowingCapture{&out};
        object.touch();
    }
};

template <class Object>
detached_task holds_by_reference(Object& object, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
    object.touch();
}

template <class Object>
detached_task holds_by_pointer(Object* object, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
    if (object) object->touch();
}

// The counting base alone, the way a helper takes the object it lives by
// without knowing its type: the base is enough to let go of it, since the
// destructor is virtual.
template <class Counter>
detached_task holds_by_base(const Counter&, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
}

template <class Object>
detached_task holds_the_second(int, Object& object, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
    object.touch();
}

using detached_promise = detached_task::promise_type;

// Which coroutines hold: the first argument and only the first, counted with
// either counter, by reference, const or not, or by pointer; the counting
// base itself counts too.
static_assert(std::is_constructible_v<detached_promise, Counted<sta_refcounted>&>);
static_assert(std::is_constructible_v<detached_promise, const Counted<refcounted_mt>&, int&>);
static_assert(std::is_constructible_v<detached_promise, Counted<sta_refcounted>*&>);
static_assert(std::is_constructible_v<detached_promise, wxl::core::refcounted&>);
static_assert(!std::is_constructible_v<detached_promise, Plain&>);
static_assert(!std::is_constructible_v<detached_promise, Plain&, Counted<sta_refcounted>&>);
static_assert(!std::is_constructible_v<detached_promise, int&, Counted<refcounted_mt>&>);

// The lowest bit of a pointer to either counting base is free to say which
// one it is, and that word is all the promise adds to a frame.
static_assert(alignof(wxl::core::refcounted) > 1 && alignof(refcounted_mt) > 1);
#ifndef WXL_AMBIENT_CANCELLATION
static_assert(sizeof(detached_promise) == sizeof(void*));
#endif

// A counted object made on the heap and held from outside by the one
// reference it was born with, which a test can drop; and the handle of the
// coroutine waiting on it.
template <class Object>
struct Scene {
    Notes notes;
    std::coroutine_handle<> waiter;
    intrusive_ptr<Object> object{new Object(notes), false};
};

// What every holding case comes to: the last reference from outside goes
// while the coroutine waits, the object is still there, and once the wait is
// over it goes with the frame -- once.
template <class Object>
void let_go_then_wake(Scene<Object>& scene) {
    ASSERT_TRUE(scene.waiter);
    EXPECT_EQ(scene.object->ref_count(), 2u);

    scene.object = nullptr;
    EXPECT_EQ(scene.notes.deaths, 0);

    scene.waiter.resume();
    EXPECT_EQ(scene.notes.deaths, 1);
}

template <class Counter>
class DetachedTaskHoldsTest : public DetachedTaskTest
{
};

class counter_name
{
public:
    template <class Counter>
    static std::string GetName(int) {
        if constexpr (std::is_same_v<Counter, refcounted_mt>)
            return "refcounted_mt";
        else
            return "sta_refcounted";
    }
};

using counters = ::testing::Types<sta_refcounted, refcounted_mt>;

TYPED_TEST_SUITE(DetachedTaskHoldsTest, counters, counter_name);

TYPED_TEST(DetachedTaskHoldsTest, AMemberCoroutineKeepsItsObjectWhileItWaits) {
    Scene<Counted<TypeParam>> scene;

    scene.object->waits(scene.waiter);
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 1);
    EXPECT_FALSE(reported);
}

TYPED_TEST(DetachedTaskHoldsTest, AConstMemberCoroutineKeepsItsObjectToo) {
    Scene<Counted<TypeParam>> scene;

    scene.object->waits_const(scene.waiter);
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 1);
}

// Started in the constructor, the frame takes a second reference beside the
// one the object was born with, and whoever adopts that one later finds two.
TYPED_TEST(DetachedTaskHoldsTest, AMemberCoroutineStartedInTheConstructorTakesASecondReference) {
    Notes notes;
    std::coroutine_handle<> waiter;
    intrusive_ptr<StartsWaiting<TypeParam>> object(new StartsWaiting<TypeParam>(notes, waiter),
                                                   false);

    ASSERT_TRUE(waiter);
    EXPECT_EQ(object->ref_count(), 2u);

    object = nullptr;
    EXPECT_EQ(notes.deaths, 0);

    waiter.resume();
    EXPECT_EQ(notes.resumed, 1);
    EXPECT_EQ(notes.deaths, 1);
}

// An object nobody holds by count goes from one to two and back to one, and
// is not deleted by the frame: it stays its owner's.
TYPED_TEST(DetachedTaskHoldsTest, AnObjectOnTheStackIsLeftToItsOwner) {
    Notes notes;
    std::coroutine_handle<> waiter;

    {
        Counted<TypeParam> object(notes);

        object.waits(waiter);
        ASSERT_TRUE(waiter);
        EXPECT_EQ(object.ref_count(), 2u);

        waiter.resume();
        EXPECT_EQ(object.ref_count(), 1u);
        EXPECT_EQ(notes.deaths, 0);
    }

    EXPECT_EQ(notes.deaths, 1);
}

TYPED_TEST(DetachedTaskHoldsTest, AFreeCoroutineKeepsAFirstArgumentByReference) {
    Scene<Counted<TypeParam>> scene;

    holds_by_reference(*scene.object, scene.waiter);
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 1);
}

TYPED_TEST(DetachedTaskHoldsTest, AFreeCoroutineKeepsAFirstArgumentByPointer) {
    Scene<Counted<TypeParam>> scene;

    holds_by_pointer(scene.object.get(), scene.waiter);
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 1);
}

TYPED_TEST(DetachedTaskHoldsTest, AFreeCoroutineKeepsAFirstArgumentSeenAsItsCountingBase) {
    Scene<Counted<TypeParam>> scene;

    holds_by_base<TypeParam>(*scene.object, scene.waiter);
    let_go_then_wake(scene);
}

TYPED_TEST(DetachedTaskHoldsTest, ANullFirstPointerHoldsNothing) {
    std::coroutine_handle<> waiter;

    holds_by_pointer<Counted<TypeParam>>(nullptr, waiter);
    ASSERT_TRUE(waiter);

    waiter.resume();
    EXPECT_FALSE(reported);
}

// A counted object in the second place keeps the one reference it had, both
// after an int and after an object that does not count -- which, for a member
// coroutine, is the first argument. Such coroutines run as they always did.
TYPED_TEST(DetachedTaskHoldsTest, OnlyTheFirstArgumentIsHeld) {
    Scene<Counted<TypeParam>> scene;
    std::coroutine_handle<> second;
    Plain plain;

    holds_the_second(0, *scene.object, scene.waiter);
    plain.waits_with(*scene.object, second);

    ASSERT_TRUE(scene.waiter);
    ASSERT_TRUE(second);
    EXPECT_EQ(scene.object->ref_count(), 1u);

    scene.waiter.resume();
    second.resume();

    EXPECT_EQ(scene.notes.resumed, 2);
    EXPECT_EQ(scene.object->ref_count(), 1u);
    EXPECT_EQ(scene.notes.deaths, 0);
}

// However the body ends, the promise goes with the frame, and the reference
// with it: through an exception for the handler...
TYPED_TEST(DetachedTaskHoldsTest, AFailureAfterTheWaitStillLetsGoOfTheObject) {
    Scene<Counted<TypeParam>> scene;

    scene.object->waits(scene.waiter,
                        std::make_exception_ptr(std::runtime_error("after the wait")));
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 0);
    EXPECT_TRUE(reported);
}

// ...through a cancellation, the ordinary end...
TYPED_TEST(DetachedTaskHoldsTest, ACancelledWaitLetsGoOfTheObject) {
    Scene<Counted<TypeParam>> scene;

    scene.object->waits(scene.waiter, std::make_exception_ptr(operation_canceled_exception{}));
    let_go_then_wake(scene);

    EXPECT_EQ(scene.notes.resumed, 0);
    EXPECT_FALSE(reported);
}

// ...and with the frame taken apart instead of resumed.
TYPED_TEST(DetachedTaskHoldsTest, ADestroyedFrameLetsGoOfTheObject) {
    Scene<Counted<TypeParam>> scene;

    scene.object->waits(scene.waiter);
    ASSERT_TRUE(scene.waiter);

    scene.object = nullptr;
    EXPECT_EQ(scene.notes.deaths, 0);

    scene.waiter.destroy();

    EXPECT_EQ(scene.notes.resumed, 0);
    EXPECT_EQ(scene.notes.deaths, 1);
}

// A detached task awaiting a task: a scenario awaiting the step it hands its work to.
// The task stands on ThrowingCapture, so it ends with whatever the test resumes it
// with -- a value, a failure or a cancellation -- and that travels up through the
// co_await into the detached one, which deals with it the way it deals with its own.

task<int> answers_when_resumed(std::coroutine_handle<>& out, std::exception_ptr error) {
    co_await ThrowingCapture{&out, error};
    co_return 42;
}

task<int> passes_the_answer_on(std::coroutine_handle<>& out, std::exception_ptr error) {
    co_return 1 + co_await answers_when_resumed(out, error);
}

detached_task awaits_the_answer(std::coroutine_handle<>& out, std::exception_ptr error, int& got,
                                bool* released) {
    const Trace trace{released};

    got = co_await answers_when_resumed(out, error);
}

detached_task awaits_the_answer_through_a_chain(std::coroutine_handle<>& out,
                                                std::exception_ptr error, int& got,
                                                bool* released) {
    const Trace trace{released};

    got = co_await passes_the_answer_on(out, error);
}

TEST_F(DetachedTaskTest, TheValueOfATaskReachesTheDetachedTaskAwaitingIt) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    awaits_the_answer(out, {}, got, &released);

    ASSERT_TRUE(out);
    EXPECT_FALSE(released);

    out.resume();

    EXPECT_EQ(got, 42);
    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

TEST_F(DetachedTaskTest, AFailureOfTheTaskItAwaitsGoesToTheHandler) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    awaits_the_answer(out, std::make_exception_ptr(std::runtime_error("the task failed")), got,
                      &released);

    ASSERT_TRUE(out);
    out.resume();

    EXPECT_EQ(got, 0);
    EXPECT_TRUE(released);
    ASSERT_TRUE(reported);

    try {
        std::rethrow_exception(reported);
        FAIL() << "the reported exception was empty";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "the task failed");
    }
}

TEST_F(DetachedTaskTest, ACancellationOfTheTaskItAwaitsIsTheOrdinaryEnd) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    awaits_the_answer(out, std::make_exception_ptr(operation_canceled_exception{}), got,
                      &released);

    ASSERT_TRUE(out);
    out.resume();

    EXPECT_EQ(got, 0);
    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

// The way an application going down ends a chain: the wait at the bottom ends with a
// cancellation, every task above passes it on, and the detached one at the top
// swallows it.
TEST_F(DetachedTaskTest, ACancellationClimbsAChainOfTasksAndEndsQuietly) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    awaits_the_answer_through_a_chain(out, std::make_exception_ptr(operation_canceled_exception{}),
                                      got, &released);

    ASSERT_TRUE(out);
    out.resume();

    EXPECT_EQ(got, 0);
    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

}  // namespace
