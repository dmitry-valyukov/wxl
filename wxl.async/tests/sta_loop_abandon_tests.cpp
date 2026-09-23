// What happens to an asynchronous operation whose awaitable goes away while the operation
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

awaitable<std::size_t> start_read(probe& p, std::span<std::byte> into, hevent* opens = nullptr) {
    return sta_loop::async_run(
        std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into, opens)));
}

awaitable<void> start_failure() {
    return sta_loop::async_call([] { throw std::runtime_error("the first read failed"); });
}

/// The code from the discussion: two reads started, the first one awaited fails, and the
/// frame unwinds with the second still on the worker, writing into `second_buf`.
task first_fails_second_in_flight(probe& p) {
    frame_witness witness(p);
    std::byte second_buf[64]{};

    auto a = start_failure();
    auto b = start_read(p, second_buf);

    co_await a;
    co_await b;
}

/// Awaited in the order opposite to the one they were started in: `a` comes back while the
/// coroutine is suspended on `b`, with nobody waiting for it yet.
task awaits_in_the_opposite_order(int& first, int& second) {
    auto a = sta_loop::async_call([] { return 1; });
    auto b = sta_loop::async_call([] { return 2; });

    second = co_await b;
    first = co_await a;
}

/// Starts a read and walks away without ever awaiting it.
task starts_and_walks_away(probe& p) {
    frame_witness witness(p);
    std::byte buf[16]{};

    auto read = start_read(p, buf);

    co_return;
}

/// The worker is held inside `k` while `a` fails, so `b` is still queued behind it when the
/// frame gives it up; cancelling `b` is what lets `k` go.
task fails_with_one_running_and_one_queued(probe& running, probe& queued) {
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

task returns_seven(int& out) {
    out = co_await sta_loop::async_call([] { return 7; });
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
task fails_while_another_coroutine_is_answered(probe& running, probe& queued,
                                              std::vector<task>& others, int& other_out,
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
task reads_into_its_frame(probe& p) {
    frame_witness witness(p);
    std::byte buf[64]{};

    co_await start_read(p, buf);

    ADD_FAILURE() << "resumed after its task was dropped";
}

/// Many reads out at once and the first awaited one fails: every one of them is given up
/// in the same unwinding, in the order a vector destroys its elements -- the order the
/// operations went out in, and so the one in which a walk from the head of the return
/// channel would start again for every one of them.
task fails_with_many_reads_out(probe& p, std::size_t count) {
    frame_witness witness(p);
    std::vector<std::array<std::byte, 16>> buffers(count);
    std::vector<awaitable<std::size_t>> reads;

    reads.reserve(count);

    auto first = start_failure();

    for (auto& buffer : buffers) reads.push_back(start_read(p, buffer));

    co_await first;

    for (auto& read : reads) co_await read;
}

/// What an operation that opens something hands back: a resource of its own, released by
/// whoever ends up holding it -- for an orphan, the loop, on the STA thread.
class handle_like
{
public:
    handle_like(std::atomic<int>& released, std::thread::id& released_on)
        : released_(&released), released_on_(&released_on) {}

    handle_like(handle_like&& other) noexcept
        : released_(std::exchange(other.released_, nullptr)), released_on_(other.released_on_) {}

    handle_like& operator=(handle_like&&) = delete;

    ~handle_like() {
        if (!released_) return;

        *released_on_ = std::this_thread::get_id();
        ++*released_;
    }

private:
    std::atomic<int>* released_;
    std::thread::id* released_on_;
};

/// Starts an operation that touches nothing of the frame, and gives it up before awaiting
/// -- once the worker is inside it, so that it has something to finish alone.
task opens_and_changes_its_mind(hevent& started, hevent& gate, std::atomic<int>& released,
                                std::thread::id& released_on, bool changes_its_mind) {
    auto opening = sta_loop::async_call(orphanable, [&started, &gate, &released, &released_on] {
        started.set();
        gate.wait();
        return handle_like(released, released_on);
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
task queues_an_orphan_and_changes_its_mind(probe& running, std::atomic<bool>& orphan_ran,
                                           std::atomic<int>& orphan_deleted,
                                           bool changes_its_mind) {
    frame_witness witness(running);
    std::byte buf[16]{};

    auto k = start_read(running, buf);
    auto opening = sta_loop::async_call(
        orphanable, [&orphan_ran, counter = deletion_counter(orphan_deleted)] {
            orphan_ran = true;
            return 0;
        });

    running.started.wait();

    if (changes_its_mind) throw std::runtime_error("changed its mind");

    co_await opening;
    co_await k;
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
    task work = first_fails_second_in_flight(p);

    // The worker is inside the second read before the first one is looked at, so the
    // unwinding below finds it running rather than queued.
    p.started.wait();

    sta_loop::run_until([&] { return work.done(); });

    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(p.canceled.load(), 1) << "the operation was never told it had been given up";
    EXPECT_EQ(p.ran.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOperationBackBeforeItsCoAwaitIsTakenThereWithoutAResume) {
    int first = 0;
    int second = 0;
    task work = awaits_in_the_opposite_order(first, second);

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
}

TEST(StaLoopAbandonTest, AnOperationNeverAwaitedIsWaitedForByTheFrameThatStartedIt) {
    probe p;
    task work = starts_and_walks_away(p);

    // The whole body ran inside the call, the wait included: the coroutine never suspended.
    EXPECT_TRUE(work.done());
    work.result();

    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran.load(), p.ran_with_frame_alive.load()) << "the worker wrote into a frame that was gone";
    EXPECT_FALSE(p.frame_alive.load());

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOperationGivenUpBeforeItStartsNeverRuns) {
    probe running;
    probe queued;
    task work = fails_with_one_running_and_one_queued(running, queued);

    // The worker is inside `k`, so `b` is behind it and cannot have started.
    running.started.wait();

    sta_loop::run_until([&] { return work.done(); });

    EXPECT_THROW(work.result(), std::runtime_error);
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
    std::vector<task> others;
    int other_out = 0;
    int other_out_while_unwinding = -1;

    task work = fails_while_another_coroutine_is_answered(running, queued, others, other_out,
                                                          other_out_while_unwinding);
    running.started.wait();

    sta_loop::run_until([&] { return work.done(); });

    EXPECT_THROW(work.result(), std::runtime_error);
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
        task work = reads_into_its_frame(p);

        p.started.wait();

        EXPECT_FALSE(work.done());
    }

    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";
    EXPECT_FALSE(p.frame_alive.load());

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, ManyOperationsGivenUpInOneUnwindingAllWaitForTheWorker) {
    constexpr std::size_t count = 64;

    probe p;
    task work = fails_with_many_reads_out(p, count);

    sta_loop::run_until([&] { return work.done(); });

    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(p.ran.load(), p.ran_with_frame_alive.load()) << "the worker wrote into a frame that was gone";

    take_what_was_given_up();

    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopAbandonTest, AnOrphanableOperationIsNotWaitedForAndReleasesWhatItBrings) {
    hevent started{true};
    hevent gate{true};
    std::atomic<int> released{0};
    std::thread::id released_on;

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

    task work = opens_and_changes_its_mind(started, gate, released, released_on, true);

    unwound.release();
    watchdog.join();

    EXPECT_FALSE(had_to_wait.load()) << "the frame waited for an operation that touches nothing of it";
    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(released.load(), 0);

    // The orphan runs to its end on its own, comes back, and what it brought is released
    // by the loop as it takes it -- on this thread.
    gate.set();

    while (released.load() == 0) sta_loop::run_one();

    EXPECT_EQ(released.load(), 1);
    EXPECT_EQ(released_on, std::this_thread::get_id());
}

TEST(StaLoopAbandonTest, AnOrphanGivenUpBeforeItStartsIsNeverRunAndStillDeleted) {
    probe running;
    std::atomic<bool> orphan_ran{false};
    std::atomic<int> orphan_deleted{0};

    task work = queues_an_orphan_and_changes_its_mind(running, orphan_ran, orphan_deleted, true);

    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(running.ran_with_frame_alive.load(), 1);

    // The worker skips the orphan and hands it back; the loop deletes it as it takes it.
    while (orphan_deleted.load() == 0) sta_loop::run_one();

    EXPECT_FALSE(orphan_ran.load()) << "an operation given up before it started was run";
    EXPECT_EQ(orphan_deleted.load(), 1);
    EXPECT_EQ(running.alive.load(), 0);
}
