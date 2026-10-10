// Subroutines of any depth: a coroutine calling a coroutine calling one more, with
// operations of this module among them at every level. Whatever holds for a caller and its
// subroutine holds at each level of the chain, whatever stands there -- a coroutine or an
// operation: the value climbs to the top; an exception thrown at the bottom, by an
// operation on the worker or by the deepest coroutine, climbs through every level; a
// request through a token passed down by hand reaches the operation the chain stands on,
// however deep, and the chain ends by itself; an owner dropping a level in the middle takes
// down what is below it, from the inside out, and what is above runs on; and a rule broken
// at the bottom is reported with its own line. Chains from 3 to 10 levels deep.
//
// The levels alternate how they call. An odd one awaits an operation and then its
// subroutine, as the call returns it; an even one starts its subroutine first, keeps it,
// awaits an operation meanwhile, and joins the subroutine after. The operations rotate
// through the three ways of carrying one out: answered inside the call, on the worker, and
// orphanable.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

constexpr int shallowest = 3;
constexpr int deepest = 10;

/// What stands at the bottom of a chain.
enum class bottom_kind
{
    /// An operation on the worker, answering 1000.
    value,

    /// A read into the bottom frame's buffer, held at a gate on the worker.
    read,

    /// An operation whose body throws on the worker.
    worker_failure,

    /// The deepest coroutine, throwing after an operation has come back.
    coroutine_failure,
};

/// The bottom read's view of the test, kept outside the chain: the read is deleted
/// whenever its turn comes, and the test looks afterwards.
struct probe {
    /// Manual-reset: the read waits here, and whoever opens it opens it for good.
    hevent gate{true};

    /// Set when the worker is inside the read.
    hevent started{true};

    /// Whether the bottom frame, which owns the buffer, is still there.
    std::atomic<bool> frame_alive{false};

    std::atomic<int> made{0};
    std::atomic<int> wrote{0};
    std::atomic<int> wrote_after_the_frame{0};
    std::atomic<int> told{0};

    /// Reads not yet deleted.
    std::atomic<int> alive{0};
};

/// One chain: its shape, and what it saw.
struct chain_state {
    bottom_kind bottom = bottom_kind::value;
    probe p;

    /// The level that drops its subroutine instead of joining it; none if 0.
    int drops_at = 0;

    /// Where the dropping level waits for the test to tell it to drop.
    std::coroutine_handle<> drop_slot;

    /// Frames of the chain alive now: each level's and the bottom's.
    int frames = 0;

    /// Levels that reached their end.
    int ended = 0;
};

/// Lives in a frame of the chain and counts it.
class frame_witness
{
public:
    explicit frame_witness(chain_state& c) : c_(c) { ++c_.frames; }

    ~frame_witness() { --c_.frames; }

private:
    chain_state& c_;
};

/// Lives beside the bottom buffer and says whether it is there. On its way out it opens
/// the gate as well, so that a chain which let the frame go first fails the test rather
/// than leaving the worker asleep for every test after it.
class buffer_witness
{
public:
    explicit buffer_witness(probe& p) : p_(p) { p_.frame_alive = true; }

    ~buffer_witness() {
        p_.frame_alive = false;
        p_.gate.set();
    }

private:
    probe& p_;
};

/// The read at the bottom: waits at the gate, then writes into the buffer in the frame --
/// unless it was asked to stop meanwhile, and then it comes back cut short, with nothing
/// written, as an overlapped read cancelled by CancelIoEx does.
class gated_read : public async_op_t<int>
{
public:
    gated_read(probe& p, std::span<std::byte> into) : p_(p), into_(into) {
        ++p_.made;
        ++p_.alive;
    }

    ~gated_read() override { --p_.alive; }

protected:
    bool execute() override {
        p_.started.set();
        p_.gate.wait();

        if (canceled()) {
            set_error(std::make_exception_ptr(std::runtime_error("cut short")));
            return true;
        }

        if (!p_.frame_alive) ++p_.wrote_after_the_frame;

        std::ranges::fill(into_, std::byte{42});
        ++p_.wrote;

        set_value(static_cast<int>(into_.size()));
        return true;
    }

    void on_cancel() noexcept override {
        ++p_.told;
        p_.gate.set();
    }

private:
    probe& p_;
    std::span<std::byte> into_;
};

/// The read, in the form with a token the operations of this module have.
task<int> read(probe& p, std::span<std::byte> into, cancellation_token stop) {
    return cancellation_detail::run_under<gated_read>(std::move(stop), p, into);
}

/// The operation a level awaits besides its subroutine, answering the level's number, in
/// the form with a token the operations of this module have -- where there is one.
///
/// An odd level awaits it before it calls down, so it is over before anything below is
/// sent, and it takes the three kinds by turns: inside the call, on the worker,
/// orphanable. An even level sends it after its subroutine has started -- by then the read
/// at the bottom may hold the one worker -- so it is answered inside the call or
/// orphanable: one queued behind the read would be waited for by an owner dropping it
/// before the read is given up, which is what opens the read's gate.
///
/// The one answered inside the call takes no token: it has nothing out to cut short.
task<int> step(int depth, cancellation_token stop) {
    const int kind = depth % 2 == 1 ? depth % 3 : depth % 4 == 0 ? 0 : 2;

    switch (kind) {
        case 0:
            return sta_loop::call_here([depth] { return depth; });
        case 1:
            return cancellation_detail::call_under([depth] { return depth; }, std::move(stop));
        default:
            return cancellation_detail::call_under(orphanable, [depth] { return depth; },
                                                   std::move(stop));
    }
}

/// Where the dropping level waits until the test tells it to drop.
struct drop_wait {
    chain_state& c;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) const noexcept { c.drop_slot = here; }
    void await_resume() const noexcept {}
};

task<int> bottom(chain_state& c, cancellation_token stop) {
    const frame_witness witness(c);

    if (c.bottom == bottom_kind::read) {
        std::byte buffer[16]{};
        const buffer_witness alive(c.p);

        co_return co_await read(c.p, buffer, std::move(stop));
    }

    if (c.bottom == bottom_kind::worker_failure)
        co_return co_await sta_loop::async_call([]() -> int { throw std::runtime_error("from the worker"); });

    const int value = co_await cancellation_detail::call_under([] { return 1000; }, stop);

    if (c.bottom == bottom_kind::coroutine_failure) throw std::runtime_error("from the bottom coroutine");

    co_return value;
}

/// Level `depth` of the chain: 1 stands on the bottom, and each above calls the one below.
task<int> level(chain_state& c, int depth, cancellation_token stop) {
    const frame_witness witness(c);
    int sum = 0;

    if (depth == 1) {
        sum = co_await bottom(c, stop);
    } else if (depth == c.drops_at) {
        // Owns its subroutine, and lets it go while it stands on the read at the bottom:
        // from outside its chain, as an owner does.
        std::optional<task<int>> below;
        below.emplace(level(c, depth - 1, stop));

        co_await drop_wait{c};
        below.reset();

        sum = depth;
    } else if (depth % 2 == 1) {
        sum = co_await step(depth, stop);
        sum += co_await level(c, depth - 1, stop);
    } else {
        task<int> below = level(c, depth - 1, stop);
        sum = co_await step(depth, stop);
        sum += co_await below;
    }

    ++c.ended;
    co_return sum;
}

/// The sum of the level numbers from `from` to `to`.
int levels_from(int from, int to) { return (from + to) * (to - from + 1) / 2; }

bool ends_cancelled(task<int>& chain) {
    try {
        (void)chain.result();
    } catch (const operation_canceled_exception&) {
        return true;
    } catch (...) {
    }

    return false;
}

std::string failure_of(task<int>& chain) {
    try {
        (void)chain.result();
    } catch (const std::runtime_error& e) {
        return e.what();
    }

    return "no failure";
}

/// Waits until everything sent before it has come back and been taken. An orphanable step
/// given up with the level that went -- dropped by its owner, or by a level that ended on the
/// cancellation -- finishes alone, and one still out when the test ends would stand in the
/// return channel ahead of the next test's operations. One worker, in turn: what was sent
/// first comes back first.
void take_back_the_given_up() {
    task<> fence = sta_loop::async_call([] {});
    sta_loop::run_until([&] { return fence.done(); });
}

}  // namespace

// Every level adds its number on the way up, and the bottom its 1000: the value of the
// deepest operation reaches the top through coroutines that awaited it as returned and
// coroutines that kept it and joined it, with operations of every kind between them. What
// is left at the end is the top frame alone, its locals already gone.
TEST(TaskDepthTest, TheValueClimbsToTheTop) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;

        task<int> chain = level(c, depth, {});
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_EQ(chain.result(), 1000 + levels_from(2, depth));
        EXPECT_EQ(c.ended, depth);
        EXPECT_EQ(c.frames, 0);
    }
}

// An operation that fails on the worker: the exception leaves through every level, none of
// which reaches its end, and is the top's to throw.
TEST(TaskDepthTest, AFailureOfTheBottomOperationClimbsToTheTop) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::worker_failure;

        task<int> chain = level(c, depth, {});
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_EQ(failure_of(chain), "from the worker");
        EXPECT_EQ(c.ended, 0);
        EXPECT_EQ(c.frames, 0);
    }
}

// The same from the deepest coroutine, after an operation has come back to it.
TEST(TaskDepthTest, AFailureOfTheBottomCoroutineClimbsToTheTop) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::coroutine_failure;

        task<int> chain = level(c, depth, {});
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_EQ(failure_of(chain), "from the bottom coroutine");
        EXPECT_EQ(c.ended, 0);
        EXPECT_EQ(c.frames, 0);
    }
}

// The token, passed down by hand through every level, reaches the read the chain stands on
// at the bottom. Asking resumes nobody; the read comes back cut short, writes nothing, and
// the cancellation climbs to the top.
TEST(TaskDepthTest, ARequestFromTheTopReachesTheBottom) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::read;
        cancellation_source stop;

        task<int> chain = level(c, depth, stop.token());
        sta_loop::run_until([&] { return c.p.made == 1; });
        c.p.started.wait();

        stop.cancel();

        EXPECT_EQ(c.p.told, 1);
        EXPECT_FALSE(chain.done()) << "cancel() resumed the chain itself";

        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(c.p.wrote, 0);
        EXPECT_EQ(c.ended, 0);
        EXPECT_EQ(c.frames, 0);
        EXPECT_EQ(c.p.alive, 0);

        take_back_the_given_up();
    }
}

// Asked before it began: the chain ends with the cancellation at the first wait under the
// token, and the read at the bottom is never made.
TEST(TaskDepthTest, ARequestBeforeTheStartEndsTheChainAtItsFirstWait) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::read;
        cancellation_source stop;
        stop.cancel();

        task<int> chain = level(c, depth, stop.token());
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(c.p.made, 0);
        EXPECT_EQ(c.ended, 0);
        EXPECT_EQ(c.frames, 0);
    }
}

// A level in the middle owns its subroutine and drops it while the chain below stands on
// the read at the bottom. What is below goes from the inside out: the read is given up and
// waited for, so it writes nothing into the frame being destroyed, and nobody below is
// resumed. What is above runs on and ends with the value of the levels it still has.
TEST(TaskDepthTest, AnOwnerInTheMiddleDropsWhatIsBelowIt) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::read;
        c.drops_at = depth / 2 + 1;

        task<int> chain = level(c, depth, {});
        sta_loop::run_until([&] { return c.p.made == 1; });
        c.p.started.wait();

        ASSERT_TRUE(c.drop_slot);
        c.drop_slot.resume();

        EXPECT_EQ(c.p.told, 1) << "giving the read up asks it to stop";
        EXPECT_EQ(c.p.wrote, 0);
        EXPECT_EQ(c.p.wrote_after_the_frame, 0);

        // The read given up is deleted by the loop when it meets it in the return channel.
        sta_loop::run_until([&] { return chain.done() && c.p.alive == 0; });

        EXPECT_EQ(chain.result(), levels_from(c.drops_at, depth));
        EXPECT_EQ(c.ended, depth - c.drops_at + 1) << "a level below the drop was resumed";
        EXPECT_EQ(c.frames, 0);

        take_back_the_given_up();
    }
}

// The owner of the whole chain drops it while it stands on the read: the same from the top.
TEST(TaskDepthTest, AnOwnerDroppingTheChainTakesItDownFromTheInsideOut) {
    for (int depth = shallowest; depth <= deepest; ++depth) {
        SCOPED_TRACE(std::format("depth {}", depth));
        chain_state c;
        c.bottom = bottom_kind::read;

        std::optional<task<int>> chain;
        chain.emplace(level(c, depth, {}));
        sta_loop::run_until([&] { return c.p.made == 1; });
        c.p.started.wait();

        chain.reset();

        EXPECT_EQ(c.p.told, 1);
        EXPECT_EQ(c.p.wrote, 0);
        EXPECT_EQ(c.ended, 0);
        EXPECT_EQ(c.frames, 0);

        sta_loop::run_until([&] { return c.p.alive == 0; });
        take_back_the_given_up();
    }
}

// The checks: the same rule broken at the bottom of a chain, by a coroutine taking the value
// of an operation twice, and of a coroutine twice, is caught there and named with its line.

namespace {

void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

task<int> answers(int value) { co_return value; }

// Each misuse a strict build reports with the line of the co_await sits alone on the line
// below the one that records it.

constexpr std::uint_least32_t line_of_the_second_take = std::source_location::current().line() + 1;
task<int> takes_twice(task<int>& kept) { (void)co_await kept; co_return co_await kept; }

/// A chain answered inside the call at every level, so that it breaks the rule before the
/// call that starts it returns -- in a process of its own, without the loop.
task<int> misusing_chain(int depth, bool of_an_operation) {
    const int here = co_await sta_loop::call_here([depth] { return depth; });

    if (depth > 1) co_return here + co_await misusing_chain(depth - 1, of_an_operation);

    task<int> kept = of_an_operation ? sta_loop::call_here([] { return 1; }) : answers(1);
    co_return here + co_await takes_twice(kept);
}

void misuse_at_the_bottom(int depth, bool of_an_operation) {
    report_failures_to_stderr();

    task<int> chain = misusing_chain(depth, of_an_operation);
}

std::string place_of(std::uint_least32_t line) {
    return std::format("task_depth_tests\\.cpp\\({}\\): ", line);
}

class TaskDepthDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

class TaskDepthStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(TaskDepthDeathTest, TheValueIsTakenOnceAtAnyDepth) {
    for (int depth : {shallowest, deepest}) {
        EXPECT_DEATH(misuse_at_the_bottom(depth, true), "already been taken");
        EXPECT_DEATH(misuse_at_the_bottom(depth, false), "already been taken");
    }
}

TEST_F(TaskDepthStrictDeathTest, TheReportNamesTheCoAwaitAtAnyDepth) {
    for (int depth : {shallowest, deepest}) {
        EXPECT_DEATH(misuse_at_the_bottom(depth, true),
                     place_of(line_of_the_second_take) + "task: the value has already been taken");
        EXPECT_DEATH(misuse_at_the_bottom(depth, false),
                     place_of(line_of_the_second_take) + "task: the value has already been taken");
    }
}
