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
