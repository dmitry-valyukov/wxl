#include <crtdbg.h>
#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;
using wxl::core::intrusive_ptr;

namespace {

using Log = std::vector<std::string>;

// What the failure handler saw, in order: the handler is a bare function pointer and
// has nowhere of its own to put it.
std::vector<std::exception_ptr> reported;

std::string what_of(std::exception_ptr error) {
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "?";
    }
}

class ScenarioOwnerTest : public ::testing::Test
{
protected:
    void SetUp() override {
        reported.clear();
        previous_ = on_detached_task_failure();
        on_detached_task_failure() = [](std::exception_ptr error) noexcept {
            reported.push_back(error);
        };
    }

    void TearDown() override { on_detached_task_failure() = previous_; }

private:
    detached_task_failure_handler previous_ = nullptr;
};

/// An owner made on the heap, as an application makes one, with what it saw of its
/// own life kept outside it.
class Owner : public scenario_owner
{
public:
    explicit Owner(int& deaths) noexcept : deaths_(deaths) {}
    ~Owner() override { ++deaths_; }

    using scenario_owner::ref_count;

private:
    int& deaths_;
};

struct Scene {
    int deaths = 0;
    Log log;
    intrusive_ptr<Owner> owner{new Owner(deaths), false};
};

/// Writes its name into the log as it goes: a local, a parameter copy, whatever a
/// frame holds. A moved-from one is nobody.
struct Witness {
    Log* log;
    std::string name;

    Witness(Log& to, std::string what) : log(&to), name(std::move(what)) {}
    Witness(Witness&& other) noexcept
        : log(std::exchange(other.log, nullptr)), name(std::move(other.name)) {}
    Witness& operator=(Witness&&) = delete;

    ~Witness() {
        if (log) log->push_back(name);
    }
};

/// The shape of every real awaitable in wxl: resumed by hand here, and ending with
/// whatever the test resumes it with -- nothing, a failure or a cancellation.
struct ThrowingCapture {
    std::coroutine_handle<>* out;
    std::exception_ptr error;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const {
        if (error) std::rethrow_exception(error);
    }
};

/// Hands the frame's own handle out without suspending, so a test can destroy a
/// frame that later stands in a wait it cannot reach otherwise.
struct OwnHandle {
    std::coroutine_handle<>* out;

    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> self) const noexcept {
        *out = self;
        return false;
    }
    void await_resume() const noexcept {}
};

// Scenarios: the owner first.

detached_task scenario(Owner&, std::coroutine_handle<>& out, Log& log, std::string name,
                       std::exception_ptr error = {}) {
    const Witness local{log, name + ": local"};
    co_await ThrowingCapture{&out, error};
    log.push_back(name + ": ends");
}

detached_task scenario_with_a_parameter(Owner&, Witness, std::coroutine_handle<>& out, Log& log) {
    const Witness local{log, "S: local"};
    co_await ThrowingCapture{&out};
    log.push_back("S: ends");
}

detached_task scenario_by_pointer(Owner*, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
}

detached_task scenario_at_once(Owner&, Log& log) {
    log.push_back("at once");
    co_return;
}

/// Starts another scenario of its owner before it ends.
detached_task starts_another(Owner& owner, std::coroutine_handle<>& out,
                             std::coroutine_handle<>& next, Log& log) {
    co_await ThrowingCapture{&out};
    scenario(owner, next, log, "S2");
    log.push_back("S1: ends");
}

/// A scenario waiting for the others.
detached_task scenario_waits(Owner& owner, Log& log, std::coroutine_handle<>* self = nullptr) {
    if (self) co_await OwnHandle{self};
    co_await owner.scenarios_ended();
    log.push_back("L: woke with " + std::to_string(owner.scenarios()));
}

// Coroutines that are not scenarios: the owner is not their first argument.

detached_task waits(Log& log, Owner& owner) {
    co_await owner.scenarios_ended();
    log.push_back("W: woke with " + std::to_string(owner.scenarios()));
}

/// Wakes, starts a scenario at once, and waits again.
detached_task waits_twice(Log& log, Owner& owner, std::coroutine_handle<>& next) {
    co_await owner.scenarios_ended();
    log.push_back("W: woke with " + std::to_string(owner.scenarios()));

    scenario(owner, next, log, "S4");
    co_await owner.scenarios_ended();
    log.push_back("W: woke again with " + std::to_string(owner.scenarios()));
}

detached_task waits_then_fails(Log& log, Owner& owner) {
    co_await owner.scenarios_ended();
    log.push_back("W: woke");
    throw std::runtime_error("the waiter failed");
}

task<> task_waits(Owner& owner, Log& log, bool fails = false) {
    co_await owner.scenarios_ended();
    log.push_back("T: woke with " + std::to_string(owner.scenarios()));
    if (fails) throw std::runtime_error("the task failed");
}

detached_task held_as_counted(const wxl::core::refcounted&, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
}

detached_task owner_second(int, Owner&, std::coroutine_handle<>& out) {
    co_await ThrowingCapture{&out};
}

using detached_promise = detached_task::promise_type;

// The owner counts by the type of the first parameter, chosen over the counting base
// it derives from; a promise stays one word.
static_assert(std::is_constructible_v<detached_promise, Owner&, int&>);
static_assert(std::is_constructible_v<detached_promise, Owner*&>);
static_assert(std::is_constructible_v<detached_promise, const scenario_owner&>);
static_assert(sizeof(detached_promise) == sizeof(void*));

// Two bits of the word: the pointer to an owner keeps them clear.
static_assert(alignof(scenario_owner) >= 4);

TEST_F(ScenarioOwnerTest, AnOwnerWithNoScenariosIsAwaitedWithoutSuspending) {
    Scene scene;

    task<> t = task_waits(*scene.owner, scene.log);
    waits(scene.log, *scene.owner);

    EXPECT_TRUE(t.done());
    EXPECT_EQ(scene.log, (Log{"T: woke with 0", "W: woke with 0"}));
}

TEST_F(ScenarioOwnerTest, AScenarioIsCountedFromItsCallUntilItsFrameIsGone) {
    Scene scene;
    std::coroutine_handle<> out, by_pointer;

    scenario(*scene.owner, out, scene.log, "S");
    scenario_by_pointer(scene.owner.get(), by_pointer);

    EXPECT_EQ(scene.owner->scenarios(), 2u);
    EXPECT_EQ(scene.owner->ref_count(), 3u);

    out.resume();
    by_pointer.resume();

    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
    EXPECT_EQ(scene.log, (Log{"S: ends", "S: local"}));
}

TEST_F(ScenarioOwnerTest, AScenarioThatNeverSuspendsHasLeftByTheTimeTheCallReturns) {
    Scene scene;

    scenario_at_once(*scene.owner, scene.log);

    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

// The owner as the counting base, or in the second place: held or not, as before, and
// never counted.
TEST_F(ScenarioOwnerTest, OnlyAFirstParameterOfTheOwnersTypeCounts) {
    Scene scene;
    std::coroutine_handle<> held, second;

    held_as_counted(*scene.owner, held);
    owner_second(0, *scene.owner, second);

    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 2u);

    task<> t = task_waits(*scene.owner, scene.log);
    EXPECT_TRUE(t.done());

    held.resume();
    second.resume();
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AnOwnerNobodyAwaitsLetsEachFrameGoAsItEnds) {
    Scene scene;
    std::coroutine_handle<> first, second;

    scenario(*scene.owner, first, scene.log, "S1");
    scenario(*scene.owner, second, scene.log, "S2");

    second.resume();
    EXPECT_EQ(scene.log, (Log{"S2: ends", "S2: local"}));
    EXPECT_EQ(scene.owner->scenarios(), 1u);

    first.resume();
    EXPECT_EQ(scene.log, (Log{"S2: ends", "S2: local", "S1: ends", "S1: local"}));
    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, TheWaiterIsResumedWhenTheLastScenarioEnds) {
    Scene scene;
    std::coroutine_handle<> first, second;

    scenario(*scene.owner, first, scene.log, "S1");
    scenario(*scene.owner, second, scene.log, "S2");
    task<> t = task_waits(*scene.owner, scene.log);

    second.resume();
    EXPECT_FALSE(t.done());

    first.resume();
    EXPECT_TRUE(t.done());
    EXPECT_EQ(scene.log, (Log{"S2: ends", "S2: local", "S1: ends", "S1: local", "T: woke with 0"}));
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

// The point of the wake, fixed: the last frame is gone -- its locals, its promise with
// the reference, its parameter copies -- and the waiter has run up to its next
// suspension or its end before whoever resumed the last scenario goes on.
TEST_F(ScenarioOwnerTest, TheWaiterGoesOnOnceTheLastFrameIsGoneAndBeforeTheResumerDoes) {
    Scene scene;
    std::coroutine_handle<> out;

    scenario_with_a_parameter(*scene.owner, Witness(scene.log, "S: parameter"), out, scene.log);
    waits(scene.log, *scene.owner);

    out.resume();
    scene.log.push_back("resumer goes on");

    EXPECT_EQ(scene.log,
              (Log{"S: ends", "S: local", "S: parameter", "W: woke with 0", "resumer goes on"}));
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AScenarioAwaitingTheEndAloneDoesNotSuspend) {
    Scene scene;

    scenario_waits(*scene.owner, scene.log);

    EXPECT_EQ(scene.log, (Log{"L: woke with 1"}));
    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AScenarioAwaitingTheEndWaitsForTheOthersAndNotForItself) {
    Scene scene;
    std::coroutine_handle<> out;

    scenario(*scene.owner, out, scene.log, "S1");
    scenario_waits(*scene.owner, scene.log);

    // The waiting one is not counted while it stands there.
    EXPECT_EQ(scene.owner->scenarios(), 1u);
    EXPECT_TRUE(scene.log.empty());

    out.resume();

    EXPECT_EQ(scene.log, (Log{"S1: ends", "S1: local", "L: woke with 1"}));
    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AScenarioStartedByAScenarioDuringTheWaitIsWaitedForToo) {
    Scene scene;
    std::coroutine_handle<> first, second;

    starts_another(*scene.owner, first, second, scene.log);
    task<> t = task_waits(*scene.owner, scene.log);

    first.resume();
    EXPECT_FALSE(t.done());
    EXPECT_EQ(scene.owner->scenarios(), 1u);

    second.resume();
    EXPECT_TRUE(t.done());
    EXPECT_EQ(scene.log, (Log{"S1: ends", "S2: ends", "S2: local", "T: woke with 0"}));
}

TEST_F(ScenarioOwnerTest, AScenarioStartedFromOutsideDuringTheWaitIsWaitedForToo) {
    Scene scene;
    std::coroutine_handle<> first, third;

    scenario(*scene.owner, first, scene.log, "S1");
    task<> t = task_waits(*scene.owner, scene.log);
    scenario(*scene.owner, third, scene.log, "S3");

    first.resume();
    EXPECT_FALSE(t.done());

    third.resume();
    EXPECT_TRUE(t.done());
}

// Started once the waiter has been resumed -- by the waiter itself, or by whoever
// resumed the last scenario -- a scenario is not waited for: it is counted, and a
// wait made after it waits for it.
TEST_F(ScenarioOwnerTest, AScenarioStartedAfterTheWakeIsNotWaitedFor) {
    Scene scene;
    std::coroutine_handle<> first, fourth, fifth;

    scenario(*scene.owner, first, scene.log, "S1");
    waits_twice(scene.log, *scene.owner, fourth);

    first.resume();
    scenario(*scene.owner, fifth, scene.log, "S5");

    EXPECT_EQ(scene.log, (Log{"S1: ends", "S1: local", "W: woke with 0"}));
    EXPECT_EQ(scene.owner->scenarios(), 2u);

    fourth.resume();
    EXPECT_EQ(scene.log.size(), 5u);

    fifth.resume();
    EXPECT_EQ(scene.log.back(), "W: woke again with 0");
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, ALastScenarioEndingInAFailureOrACancellationEndsTheWaitAllTheSame) {
    Scene scene;
    std::coroutine_handle<> failing, cancelled;

    scenario(*scene.owner, failing, scene.log, "S1",
             std::make_exception_ptr(std::runtime_error("S1 failed")));
    scenario(*scene.owner, cancelled, scene.log, "S2",
             std::make_exception_ptr(operation_canceled_exception{}));
    task<> t = task_waits(*scene.owner, scene.log);

    failing.resume();
    EXPECT_FALSE(t.done());
    cancelled.resume();
    EXPECT_TRUE(t.done());

    EXPECT_EQ(scene.log, (Log{"S1: local", "S2: local", "T: woke with 0"}));
    ASSERT_EQ(reported.size(), 1u);
    EXPECT_EQ(what_of(reported[0]), "S1 failed");
}

// The waiter runs on the thread the last scenario handed it, but its exceptions are
// its own: the scenario's ended in its handler before the hand-over, and the waiter's
// go to the waiter's promise -- nothing comes out of the resume that ended the scenario.
TEST_F(ScenarioOwnerTest, TheWaitersOwnFailureStaysWithTheWaiter) {
    Scene scene;
    std::coroutine_handle<> first, second;

    scenario(*scene.owner, first, scene.log, "S1",
             std::make_exception_ptr(std::runtime_error("S1 failed")));
    task<> t = task_waits(*scene.owner, scene.log, true);

    EXPECT_NO_THROW(first.resume());
    ASSERT_TRUE(t.done());
    EXPECT_THROW(t.result(), std::runtime_error);

    scenario(*scene.owner, second, scene.log, "S2",
             std::make_exception_ptr(std::runtime_error("S2 failed")));
    waits_then_fails(scene.log, *scene.owner);

    EXPECT_NO_THROW(second.resume());

    ASSERT_EQ(reported.size(), 3u);
    EXPECT_EQ(what_of(reported[0]), "S1 failed");
    EXPECT_EQ(what_of(reported[1]), "S2 failed");
    EXPECT_EQ(what_of(reported[2]), "the waiter failed");
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AFrameDestroyedFromOutsideLeavesAsWell) {
    Scene scene;
    std::coroutine_handle<> out;

    scenario(*scene.owner, out, scene.log, "S");
    out.destroy();

    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
    EXPECT_EQ(scene.log, (Log{"S: local"}));
}

// Destroyed from outside, the last scenario has no final point to hand the thread over
// from: the waiter is resumed inside the destruction, once the frame has let go of the
// owner and before its parameter copies go.
TEST_F(ScenarioOwnerTest, ALastFrameDestroyedFromOutsideWakesTheWaiterInsideItsDestruction) {
    Scene scene;
    std::coroutine_handle<> out;

    scenario_with_a_parameter(*scene.owner, Witness(scene.log, "S: parameter"), out, scene.log);
    task<> t = task_waits(*scene.owner, scene.log);

    out.destroy();

    EXPECT_TRUE(t.done());
    EXPECT_EQ(scene.log, (Log{"S: local", "T: woke with 0", "S: parameter"}));
    EXPECT_EQ(scene.owner->ref_count(), 1u);
}

TEST_F(ScenarioOwnerTest, AWaiterDestroyedWhileItWaitsTakesItselfOff) {
    Scene scene;
    std::coroutine_handle<> out;

    scenario(*scene.owner, out, scene.log, "S1");
    {
        task<> dropped = task_waits(*scene.owner, scene.log);
        EXPECT_FALSE(dropped.done());
    }

    // Nobody waits now: the scenario ends as any detached frame does, and another
    // coroutine may wait.
    task<> t = task_waits(*scene.owner, scene.log);
    out.resume();

    EXPECT_TRUE(t.done());
    EXPECT_EQ(scene.log, (Log{"S1: ends", "S1: local", "T: woke with 0"}));
}

TEST_F(ScenarioOwnerTest, AScenarioDestroyedWhileItWaitsCountsAgainToLeave) {
    Scene scene;
    std::coroutine_handle<> out, waiting;

    scenario(*scene.owner, out, scene.log, "S1");
    scenario_waits(*scene.owner, scene.log, &waiting);
    ASSERT_TRUE(waiting);
    EXPECT_EQ(scene.owner->scenarios(), 1u);

    waiting.destroy();
    EXPECT_EQ(scene.owner->scenarios(), 1u);
    EXPECT_EQ(scene.owner->ref_count(), 2u);

    out.resume();
    EXPECT_EQ(scene.owner->scenarios(), 0u);
    EXPECT_EQ(scene.owner->ref_count(), 1u);
    EXPECT_EQ(scene.log, (Log{"S1: ends", "S1: local"}));
}

task<> waits_and_forgets(Owner& owner, Log& log) {
    co_await owner.scenarios_ended();
    log.push_back("T: woke");
}

// The owner held by its scenarios alone: the frame destroyed in the waiter's co_await
// is what lets it go, and the wait touches nothing of it afterwards. Neither may the
// waiter, which did not hold it.
TEST_F(ScenarioOwnerTest, TheLastFrameMayTakeTheOwnerWithIt) {
    int deaths = 0;
    Log log;
    std::coroutine_handle<> out;
    std::optional<task<>> t;
    {
        intrusive_ptr<Owner> owner{new Owner(deaths), false};
        scenario(*owner, out, log, "S");
        t.emplace(waits_and_forgets(*owner, log));
    }

    EXPECT_EQ(deaths, 0);
    out.resume();

    EXPECT_EQ(deaths, 1);
    EXPECT_TRUE(t->done());
    EXPECT_EQ(log, (Log{"S: ends", "S: local", "T: woke"}));
}

// Member coroutines: the object is the first argument. GCC 13 does not pass it to the
// promise's constructor, so these hold on the compilers wxl is built with.

/// An application's exit, in the shape the owner is for: asked to close, it cancels
/// what reads, waits for its scenarios, writes what is left -- scenarios too -- waits
/// again, and only then says it is done.
class Exiting : public scenario_owner
{
public:
    explicit Exiting(Log& log) noexcept : log_(log) {}

    using scenario_owner::ref_count;

    /// Ends with the cancellation once the token is cancelled, as any wait under it.
    detached_task reads(std::coroutine_handle<>& out) {
        co_await cancellable(ThrowingCapture{&out}, stop_.token());
        log_.push_back("read");
    }

    detached_task writes(std::coroutine_handle<>& out) const {
        co_await ThrowingCapture{&out};
        log_.push_back("written");
    }

    detached_task leave(std::coroutine_handle<>& write) {
        stop_.cancel();
        co_await scenarios_ended();
        log_.push_back("reads ended, " + std::to_string(scenarios()) + " running");

        writes(write);
        co_await scenarios_ended();
        log_.push_back("writes ended");
        finished_ = true;
    }

    bool finished() const noexcept { return finished_; }

private:
    Log& log_;
    cancellation_source stop_;
    bool finished_ = false;
};

class ScenarioOwnerMemberTest : public ScenarioOwnerTest
{
};

TEST_F(ScenarioOwnerMemberTest, AMemberCoroutineIsAScenarioOfItsObject) {
    Log log;
    intrusive_ptr<Exiting> app{new Exiting(log), false};
    std::coroutine_handle<> read, write;

    app->reads(read);
    app->writes(write);
    EXPECT_EQ(app->scenarios(), 2u);
    EXPECT_EQ(app->ref_count(), 3u);

    read.resume();
    write.resume();
    EXPECT_EQ(app->scenarios(), 0u);
    EXPECT_EQ(app->ref_count(), 1u);
}

TEST_F(ScenarioOwnerMemberTest, AnExitWaitsForWhatItCancelledAndForWhatItWrote) {
    Log log;
    intrusive_ptr<Exiting> app{new Exiting(log), false};
    std::coroutine_handle<> read, write;

    app->reads(read);
    app->leave(write);
    EXPECT_FALSE(app->finished());

    // The read ends with the cancellation it was told, as the token tells it.
    read.resume();
    EXPECT_EQ(log, (Log{"reads ended, 1 running"}));
    EXPECT_FALSE(app->finished());
    ASSERT_TRUE(write);

    write.resume();
    EXPECT_TRUE(app->finished());
    EXPECT_EQ(log, (Log{"reads ended, 1 running", "written", "writes ended"}));
    EXPECT_EQ(app->scenarios(), 0u);
    EXPECT_EQ(app->ref_count(), 1u);
    EXPECT_TRUE(reported.empty());
}

}  // namespace

// The checks: a misuse ends the process at the check that names it -- assert in a Debug
// build, core::abort under STRICT_CORO in any build. A build with neither has nothing to
// stop at, and the misuse is undefined behaviour, so it is not run there.

namespace {

void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

/// A coroutine with its frame from the ordinary allocator, for a wait from a thread the
/// pool does not belong to.
struct heap_task {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct promise_type {
        heap_task get_return_object() { return heap_task{handle_t::from_promise(*this)}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };

    ~heap_task() {
        if (handle) handle.destroy();
    }

    handle_t handle;
};

heap_task heap_waits(Owner& owner) { co_await owner.scenarios_ended(); }

void wait_twice() {
    report_failures_to_stderr();

    Scene scene;
    std::coroutine_handle<> out;
    scenario(*scene.owner, out, scene.log, "S");
    task<> one = task_waits(*scene.owner, scene.log);
    task<> two = task_waits(*scene.owner, scene.log);
}

void wait_from_another_thread() {
    report_failures_to_stderr();

    Scene scene;
    std::coroutine_handle<> out;
    scenario(*scene.owner, out, scene.log, "S");
    std::thread([&scene] { heap_task waiter = heap_waits(*scene.owner); }).join();
}

void drop_an_owner_with_a_running_scenario() {
    report_failures_to_stderr();

    int deaths = 0;
    Log log;
    std::coroutine_handle<> out;
    {
        Owner owner(deaths);
        scenario(owner, out, log, "S");
    }
}

/// The place a strict build's report names, as core::abort writes it.
std::string place_of(std::string_view file, std::uint_least32_t line) {
    return std::format("{}\\({}\\): ", file, line);
}

constexpr std::uint_least32_t line_of_the_second_wait = std::source_location::current().line() + 1;
task<> waits_second(Owner& owner) { co_await owner.scenarios_ended(); }

void wait_second() {
    report_failures_to_stderr();

    Scene scene;
    std::coroutine_handle<> out;
    scenario(*scene.owner, out, scene.log, "S");
    task<> one = task_waits(*scene.owner, scene.log);
    task<> two = waits_second(*scene.owner);
}

class ScenarioOwnerDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

class ScenarioOwnerStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(ScenarioOwnerDeathTest, OneCoroutineWaitsAtATime) {
    EXPECT_DEATH(wait_twice(), "second coroutine");
}

TEST_F(ScenarioOwnerDeathTest, TheScenariosAreAwaitedOnTheirOwnThread) {
    EXPECT_DEATH(wait_from_another_thread(), "other than its scenarios'");
}

TEST_F(ScenarioOwnerDeathTest, AnOwnerOutlivesItsScenarios) {
    EXPECT_DEATH(drop_an_owner_with_a_running_scenario(), "destroyed while a scenario of it runs");
}

TEST_F(ScenarioOwnerStrictDeathTest, ASecondWaiterIsNamedByItsCoAwait) {
    EXPECT_DEATH(
        wait_second(),
        place_of("scenario_owner_tests\\.cpp", line_of_the_second_wait) +
            "scenario_owner: awaited by a second coroutine while the first one still waits");
}
