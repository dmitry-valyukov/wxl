// What happens to an asynchronous operation whose task goes away while the operation
// is still out: its frame unwinds on an exception, its task is dropped, or it was never
// awaited at all. Every test here checks the rule ordinary code keeps on a plain stack --
// by the time a frame is gone, nobody writes into it -- together with the one a thread in
// the middle of unwinding needs: nothing else is resumed while it waits.
#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// What the operations of one test saw and did, kept outside them: a given-up operation is
/// deleted by the loop whenever its turn comes, and the test looks afterwards.
struct probe {
    /// Manual-reset: the bodies wait here, and whoever opens it opens it for good.
    hevent gate{true};

    /// Set by the first body to reach the gate, so a test can know the worker is inside it.
    hevent started{true};

    std::atomic<bool> frame_alive{false};

    std::atomic<int> ran{0};
    std::atomic<int> ran_with_frame_alive{0};
    std::atomic<int> canceled{0};

    /// Operations of this probe not yet deleted.
    std::atomic<int> alive{0};
};

/// Lives in a coroutine frame and says so. On the way out it opens the gate as well, so
/// that a loop which let the frame go first fails the test rather than leaving the worker
/// asleep for every test after it.
class frame_witness
{
public:
    explicit frame_witness(probe& p) : p_(p) { p_.frame_alive = true; }

    ~frame_witness() {
        p_.frame_alive = false;
        p_.gate.set();
    }

private:
    probe& p_;
};

/// A read standing for one on a slow device: the body waits at the gate, then writes into
/// the caller's buffer -- which lives in the frame -- if the frame is there to write into.
///
/// Cancelling it opens a gate, the way CancelIoEx completes a read the kernel is holding:
/// its own, or `opens` when a test needs the cancellation to release something else.
class gated_read : public async_op_t<std::size_t>
{
public:
    gated_read(probe& p, std::span<std::byte> into, hevent* opens)
        : p_(p), into_(into), opens_(opens ? opens : &p.gate) {
        ++p_.alive;
    }

    ~gated_read() override { --p_.alive; }

protected:
    bool execute() override {
        p_.started.set();
        p_.gate.wait();

        ++p_.ran;

        if (p_.frame_alive) {
            ++p_.ran_with_frame_alive;
            std::ranges::fill(into_, std::byte{42});
        }

        set_value(into_.size());
        return true;
    }

    void on_cancel() noexcept override {
        ++p_.canceled;
        opens_->set();
    }

private:
    probe& p_;
    std::span<std::byte> into_;
    hevent* opens_;
};

task<std::size_t> start_read(probe& p, std::span<std::byte> into, hevent* opens = nullptr) {
    return sta_loop::async_run(
        std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into, opens)));
}

task<> start_failure() {
    return sta_loop::async_call([] { throw std::runtime_error("the first read failed"); });
}

/// The code from the discussion: two reads started, the first one awaited fails, and the
/// frame unwinds with the second still on the worker, writing into `second_buf`.
task<> first_fails_second_in_flight(probe& p) {
    frame_witness witness(p);
    std::byte second_buf[64]{};

    auto a = start_failure();
    auto b = start_read(p, second_buf);

    co_await a;
    co_await b;
}

/// Awaited in the order opposite to the one they were started in: `a` comes back while the
/// coroutine is suspended on `b`, with nobody waiting for it yet.
task<> awaits_in_the_opposite_order(int& first, int& second) {
    auto a = sta_loop::async_call([] { return 1; });
    auto b = sta_loop::async_call([] { return 2; });

    second = co_await b;
    first = co_await a;
}

/// Starts a read and walks away without ever awaiting it.
task<> starts_and_walks_away(probe& p) {
    frame_witness witness(p);
    std::byte buf[16]{};

    auto read = start_read(p, buf);

    co_return;
}

/// The worker is held inside `k` while `a` fails, so `b` is still queued behind it when the
/// frame gives it up; cancelling `b` is what lets `k` go.
task<> fails_with_one_running_and_one_queued(probe& running, probe& queued) {
    frame_witness running_witness(running);
    frame_witness queued_witness(queued);
    std::byte running_buf[16]{};
    std::byte queued_buf[16]{};

    auto a = start_failure();
    auto k = start_read(running, running_buf);
    auto b = start_read(queued, queued_buf, &running.gate);

    co_await a;
    co_await k;
    co_await b;
}

task<> returns_seven(int& out) {
    out = co_await sta_loop::async_call([] { return 7; });
}

/// The first operation is ahead of the second in every queue, so by the time the second
/// has been awaited the loop has taken the first out as well, with nobody waiting for it.
task<> watches_readiness(bool& before, bool& after, bool& here) {
    auto first = sta_loop::async_call([] { return 3; });

    before = first.done();

    co_await sta_loop::async_call([] {});

    after = first.done();
    here = sta_loop::call_here([] { return 1; }).done();

    co_await first;
}

/// Writes down, as the frame goes, what `watched` held at that moment.
class snapshot_at_exit
{
public:
    snapshot_at_exit(const int& watched, int& seen) : watched_(watched), seen_(seen) {}

    ~snapshot_at_exit() { seen_ = watched_; }

private:
    const int& watched_;
    int& seen_;
};

/// The same as above, with another coroutine's operation started between `k` and `b`: it
/// comes back while this frame is unwinding, and must wait for the unwinding to end.
task<> fails_while_another_coroutine_is_answered(probe& running, probe& queued,
                                                 std::vector<task<>>& others, int& other_out,
                                                 int& other_out_while_unwinding) {
    snapshot_at_exit snapshot(other_out, other_out_while_unwinding);
    frame_witness running_witness(running);
    frame_witness queued_witness(queued);
    std::byte running_buf[16]{};
    std::byte queued_buf[16]{};

    auto a = start_failure();
    auto k = start_read(running, running_buf);

    others.push_back(returns_seven(other_out));

    auto b = start_read(queued, queued_buf, &running.gate);

    co_await a;
    co_await k;
    co_await b;
}

/// Suspends on a read into its own frame, and is dropped by its owner there.
task<> reads_into_its_frame(probe& p) {
    frame_witness witness(p);
    std::byte buf[64]{};

    co_await start_read(p, buf);

    ADD_FAILURE() << "resumed after its task was dropped";
}

task<> fails_through_an_operation() { co_await start_failure(); }

task<std::size_t> answers_with_a_read_into_its_frame(probe& p) {
    frame_witness witness(p);
    std::byte buf[64]{};

    co_return co_await start_read(p, buf);
}

/// The code from the discussion one level up: two tasks in flight rather than two
/// operations. The first one awaited fails, and the frame unwinds past the second, which
/// stands on a read into its own frame -- and goes with the outer one, giving the read up.
task<> first_task_fails_second_in_flight(probe& p) {
    auto a = fails_through_an_operation();
    auto b = answers_with_a_read_into_its_frame(p);

    co_await a;

    // Not reached. A frame that went on would wait for a read nobody lets go of, so the
    // gate is opened for it: the test fails instead of hanging.
    ADD_FAILURE() << "went on past a task that failed";
    p.gate.set();

    co_await b;
}

/// Many reads out at once and the first awaited one fails: every one of them is given up
/// in the same unwinding, in the order a vector destroys its elements -- the order the
/// operations went out in, so each is looked for from the head of the return channel
/// past all the ones given up before it.
task<> fails_with_many_reads_out(probe& p, std::size_t count) {
    frame_witness witness(p);
    std::vector<std::array<std::byte, 16>> buffers(count);
    std::vector<task<std::size_t>> reads;

    reads.reserve(count);

    auto first = start_failure();

    for (auto& buffer : buffers) reads.push_back(start_read(p, buffer));

    co_await first;

    for (auto& read : reads) co_await read;
}

/// Where a resource was let go of, and the moment it was.
struct release_record {
    std::atomic<int> times{0};
    std::thread::id on;

    /// Manual-reset, set as the resource goes.
    hevent happened{true};
};

/// What an operation that opens something hands back: a resource of its own.
class handle_like
{
public:
    explicit handle_like(release_record& record) : record_(&record) {}

    handle_like(handle_like&& other) noexcept : record_(std::exchange(other.record_, nullptr)) {}

    handle_like& operator=(handle_like&&) = delete;

    ~handle_like() {
        if (!record_) return;

        record_->on = std::this_thread::get_id();
        ++record_->times;
        record_->happened.set();
    }

private:
    release_record* record_;
};

/// Starts an operation that touches nothing of the frame, and gives it up before awaiting
/// -- once the worker is inside it, so that it has something to finish alone.
task<> opens_and_changes_its_mind(hevent& started, hevent& gate, release_record& record,
                                  bool changes_its_mind) {
    auto opening = sta_loop::async_call(orphanable, [&started, &gate, &record] {
        started.set();
        gate.wait();
        return handle_like(record);
    });

    started.wait();

    if (changes_its_mind) throw std::runtime_error("changed its mind");

    co_await opening;
}

/// Counts, as it goes, that the operation carrying it has been deleted: it rides in the
/// capture of an operation's body, which is destroyed with the operation.
class deletion_counter
{
public:
    explicit deletion_counter(std::atomic<int>& deleted) : deleted_(&deleted) {}

    deletion_counter(deletion_counter&& other) noexcept
        : deleted_(std::exchange(other.deleted_, nullptr)) {}

    deletion_counter& operator=(deletion_counter&&) = delete;

    ~deletion_counter() {
        if (deleted_) ++*deleted_;
    }

private:
    std::atomic<int>* deleted_;
};

/// Queues an orphan behind a read the worker is held inside, and gives both up: the orphan
/// before the worker can have reached it.
task<> queues_an_orphan_and_changes_its_mind(probe& running, std::atomic<bool>& orphan_ran,
                                             std::atomic<int>& orphan_deleted,
                                             bool changes_its_mind) {
    frame_witness witness(running);
    std::byte buf[16]{};

    auto k = start_read(running, buf);
    auto opening =
        sta_loop::async_call(orphanable, [&orphan_ran, counter = deletion_counter(orphan_deleted)] {
            orphan_ran = true;
            return 0;
        });

    running.started.wait();

    if (changes_its_mind) throw std::runtime_error("changed its mind");

    co_await opening;
    co_await k;
}

/// An exception that counts its own copies: what an exception_ptr keeps alive.
class counted_failure : public std::runtime_error
{
public:
    static inline std::atomic<int> live{0};

    counted_failure() : std::runtime_error("counted") { ++live; }
    counted_failure(const counted_failure& other) : std::runtime_error(other) { ++live; }
    ~counted_failure() override { --live; }
};

/// A value that counts itself, and counts being destroyed twice. Made on the worker, and
/// destroyed on whichever thread lets go of it.
class counted_value
{
public:
    static inline std::atomic<int> live{0};
    static inline std::atomic<int> destroyed_twice{0};

    counted_value() noexcept { ++live; }
    counted_value(counted_value&&) noexcept { ++live; }
    counted_value& operator=(counted_value&&) = delete;

    ~counted_value() {
        if (state_ != constructed) ++destroyed_twice;
        state_ = gone;
        --live;
    }

private:
    static constexpr std::uint32_t constructed = 0x600dc0de;
    static constexpr std::uint32_t gone = 0xdeadbeef;

    std::uint32_t state_ = constructed;
};

/// Both counts at zero before the test, and checked after it.
class counts_balanced
{
public:
    counts_balanced() noexcept {
        counted_failure::live = 0;
        counted_value::live = 0;
        counted_value::destroyed_twice = 0;
    }

    counts_balanced(const counts_balanced&) = delete;
    counts_balanced& operator=(const counts_balanced&) = delete;

    ~counts_balanced() {
        EXPECT_EQ(counted_failure::live.load(), 0) << "an exception was kept past its operation";
        EXPECT_EQ(counted_value::live.load(), 0) << "a value was kept past its operation";
        EXPECT_EQ(counted_value::destroyed_twice.load(), 0);
    }
};

/// Answers a counted value once the gate opens; cancelling it opens the gate, and it
/// answers the value all the same, as one that got there first does.
class gated_answer : public async_op_t<counted_value>
{
public:
    gated_answer(hevent& started, hevent& gate) noexcept : started_(started), gate_(gate) {}

protected:
    bool execute() override {
        started_.set();
        gate_.wait();

        set_value(counted_value{});
        return true;
    }

    void on_cancel() noexcept override { gate_.set(); }

private:
    hevent& started_;
    hevent& gate_;
};

task<counted_value> start_gated_answer(hevent& started, hevent& gate) {
    return sta_loop::async_run(
        std::unique_ptr<async_op_t<counted_value>>(new gated_answer(started, gate)));
}

/// Given-up operations stay in the return channel until their turn, and the sleeping loop
/// of this binary takes them only when asked. Asked here, so that no test leaves anything
/// behind for the next one.
void take_what_was_given_up() {
    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}

}  // namespace

TEST(StaLoopAbandonTest, AFrameUnwindingPastAnOperationWaitsForTheWorkerToLetGo) {
    probe p;
    task<> work = first_fails_second_in_flight(p);

    // The worker is inside the second read before the first one is looked at, so the
    // unwinding below finds it running rather than queued.
    p.started.wait();

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(p.canceled.load(), 1) << "the operation was never told it had been given up";
    EXPECT_EQ(p.ran.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOperationBackBeforeItsCoAwaitIsTakenThereWithoutAResume) {
    int first = 0;
    int second = 0;
    task<> work = awaits_in_the_opposite_order(first, second);

    sta_loop::run_until(work);

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(StaLoopAbandonTest, AnAwaitableIsReadyOnceTheLoopHasTakenItsOperationOut) {
    bool before = true;
    bool after = false;
    bool here = false;
    task<> work = watches_readiness(before, after, here);

    sta_loop::run_until(work);

    EXPECT_FALSE(before);
    EXPECT_TRUE(after);
    EXPECT_TRUE(here);
}

TEST(StaLoopAbandonTest, AnOperationNeverAwaitedIsWaitedForByTheFrameThatStartedIt) {
    probe p;
    task<> work = starts_and_walks_away(p);

    // The whole body ran inside the call, the wait included: the coroutine never suspended.
    EXPECT_TRUE(work.done());
    work.result();

    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran.load(), p.ran_with_frame_alive.load())
        << "the worker wrote into a frame that was gone";
    EXPECT_FALSE(p.frame_alive.load());

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOperationGivenUpBeforeItStartsNeverRuns) {
    probe running;
    probe queued;
    task<> work = fails_with_one_running_and_one_queued(running, queued);

    // The worker is inside `k`, so `b` is behind it and cannot have started.
    running.started.wait();

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(queued.canceled.load(), 1);
    EXPECT_EQ(queued.ran.load(), 0) << "a cancelled operation was started all the same";
    EXPECT_EQ(running.ran.load(), 1);
    EXPECT_EQ(running.ran_with_frame_alive.load(), 1);

    take_what_was_given_up();

    EXPECT_EQ(running.alive.load(), 0);
    EXPECT_EQ(queued.alive.load(), 0);
}

TEST(StaLoopAbandonTest, NobodyElseIsResumedWhileAFrameUnwinds) {
    probe running;
    probe queued;
    std::vector<task<>> others;
    int other_out = 0;
    int other_out_while_unwinding = -1;

    task<> work = fails_while_another_coroutine_is_answered(running, queued, others, other_out,
                                                            other_out_while_unwinding);
    running.started.wait();

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(other_out_while_unwinding, 0)
        << "another coroutine was resumed from inside an unwinding frame";

    // ...and was not lost either: its answer came back during the wait, and is there to be
    // taken now.
    ASSERT_EQ(others.size(), 1u);

    sta_loop::run_until([&] { return others.front().done(); });
    others.front().result();

    EXPECT_EQ(other_out, 7);

    take_what_was_given_up();

    EXPECT_EQ(running.alive.load(), 0);
    EXPECT_EQ(queued.alive.load(), 0);
}

TEST(StaLoopAbandonTest, DroppingATaskSuspendedOnAnOperationWaitsForTheWorker) {
    probe p;

    {
        task<> work = reads_into_its_frame(p);

        p.started.wait();

        EXPECT_FALSE(work.done());
    }

    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";
    EXPECT_FALSE(p.frame_alive.load());

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, ATaskInFlightPastWhichAFrameUnwindsWaitsForTheWorkerToLetGo) {
    probe p;
    task<> work = first_task_fails_second_in_flight(p);

    // The failure went to the worker first, so by the time it is inside the read the
    // failure is on its way back, and the second task is found running, not queued.
    p.started.wait();

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(p.canceled.load(), 1) << "the second task's read was never given up";
    EXPECT_EQ(p.ran.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";
    EXPECT_FALSE(p.frame_alive.load());

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, ManyOperationsGivenUpInOneUnwindingAllWaitForTheWorker) {
    constexpr std::size_t count = 64;

    probe p;
    task<> work = fails_with_many_reads_out(p, count);

    EXPECT_THROW(sta_loop::run_until(work), std::runtime_error);
    EXPECT_EQ(p.ran.load(), p.ran_with_frame_alive.load())
        << "the worker wrote into a frame that was gone";

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOrphanGivenUpWhileItRunsLetsGoOfWhatItMakesAsSoonAsItIsMade) {
    hevent started{true};
    hevent gate{true};
    release_record record;

    // Only a scheme that waits for an orphan would keep the call below from returning while
    // the gate is shut. The watchdog opens it after a while, so that such a scheme fails
    // the test instead of hanging the binary.
    std::binary_semaphore unwound{0};
    std::atomic<bool> had_to_wait{false};
    std::jthread watchdog([&] {
        if (unwound.try_acquire_for(std::chrono::seconds(10))) return;

        had_to_wait = true;
        gate.set();
    });

    task<> work = opens_and_changes_its_mind(started, gate, record, true);

    unwound.release();
    watchdog.join();

    EXPECT_FALSE(had_to_wait.load())
        << "the frame waited for an operation that touches nothing of it";
    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(record.times.load(), 0);

    // The orphan runs to its end on its own, and what it made goes there and then: the
    // loop has not turned, and nobody is left to take it.
    gate.set();
    record.happened.wait();

    EXPECT_EQ(record.times.load(), 1);
    EXPECT_NE(record.on, std::this_thread::get_id());

    // The operation itself still comes back, to be deleted where it was made.
    sta_loop::run_one();
    take_what_was_given_up();
}

TEST(StaLoopAbandonTest, AnOrphanGivenUpWhenItIsDoneLetsGoOfWhatItMadeBeforeTheDestructorReturns) {
    release_record record;
    hevent passed{true};

    {
        auto opening = sta_loop::async_call(orphanable, [&record] { return handle_like(record); });

        // The worker takes one operation at a time, so by the time it is inside this
        // one it has done with the opening -- which nobody has taken back yet.
        auto barrier = sta_loop::async_call([&passed] { passed.set(); });

        passed.wait();

        EXPECT_EQ(record.times.load(), 0);
    }

    EXPECT_EQ(record.times.load(), 1);
    EXPECT_EQ(record.on, std::this_thread::get_id());

    take_what_was_given_up();
}

TEST(StaLoopAbandonTest, AnOrphanGivenUpBeforeItStartsIsNeverRunAndStillDeleted) {
    probe running;
    std::atomic<bool> orphan_ran{false};
    std::atomic<int> orphan_deleted{0};

    task<> work = queues_an_orphan_and_changes_its_mind(running, orphan_ran, orphan_deleted, true);

    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(running.ran_with_frame_alive.load(), 1);

    // The worker skips the orphan and hands it back; the loop deletes it as it takes it.
    while (orphan_deleted.load() == 0) sta_loop::run_one();

    EXPECT_FALSE(orphan_ran.load()) << "an operation given up before it started was run";
    EXPECT_EQ(orphan_deleted.load(), 1);
    EXPECT_EQ(running.alive.load(), 0);
}

// An orphan whose task went at once is left to finish alone on the worker, and is counted while
// it does: run_until() on outstanding() -- what the end of a process waits for -- ends once
// the loop has taken it back and deleted it.
TEST(StaLoopAbandonTest, AnOrphanLeftToFinishAloneIsWaitedForByRunUntil) {
    std::atomic<int> deleted{0};

    {
        auto let_go = sta_loop::async_call(orphanable, [counter = deletion_counter(deleted)] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 0;
        });
    }

    EXPECT_NE(sta_loop::outstanding(), 0u) << "an orphan left to finish alone is not counted";
    EXPECT_EQ(deleted.load(), 0);

    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

    EXPECT_EQ(deleted.load(), 1) << "the wait ended before the orphan was back";
}

// What a given-up operation made is destroyed once: an ordinary one's where it is deleted, by
// the destructor; an orphan's by whichever thread wins the race over it, and not again when
// the loop deletes the orphan. An error is released at the deletion either way.

TEST(StaLoopAbandonTest, AValueOfAnOperationGivenUpIsDestroyedOnceAsItIsDeleted) {
    counts_balanced counts;
    hevent started{true};
    hevent gate{true};

    // Next in line once it is back: the task's destructor deletes it, with its value.
    {
        task<counted_value> answer = start_gated_answer(started, gate);
        started.wait();
    }

    EXPECT_EQ(counted_value::live.load(), 0);

    // Behind another one: the loop deletes it when it takes it back.
    started.reset();
    gate.reset();

    auto ahead = sta_loop::async_call([] { return 1; });
    task<counted_value> answer = start_gated_answer(started, gate);
    started.wait();

    answer = {};

    EXPECT_EQ(counted_value::live.load(), 1) << "the value went before its operation";

    EXPECT_EQ(sta_loop::run_until(ahead), 1);
    take_what_was_given_up();

    EXPECT_EQ(counted_value::live.load(), 0);
}

TEST(StaLoopAbandonTest, AValueOfAnOrphanGivenUpIsDestroyedOnceByTheThreadThatWinsTheRace) {
    counts_balanced counts;
    hevent passed{true};

    // Done before it is given up: the destructor of its task destroys the value.
    {
        auto opening = sta_loop::async_call(orphanable, [] { return counted_value{}; });
        auto barrier = sta_loop::async_call([&passed] { passed.set(); });

        passed.wait();

        EXPECT_EQ(counted_value::live.load(), 1);
    }

    EXPECT_EQ(counted_value::live.load(), 0);

    take_what_was_given_up();

    // Given up while it runs: the worker destroys the value as it is made.
    hevent started{true};
    hevent gate{true};

    {
        auto opening = sta_loop::async_call(orphanable, [&started, &gate] {
            started.set();
            gate.wait();
            return counted_value{};
        });

        started.wait();
    }

    gate.set();
    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

    EXPECT_EQ(counted_value::live.load(), 0);
}

TEST(StaLoopAbandonTest, AnErrorOfAnOrphanGivenUpIsReleasedAsItIsDeleted) {
    counts_balanced counts;

    // Given up before it starts, behind a read the worker is held inside: it never runs,
    // answers the cancellation, and is deleted with it, its value never made.
    {
        probe running;
        std::byte buf[16]{};
        std::atomic<bool> ran{false};
        std::atomic<int> deleted{0};

        auto k = start_read(running, buf);
        running.started.wait();

        {
            auto opening =
                sta_loop::async_call(orphanable, [&ran, counter = deletion_counter(deleted)] {
                    ran = true;
                    return counted_value{};
                });
        }

        running.gate.set();
        sta_loop::run_until(k);
        sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

        EXPECT_FALSE(ran.load());
        EXPECT_EQ(deleted.load(), 1);
    }

    // Given up while it runs, and answering it with a failure of its own: the failure is
    // released as the loop deletes it.
    hevent started{true};

    {
        auto opening = sta_loop::async_call(orphanable,
                                            [&started](const orphan_state& state) -> counted_value {
                                                started.set();

                                                while (!state.stop_requested())
                                                    std::this_thread::yield();

                                                throw counted_failure();
                                            });

        started.wait();
    }

    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

    EXPECT_EQ(counted_failure::live.load(), 0);
}
