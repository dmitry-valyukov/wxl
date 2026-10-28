// How long the frame of a task's coroutine lives, and where its result is meanwhile. The
// frame goes at the end of the body: right there when nobody awaits the task, and before the
// waiter is resumed when somebody does. The result outlives it in the task, to be read
// whenever its owner asks, and a body still in flight goes with the task that holds it.
//
// What a frame is made of is what these tests look at: a parameter taken by value has its
// copy in the frame and goes with it, so a parameter that counts its copies says whether the
// frame is still there. And a build that checks counts the frames where they are made and
// unmade, which live_frames() reports.
#include <crtdbg.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// A parameter by value that counts the copies made of it: the copy a coroutine keeps of its
/// parameter is made by the coroutine, into its frame, so the count is the number of frames
/// that hold one. The caller's own, a temporary living to the end of the caller's full
/// expression, is not counted.
class frame_mark
{
public:
    explicit frame_mark(int& alive) noexcept : alive_(&alive) {}

    frame_mark(const frame_mark& other) noexcept : alive_(other.alive_), counted_(true) {
        ++*alive_;
    }

    frame_mark& operator=(const frame_mark&) = delete;

    ~frame_mark() {
        if (counted_) --*alive_;
    }

private:
    int* alive_;
    bool counted_ = false;
};

/// Lets a test resume a coroutine by hand, as an operation would be taken back.
struct parked {
    std::coroutine_handle<>& slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { slot = here; }
    void await_resume() const noexcept {}
};

/// Counts its own destruction: a local of a body, which goes when the body is left.
struct notes_death {
    int& deaths;
    ~notes_death() { ++deaths; }
};

/// Throws, and to the compiler "gives" a T: the body of a coroutine with a value has to end
/// in co_return.
template <class T>
[[noreturn]] T fail_with(const char* what) {
    throw std::runtime_error(what);
}

task<int> answers_at_once(frame_mark) { co_return 7; }

task<int> answers_after_a_pause(frame_mark, std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return 42;
}

task<int> fails_after_a_pause(frame_mark, std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return fail_with<int>("after the pause");
}

task<> fails_plainly_after_a_pause(frame_mark, std::coroutine_handle<>& slot) {
    co_await parked{slot};
    throw std::runtime_error("without a value");
}

task<> ends_after_a_pause(frame_mark, std::coroutine_handle<>& slot) { co_await parked{slot}; }

/// What a waiter saw the moment its co_await gave it the value: still inside the full
/// expression of the co_await, where a temporary task the call returned is alive.
struct seen_at_resume {
    int value = 0;
    int frames = -1;
    std::size_t live = 0;
};

/// The usual co_await, of the task the call has just returned.
task<> awaits_the_call(int& alive, std::coroutine_handle<>& slot, seen_at_resume& seen) {
    if constexpr (detail::checked)
        seen = seen_at_resume{co_await answers_after_a_pause(frame_mark{alive}, slot), alive,
                              live_frames()};
    else
        seen = seen_at_resume{co_await answers_after_a_pause(frame_mark{alive}, slot), alive};
}

/// A co_await of a task kept in a local: the task lives on past the co_await.
task<> joins_a_kept_task(int& alive, std::coroutine_handle<>& slot, seen_at_resume& seen) {
    task<int> kept = answers_after_a_pause(frame_mark{alive}, slot);
    seen.value = co_await kept;
    seen.frames = alive;
    if constexpr (detail::checked) seen.live = live_frames();
}

/// Joins a task somebody else keeps.
task<> joins(task<int>& kept, int& got) { got = co_await kept; }

task<> joins_and_catches(task<int>& kept, int& frames_when_caught, std::string& what,
                         const int& alive) {
    try {
        (void)co_await kept;
    } catch (const std::runtime_error& failure) {
        what = failure.what();
        frames_when_caught = alive;
    }
}

task<> joins_twice(task<>& kept, int& asked) {
    co_await kept;
    ++asked;
    co_await kept;
    ++asked;
}

}  // namespace

// (a) With somebody awaiting, the frame is gone by the time the waiter has its value: the
// waiter sees no frame of it even in the full expression of its co_await, where the task the
// call returned still stands.
TEST(TaskLifetimeTest, AWaitedFrameIsGoneBeforeTheWaiterGoesOn) {
    int alive = 0;
    std::coroutine_handle<> slot;
    seen_at_resume seen;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<> outer = awaits_the_call(alive, slot, seen);

    ASSERT_TRUE(slot);
    EXPECT_EQ(alive, 1) << "the awaited frame keeps its copy while it waits";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before + 2);
    }

    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(seen.value, 42);
    EXPECT_EQ(seen.frames, 0) << "the waiter was resumed with the awaited frame still there";
    if constexpr (detail::checked) {
        EXPECT_EQ(seen.live, before + 1) << "the waiter's own frame is the one left";
    }
    EXPECT_EQ(alive, 0);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before)
            << "the outer frame, awaited by nobody, stayed after its body";
    }
}

TEST(TaskLifetimeTest, AJoinedFrameIsGoneBeforeTheJoinerGoesOn) {
    int alive = 0;
    std::coroutine_handle<> slot;
    seen_at_resume seen;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<> outer = joins_a_kept_task(alive, slot, seen);

    ASSERT_TRUE(slot);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before + 2);
    }

    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(seen.value, 42);
    EXPECT_EQ(seen.frames, 0) << "the frame of a kept task stayed after its body ended";
    if constexpr (detail::checked) {
        EXPECT_EQ(seen.live, before + 1);
        EXPECT_EQ(live_frames(), before);
    }
}

// (b) With nobody awaiting, the frame goes right at the end of the body, and the result
// waits in the task for its owner.
TEST(TaskLifetimeTest, AFrameNobodyAwaitsGoesAtTheEndOfItsBody) {
    int alive = 0;
    std::coroutine_handle<> slot;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<int> t = answers_after_a_pause(frame_mark{alive}, slot);

    ASSERT_TRUE(slot);
    EXPECT_EQ(alive, 1);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before + 1);
    }
    EXPECT_FALSE(t.done());

    slot.resume();

    EXPECT_EQ(alive, 0) << "the frame stayed after its body ended";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    ASSERT_TRUE(t.done());
    EXPECT_EQ(t.result(), 42);
}

// (c) The three orders of the end and the read.

// The body ends first, nobody awaiting; it is read afterwards, by result() or by a co_await
// that finds it ended and does not suspend.
TEST(TaskLifetimeTest, TheResultMadeBeforeTheReadWaitsInTheTask) {
    int alive = 0;
    std::coroutine_handle<> slot;
    task<int> first = answers_after_a_pause(frame_mark{alive}, slot);
    slot.resume();

    EXPECT_EQ(alive, 0);
    EXPECT_EQ(first.result(), 42);

    task<int> second = answers_after_a_pause(frame_mark{alive}, slot);
    slot.resume();

    int got = 0;
    task<> joiner = joins(second, got);

    EXPECT_TRUE(joiner.done()) << "a co_await of an ended task suspended";
    EXPECT_EQ(got, 42);
    EXPECT_EQ(alive, 0);
}

// The read comes first: the waiter parks, and the end of the body hands it the value.
TEST(TaskLifetimeTest, AReadBeforeTheResultIsHandedItByTheEnd) {
    int alive = 0;
    std::coroutine_handle<> slot;
    task<int> kept = answers_after_a_pause(frame_mark{alive}, slot);

    int got = 0;
    task<> joiner = joins(kept, got);

    EXPECT_FALSE(joiner.done());
    ASSERT_TRUE(slot);

    slot.resume();

    EXPECT_TRUE(joiner.done());
    EXPECT_EQ(got, 42);
    EXPECT_EQ(alive, 0);
}

// The body ends inside the call: by the time the call returns the frame is gone already,
// and the task holds the result.
TEST(TaskLifetimeTest, ABodyThatEndsInsideTheCallLeavesNoFrame) {
    int alive = 0;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<int> t = answers_at_once(frame_mark{alive});

    EXPECT_EQ(alive, 0) << "the frame of a body that never suspended outlived the call";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    ASSERT_TRUE(t.done());
    EXPECT_EQ(t.result(), 7);
}

// (d) The copies of the parameters go with the frame, at the end of the body, and not when
// the task that holds the result goes.
TEST(TaskLifetimeTest, TheCopiesOfTheParametersGoAtTheEndOfTheBody) {
    int alive = 0;
    std::coroutine_handle<> slot;
    {
        task<> t = ends_after_a_pause(frame_mark{alive}, slot);

        EXPECT_EQ(alive, 1);

        slot.resume();

        EXPECT_EQ(alive, 0) << "the copy of the parameter waited for the task to go";
        EXPECT_TRUE(t.done());
    }
    EXPECT_EQ(alive, 0);
}

namespace {

/// A body in flight: a local that notes its death, a copy of a parameter, and a pause.
task<int> stays_in_flight(frame_mark, std::coroutine_handle<>& slot, int& deaths, int& resumed) {
    notes_death guard{deaths};
    co_await parked{slot};
    ++resumed;
    co_return 1;
}

/// What keeps a task in a field, the way an object keeps the load it has under way.
struct holds_a_load {
    task<int> load;
};

}  // namespace

// (e) A field assigned a new task over one in flight stops the body it held: its locals and
// its copies go there and then, and nothing resumes it.
TEST(TaskLifetimeTest, AssigningOverAFieldStopsTheBodyItHeld) {
    int alive = 0, deaths = 0, resumed = 0;
    std::coroutine_handle<> first_slot, second_slot;
    holds_a_load owner;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();

    owner.load = stays_in_flight(frame_mark{alive}, first_slot, deaths, resumed);

    ASSERT_TRUE(first_slot);
    EXPECT_EQ(alive, 1);

    owner.load = stays_in_flight(frame_mark{alive}, second_slot, deaths, resumed);

    EXPECT_EQ(deaths, 1) << "the locals of the body assigned over are still there";
    EXPECT_EQ(alive, 1) << "only the new body's frame is left";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before + 1);
    }
    EXPECT_EQ(resumed, 0);

    ASSERT_TRUE(second_slot);
    second_slot.resume();

    EXPECT_EQ(deaths, 2);
    EXPECT_EQ(alive, 0);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    EXPECT_EQ(resumed, 1);
    EXPECT_EQ(owner.load.result(), 1);
}

namespace {

/// What the reads of one test saw, kept outside the frames they write into.
struct read_probe {
    /// Manual-reset: the held read waits here, and its cancellation opens it.
    hevent gate{true};

    /// Set once the worker is inside the held read.
    hevent started{true};

    /// Whether the buffers are still there to be written.
    std::atomic<bool> buffers_alive{false};

    std::atomic<int> wrote{0};
    std::atomic<int> wrote_after_the_buffers{0};
    std::atomic<int> canceled{0};

    /// Reads not yet deleted.
    std::atomic<int> alive{0};
};

/// Stands beside the buffers and says whether they are there. It opens the gate on its way
/// out too, so that a scheme which let the buffers go first fails the test instead of
/// leaving the worker asleep.
class buffers_witness
{
public:
    explicit buffers_witness(read_probe& p) noexcept : p_(p) { p_.buffers_alive = true; }

    ~buffers_witness() {
        p_.buffers_alive = false;
        p_.gate.set();
    }

private:
    read_probe& p_;
};

/// A read on a slow device into the caller's buffer: it waits at the gate, then fills the
/// buffer -- and, if told to, fails after that. Cancelling it opens the gate, as CancelIoEx
/// completes a read the kernel is holding.
class held_read : public async_op_t<std::size_t>
{
public:
    held_read(read_probe& p, std::span<std::byte> into, bool held, bool fails)
        : p_(p), into_(into), held_(held), fails_(fails) {
        ++p_.alive;
    }

    ~held_read() override { --p_.alive; }

protected:
    bool execute() override {
        if (held_) {
            p_.started.set();
            p_.gate.wait();
        }

        if (p_.buffers_alive)
            std::ranges::fill(into_, std::byte{42});
        else
            ++p_.wrote_after_the_buffers;

        ++p_.wrote;

        if (fails_) throw std::runtime_error("the read failed");

        set_value(into_.size());
        return true;
    }

    void on_cancel() noexcept override {
        ++p_.canceled;
        p_.gate.set();
    }

private:
    read_probe& p_;
    std::span<std::byte> into_;
    const bool held_;
    const bool fails_;
};

/// The read as a coroutine of its own: what `read(buffer)` is in application code.
task<std::size_t> read(read_probe& p, std::span<std::byte> into, bool held, bool fails) {
    co_return co_await sta_loop::async_run(
        std::unique_ptr<async_op_t<std::size_t>>(new held_read(p, into, held, fails)));
}

/// Two reads into two buffers of this frame, both under way before either is awaited; the
/// first one fails, and the frame unwinds with the second still on the worker, writing into
/// its buffer.
task<> reads_into_two_buffers(read_probe& p) {
    std::byte first_buffer[64]{};
    std::byte second_buffer[64]{};
    buffers_witness witness(p);

    auto a = read(p, first_buffer, false, true);
    auto b = read(p, second_buffer, true, false);

    co_await a;
    co_await b;
}

}  // namespace

// (f) Destroying a task whose body stands on an operation in flight waits for the worker to
// let go of it: the buffer it writes into is still there for as long as it writes.
TEST(TaskLifetimeTest, ABodyStoppedOnAnOperationInFlightWaitsForTheWorker) {
    read_probe p;
    task<> work = reads_into_two_buffers(p);

    // The first read goes to the worker first; once the worker is inside the second, the
    // first is done and on its way back, and the unwinding finds the second running.
    p.started.wait();

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(p.canceled.load(), 1) << "the second read was never given up";
    EXPECT_EQ(p.wrote.load(), 2);
    EXPECT_EQ(p.wrote_after_the_buffers.load(), 0)
        << "the worker wrote into buffers that were gone";
    EXPECT_FALSE(p.buffers_alive.load());

    // The read given up waits in the return channel for the loop to delete it.
    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a read given up resumed somebody";
    EXPECT_EQ(p.alive.load(), 0);
}

namespace {

constexpr int chain_depth = 10;

// Where the stack stands in the function that runs the body: that function's own frame, which a
// local of the body cannot show -- the local is in the coroutine's frame, and under the address
// sanitizer a local of a plain function is in a stack of its own.
#if defined(_MSC_VER)
#define STACK_POSITION() reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress())
#else
#define STACK_POSITION() reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0))
#endif

/// Whether this build turns the hand-over at the end of a body into a jump, as the stack check
/// below expects: MSVC does. g++ does only when it optimizes, and neither g++ nor clang does
/// under the address sanitizer; there the numbers are printed and not held against the build.
constexpr bool hands_over_by_a_jump =
#if defined(_MSC_VER)
    true;
#elif defined(__clang__)
#if __has_feature(address_sanitizer)
    false;
#else
    true;
#endif
#elif !defined(__OPTIMIZE__) || defined(__SANITIZE_ADDRESS__)
    false;
#else
    true;
#endif

/// What the levels of a chain saw as each was resumed.
struct chain_notes {
    std::coroutine_handle<> slot;
    int alive = 0;

    /// Per level: how many frames were there in the full expression of its co_await, and
    /// where the stack stood once it went on.
    std::array<int, chain_depth + 1> frames_at_resume{};
    std::array<std::size_t, chain_depth + 1> live_at_resume{};
    std::array<std::uintptr_t, chain_depth + 1> stack_at_resume{};
};

/// A level of the chain: the bottom one waits to be resumed by hand, every other awaits the
/// level below it, and each keeps one copy of the counted parameter in its frame.
task<int> chain_level(int depth, frame_mark, chain_notes& notes) {
    if (depth == 0) {
        co_await parked{notes.slot};
        notes.frames_at_resume[0] = notes.alive;
        if constexpr (detail::checked) notes.live_at_resume[0] = live_frames();
        notes.stack_at_resume[0] = STACK_POSITION();
        co_return 0;
    }

    int below = 0;
    int frames = 0;
    std::size_t live = 0;

    if constexpr (detail::checked)
        std::tie(below, frames, live) = std::tuple<int, int, std::size_t>{
            co_await chain_level(depth - 1, frame_mark{notes.alive}, notes), notes.alive,
            live_frames()};
    else
        std::tie(below, frames) = std::tuple<int, int>{
            co_await chain_level(depth - 1, frame_mark{notes.alive}, notes), notes.alive};

    notes.frames_at_resume[depth] = frames;
    notes.live_at_resume[depth] = live;
    notes.stack_at_resume[depth] = STACK_POSITION();
    co_return below + 1;
}

}  // namespace

// (g) A chain ten levels deep ends link by link: each frame is gone before the level above it
// goes on, and each level is resumed at the same depth of the stack as the one below it --
// the thread is handed over, not called into.
TEST(TaskLifetimeTest, AChainTenDeepUnmakesItsFramesAsItEnds) {
    chain_notes notes;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<int> top = chain_level(chain_depth, frame_mark{notes.alive}, notes);

    ASSERT_TRUE(notes.slot);
    EXPECT_EQ(notes.alive, chain_depth + 1);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before + chain_depth + 1);
    }

    notes.slot.resume();

    ASSERT_TRUE(top.done());
    EXPECT_EQ(top.result(), chain_depth);
    EXPECT_EQ(notes.alive, 0) << "the top frame stayed after its body ended";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }

    // The bottom saw every frame; each level above, only its own and those above it.
    EXPECT_EQ(notes.frames_at_resume[0], chain_depth + 1);
    if constexpr (detail::checked) {
        EXPECT_EQ(notes.live_at_resume[0], before + chain_depth + 1);
    }

    for (int depth = 1; depth <= chain_depth; ++depth) {
        EXPECT_EQ(notes.frames_at_resume[depth], chain_depth + 1 - depth) << "level " << depth;
        if constexpr (detail::checked) {
            EXPECT_EQ(notes.live_at_resume[depth], before + chain_depth + 1 - depth)
                << "level " << depth;
        }
    }

    const auto [lowest, highest] = std::ranges::minmax(notes.stack_at_resume);

    if constexpr (hands_over_by_a_jump) {
        EXPECT_LT(highest - lowest, 256u)
            << "the stack grew from one level of the chain to the next";
    } else {
        std::cout << std::format(
            "[ INFO     ] the stack spread {} bytes over the chain; this build "
            "does not promise a jump\n",
            highest - lowest);
    }
}

namespace {

task<> awaits_a_failure(int& alive, std::coroutine_handle<>& slot, std::string& what,
                        int& frames_when_caught) {
    task<int> kept = fails_after_a_pause(frame_mark{alive}, slot);

    try {
        (void)co_await kept;
    } catch (const std::runtime_error& failure) {
        what = failure.what();
        frames_when_caught = alive;
    }
}

}  // namespace

// (h) An exception out of the body: the frame goes the same way, before the waiter catches it
// or right at the end with nobody waiting, and the exception waits in the task.
TEST(TaskLifetimeTest, AFailureWithAWaiterReachesItWithTheFrameGone) {
    int alive = 0, frames_when_caught = -1;
    std::coroutine_handle<> slot;
    std::string what;
    task<> outer = awaits_a_failure(alive, slot, what, frames_when_caught);

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_NO_THROW(outer.result());
    EXPECT_EQ(what, "after the pause");
    EXPECT_EQ(frames_when_caught, 0)
        << "the frame that failed was still there when its waiter caught";
}

TEST(TaskLifetimeTest, AFailureNobodyAwaitsWaitsInTheTaskWithTheFrameGone) {
    int alive = 0;
    std::coroutine_handle<> slot;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();
    task<int> with_value = fails_after_a_pause(frame_mark{alive}, slot);

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_EQ(alive, 0) << "the frame that failed stayed after its body ended";
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
    ASSERT_TRUE(with_value.done());
    EXPECT_THROW((void)with_value.result(), std::runtime_error);
    EXPECT_THROW((void)with_value.result(), std::runtime_error)
        << "the exception is thrown each time";

    task<> without = fails_plainly_after_a_pause(frame_mark{alive}, slot);
    slot.resume();

    EXPECT_EQ(alive, 0);
    ASSERT_TRUE(without.done());
    EXPECT_THROW(without.result(), std::runtime_error);

    int frames_when_caught = -1;
    std::string what;
    task<int> joined = fails_after_a_pause(frame_mark{alive}, slot);
    task<> catcher = joins_and_catches(joined, frames_when_caught, what, alive);
    slot.resume();

    EXPECT_TRUE(catcher.done());
    EXPECT_EQ(what, "after the pause");
    EXPECT_EQ(frames_when_caught, 0);
}

// (i) A task<> has no value to take: its end is read as often as it is asked, by result() and
// by co_await, in any order, with the frame long gone.
TEST(TaskLifetimeTest, AnEndWithoutAValueIsReadAsOftenAsItIsAsked) {
    int alive = 0;
    std::coroutine_handle<> slot;
    task<> t = ends_after_a_pause(frame_mark{alive}, slot);
    slot.resume();

    EXPECT_EQ(alive, 0);
    EXPECT_NO_THROW(t.result());
    EXPECT_NO_THROW(t.result());

    int asked = 0;
    task<> joiner = joins_twice(t, asked);

    EXPECT_TRUE(joiner.done());
    EXPECT_EQ(asked, 2);
    EXPECT_NO_THROW(t.result());
}

// The count of frames is the count of bodies under way: a frame stopped in flight leaves it as
// one that ended does, and a task holding only its result is not in it.
TEST(TaskLifetimeTest, AFrameIsCountedForAsLongAsItsBody) {
    if constexpr (detail::checked) {
        int alive = 0;
        std::coroutine_handle<> slot;
        const std::size_t before = live_frames();
        {
            task<int> stopped = answers_after_a_pause(frame_mark{alive}, slot);
            task<int> ended = answers_at_once(frame_mark{alive});

            EXPECT_EQ(live_frames(), before + 1);
        }
        EXPECT_EQ(live_frames(), before) << "a frame stopped in flight is still counted";
        EXPECT_EQ(alive, 0);
    } else {
        GTEST_SKIP() << "frames are counted only in a build that checks coroutines";
    }
}

namespace {

/// A local of a body whose destructor resumes, there and then, a coroutine parked by the test:
/// what a body's locals may do on their way out -- an event fired, a wait answered.
struct resumes_as_it_goes {
    std::coroutine_handle<>& parked_slot;
    std::vector<std::string>& log;

    ~resumes_as_it_goes() {
        std::exchange(parked_slot, {}).resume();
        log.push_back("the body's local gone");
    }
};

task<int> ends_with_a_local_that_resumes(std::coroutine_handle<>& own_slot,
                                         std::coroutine_handle<>& parked_slot,
                                         std::vector<std::string>& log) {
    const resumes_as_it_goes local{parked_slot, log};
    co_await parked{own_slot};
    co_return 5;
}

/// Parks first, and once resumed joins the task kept in `kept`.
task<> joins_once_resumed(std::coroutine_handle<>& slot, task<int>& kept,
                          std::vector<std::string>& log) {
    co_await parked{slot};
    log.push_back("the joiner asks");
    const int got = co_await kept;
    log.push_back(std::format("the joiner has {}", got));
}

}  // namespace

// The result is there once the body is over, not once it is given: a co_await reaching the
// task between the co_return and the end -- from the destructor of one of the body's locals --
// does not find it ready, and is handed it only after the last local has gone.
TEST(TaskLifetimeTest, TheResultIsThereOnlyOnceTheBodyIsOver) {
    std::coroutine_handle<> own_slot, joiner_slot;
    std::vector<std::string> log;
    task<int> kept;

    task<> joiner = joins_once_resumed(joiner_slot, kept, log);
    kept = ends_with_a_local_that_resumes(own_slot, joiner_slot, log);

    ASSERT_TRUE(own_slot);
    own_slot.resume();

    EXPECT_TRUE(joiner.done());
    EXPECT_EQ(log, (std::vector<std::string>{"the joiner asks", "the body's local gone",
                                             "the joiner has 5"}))
        << "the result was handed out while the body's locals were still going";
}

// (j) The checks: a value taken by result() is not there for a co_await after it.

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so that a death test
/// can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void await_after_the_result() {
    report_failures_to_stderr();

    int alive = 0, got = 0;
    task<int> t = answers_at_once(frame_mark{alive});
    (void)t.result();
    task<> late = joins(t, got);
}

/// An object that keeps its own load in a field, and a load that keeps the object alive by a
/// parameter: the frame of the load is the object's last holder.
struct keeps_its_load : sta_refcounted {
    task<int> load;
};

task<int> loads_holding(intrusive_ptr<keeps_its_load>, std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return 1;
}

/// Joins the field of an object it does not hold: the object goes with the frame of its load --
/// before the joiner is resumed -- and the task the joiner waits on goes with it. Waiting
/// through an object takes holding the object.
void join_a_task_whose_object_goes_with_its_frame() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int got = 0;
    task<int>* field = nullptr;
    {
        auto owner = make_refcounted<keeps_its_load>();
        owner->load = loads_holding(owner, slot);
        field = &owner->load;
    }

    task<> joiner = joins(*field, got);
    slot.resume();
}

/// Death tests of the checks: skipped where the build has none.
class TaskLifetimeDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

}  // namespace

TEST_F(TaskLifetimeDeathTest, AValueTakenByResultIsNotThereForACoAwait) {
    EXPECT_DEATH(await_after_the_result(), "already been taken");
}

TEST_F(TaskLifetimeDeathTest, AJoinerHoldsTheObjectWhoseTaskItJoins) {
    EXPECT_DEATH(join_a_task_whose_object_goes_with_its_frame(),
                 "destroyed while a coroutine joins it");
}
