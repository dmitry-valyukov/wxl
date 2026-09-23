// The driven shape of sta_loop, tested where it can be: a loop starts once per process
// and cannot be restarted, wxl.async.tests starts the sleeping shape, so the shape a GUI
// application runs -- start_driven(), a callback into a dispatcher, run_pending() -- has
// a test binary of its own.
#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// The dispatcher of a thread with a message loop, reduced to what the loop asks of it:
/// a callback arrives on the worker's thread, is counted, and wakes the thread that
/// drains -- which here is the test itself, sleeping on an event the way a message loop
/// sleeps on its queue. Auto-reset, so a burst of callbacks is one wakeup, which is
/// exactly what a dispatcher makes of them; the count is what tells them apart.
class fake_dispatcher
{
public:
    void post() noexcept {
        posts_.fetch_add(1, std::memory_order_relaxed);
        posted_.set();
    }

    void wait_for_a_post() { posted_.wait(); }

    int posts() const noexcept { return posts_.load(std::memory_order_relaxed); }

private:
    std::atomic<int> posts_{0};
    hevent posted_{false};
};

fake_dispatcher& dispatcher() {
    static fake_dispatcher instance;
    return instance;
}

/// The pool and the driven loop, both standing for the whole binary.
class driven_environment : public ::testing::Environment
{
public:
    void SetUp() override {
        sta_loop::start_driven([]() noexcept { dispatcher().post(); }, "wxl driven tests: I/O");
    }

    void TearDown() override { sta_loop::stop(); }
};

inline ::testing::Environment* const driven_env =
    ::testing::AddGlobalTestEnvironment(new driven_environment);

/// One operation whose body says, on the worker, that it has run -- so a test can wait
/// for a whole burst to have been executed before it lets the STA side look.
task counted_operation(std::latch& executed, int& out) {
    out = co_await sta_loop::async_call([&executed] {
        executed.count_down();
        return 1;
    });
}

task failing_operation(std::string& message) {
    try {
        co_await sta_loop::async_call([] { throw std::runtime_error("from the worker"); });
        message = "no exception";
    } catch (const std::exception& e) {
        message = e.what();
    }
}

/// What a read below saw and did, kept outside the operation: a given-up operation is
/// deleted by the loop whenever its turn comes, and the test looks afterwards.
struct probe {
    hevent gate{true};
    hevent started{true};
    std::atomic<bool> frame_alive{false};
    std::atomic<int> ran{0};
    std::atomic<int> ran_with_frame_alive{0};
    std::atomic<int> canceled{0};
    std::atomic<int> alive{0};
};

/// Alive while the frame is; opens the gate on the way out, so that a loop which let the
/// frame go first fails the test rather than leaving the worker asleep.
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

/// A read held at a gate, writing into the frame if the frame is there; cancelling it
/// opens the gate, the way CancelIoEx completes a read the kernel is holding.
class gated_read : public async_op_t<std::size_t>
{
public:
    gated_read(probe& p, std::span<std::byte> into) : p_(p), into_(into) { ++p_.alive; }

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
        p_.gate.set();
    }

private:
    probe& p_;
    std::span<std::byte> into_;
};

awaitable<std::size_t> start_read(probe& p, std::span<std::byte> into) {
    return sta_loop::async_run(std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into)));
}

/// Two reads out, the first awaited one fails, the frame unwinds with the second on the
/// worker -- inside run_pending(), which is where a GUI thread resumes its coroutines.
task first_fails_second_in_flight(probe& p) {
    frame_witness witness(p);
    std::byte second_buf[64]{};

    auto a = sta_loop::async_call([] { throw std::runtime_error("the first read failed"); });
    auto b = start_read(p, second_buf);

    co_await a;
    co_await b;
}

task reads_into_its_frame(probe& p) {
    frame_witness witness(p);
    std::byte buf[64]{};

    co_await start_read(p, buf);
}

task returns_seven(int& out) {
    out = co_await sta_loop::async_call([] { return 7; });
}

bool all_done(const std::vector<task>& work) {
    return std::all_of(work.begin(), work.end(), [](const task& t) { return t.done(); });
}

/// Drains the way an application's callback does: run_pending() on every post, until
/// the work is done.
void drain_until_done(const std::vector<task>& work) {
    while (!all_done(work)) {
        dispatcher().wait_for_a_post();
        sta_loop::run_pending();
    }
}

}  // namespace

TEST(StaLoopDrivenTest, RunPendingWithNothingBehindItReturnsAtOnce) {
    EXPECT_EQ(sta_loop::run_pending(), 0u);
}

TEST(StaLoopDrivenTest, ABurstFinishedBeforeTheStaSideLooksCostsOneCallbackNotOneEach) {
    // Sixty-four operations, every one of them executed on the worker before the STA
    // side drains anything: the latch is counted down by the bodies, and this thread
    // does not look at the return channel until all of them have counted. Without the
    // coalescing every handover would have called the dispatcher back -- sixty-four
    // posts. With it, the first handover posts, the rest find the trigger clear.
    //
    // One straggler is allowed: the last body counts the latch down *before* its
    // operation is handed back, so the drain below may run between the two, re-arm the
    // trigger, and be called back once more for that one operation. Two at the most,
    // therefore -- and sixty-four without the protocol.
    constexpr int burst = 64;

    const int posts_before = dispatcher().posts();
    std::latch executed(burst);
    int results[burst]{};
    std::vector<task> work;

    for (int i = 0; i < burst; ++i) work.push_back(counted_operation(executed, results[i]));

    executed.wait();

    drain_until_done(work);

    for (task& t : work) t.result();
    for (const int r : results) EXPECT_EQ(r, 1);

    const int posts = dispatcher().posts() - posts_before;

    EXPECT_GE(posts, 1);
    EXPECT_LE(posts, 2) << "a burst of " << burst << " operations cost " << posts << " callbacks";
}

TEST(StaLoopDrivenTest, TheTriggerIsArmedAgainAfterEveryDrain) {
    // Each of these runs alone, so each has to be posted about: a drain that forgot to
    // re-arm the trigger would leave the next operation finished and nobody told -- and
    // this test would hang in wait_for_a_post() rather than fail.
    //
    // At most two, not exactly one: a callback with nothing behind it is inside the
    // contract, the way a spurious wakeup is inside a sleeper's. It happens when this
    // side drains on a leftover wakeup a moment before the worker reaches its own
    // disarm(), which then finds the trigger armed again and calls back once more.
    for (int i = 0; i < 3; ++i) {
        const int posts_before = dispatcher().posts();
        std::latch executed(1);
        int result = 0;
        std::vector<task> work;

        work.push_back(counted_operation(executed, result));

        drain_until_done(work);
        work.front().result();

        const int posts = dispatcher().posts() - posts_before;

        EXPECT_EQ(result, 1);
        EXPECT_GE(posts, 1);
        EXPECT_LE(posts, 2);
    }
}

TEST(StaLoopDrivenTest, AnExceptionFromTheWorkerArrivesAtTheCoAwait) {
    std::string message;
    std::vector<task> work;

    work.push_back(failing_operation(message));

    drain_until_done(work);
    work.front().result();

    EXPECT_EQ(message, "from the worker");
}

TEST(StaLoopDrivenTest, AFrameUnwindingInsideRunPendingWaitsInPlaceNotThroughTheDispatcher) {
    // The unwinding happens inside run_pending(), on a thread whose only way back to its
    // dispatcher is to return -- which it cannot do until the worker has let go of the
    // frame. So the handover it waits for has to wake it in place: a callback posted to
    // the dispatcher instead would leave this test asleep for good.
    probe p;
    std::vector<task> work;

    work.push_back(first_fails_second_in_flight(p));
    p.started.wait();

    drain_until_done(work);

    EXPECT_THROW(work.front().result(), std::runtime_error);
    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";
    EXPECT_EQ(p.alive.load(), 0);
}

TEST(StaLoopDrivenTest, GivingUpAnOperationOutsideRunPendingKeepsTheCallbackTheLoopOwes) {
    // Dropped between two drains, where the trigger is armed: the next handover owes the
    // dispatcher a callback. The wait takes that debt over -- it has to, or the handovers it
    // waits for would go to a dispatcher nobody is running -- and must pay it on the way
    // out, or the operation queued behind the read comes back to nobody.
    probe p;
    int seven = 0;
    std::vector<task> others;
    const int posts_before = dispatcher().posts();

    {
        task dropped = reads_into_its_frame(p);

        p.started.wait();
        others.push_back(returns_seven(seven));
    }

    EXPECT_EQ(p.canceled.load(), 1);
    EXPECT_EQ(p.ran_with_frame_alive.load(), 1) << "the worker wrote into a frame that was gone";

    drain_until_done(others);
    others.front().result();

    EXPECT_EQ(seven, 7);
    EXPECT_GE(dispatcher().posts() - posts_before, 1);
    EXPECT_EQ(p.alive.load(), 0);
}
