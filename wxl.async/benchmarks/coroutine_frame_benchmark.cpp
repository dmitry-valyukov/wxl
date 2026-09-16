// What one job costs when it gets a coroutine of its own, and what it costs when it
// borrows one that is already there. The question behind it: whether a pool of
// never-ending coroutines is worth its complexity, or whether a frame is cheap
// enough to be made and thrown away per job.
//
// The scenarios, all on one thread, all doing the same amount of coroutine work per
// job -- one suspension and one resumption -- so that what differs between them is
// only where the frame comes from:
//
//   1. fresh:  a coroutine per job, its frame from the ordinary allocator.
//   2. pooled memory: a coroutine per job, its frame from sta_memory_pool, through
//      promise_type::operator new.
//   3. both of those again with a kilobyte of locals alive across the suspension --
//      a frame the size a real job's would be, not the smallest one there is.
//   4. eternal: one coroutine that never ends, parked on a suspension, handed job
//      after job. The floor a pool of coroutines could reach -- and a floor no real
//      pool would stand on, because this one is resumed empty-handed. A parked
//      coroutine has a body written once and for all, so a real job would have to
//      arrive through a field of some erased type, and `core::function::create` is
//      a `new` of its own. Read the line as a lower bound that is out of reach, not
//      as what a pool would cost.
//
// Every run also reports how many frames actually reached an allocator, and that is
// not decoration. The compiler may elide the allocation whenever it can prove the
// frame dies inside the caller, and an elided frame lives on the stack and costs
// nothing to make: a run whose count is zero measures a scenario that is not there.
// The first version of this benchmark was exactly that -- every scenario elided,
// counters flat at zero, and the printed "cost of a frame" the cost of no frame at
// all. Frame size does not enter into it: a kilobyte of locals was elided just as
// readily as none. What decides it is whether the caller can see the whole life of
// the frame, so the ramps below are `noinline`: the frame has to outlive a call the
// optimiser will not open, which leaves nowhere to put it but the heap. That is also
// the truth in the real scheme, where the handle goes into a channel and comes back
// from another thread.
#include <chrono>
#include <coroutine>
#include <cstdio>

import wxl.core;

namespace {

using wxl::core::sta_memory_pool;

// What a job does: two calls on either side of the suspension, into a function the
// optimiser will not open. Every scenario pays exactly these two, so what is left
// between them is where the frame came from and how big it was.
//
// Opacity is the point of `touch`, not decoration. The wide jobs hand it their own
// array, and only because the array escapes into a call like this does the whole
// kilobyte have to live in the frame: left visible, MSVC keeps the two bytes the
// code really reads and the wide frame comes out sixteen bytes larger than the
// narrow one instead of a kilobyte.
volatile unsigned g_sink = 0;

/// What the jobs without a frame of their own hand to `touch`.
char g_scratch[1024];

__declspec(noinline) void touch(char* buffer, std::size_t size) {
    buffer[0] = char(g_sink);
    g_sink = unsigned(buffer[size - 1]) + 1;
}

// Frames that really were allocated, and how big the last one was. Both promise
// types bump the same pair, which is enough because the runs go one after another.
// The two instructions sit inside the timed loop; against the ten-odd nanoseconds
// being compared they are noise.
unsigned long long g_frames = 0;
std::size_t g_frame_size = 0;

/// The bare minimum that can be resumed and destroyed by its owner: our `task`
/// without the exception it keeps.
template <class promise_t>
struct handle_holder {
    std::coroutine_handle<promise_t> handle;

    ~handle_holder() {
        if (handle) handle.destroy();
    }
};

struct fresh_task {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct promise_type {
        fresh_task get_return_object() { return fresh_task{handle_t::from_promise(*this)}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}

        // The ordinary allocator with a counter around it. Declaring these changes
        // nothing about where the memory comes from, and it is the only way to tell
        // a frame that went to the heap from one the compiler kept on the stack.
        static void* operator new(std::size_t size) {
            ++g_frames;
            g_frame_size = size;
            return ::operator new(size);
        }
        static void operator delete(void* mem, std::size_t size) noexcept {
            ::operator delete(mem, size);
        }
    };

    handle_t handle;
};

struct pooled_task {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct promise_type {
        pooled_task get_return_object() { return pooled_task{handle_t::from_promise(*this)}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}

        // The frame is made and destroyed on the STA thread, like everything else in
        // this scheme, so the pool is where it belongs.
        static void* operator new(std::size_t size) {
            ++g_frames;
            g_frame_size = size;
            return sta_memory_pool::alloc(size);
        }
        static void operator delete(void* mem, std::size_t size) noexcept {
            sta_memory_pool::free(mem, size);
        }
    };

    handle_t handle;
};

__declspec(noinline) fresh_task fresh_job() {
    touch(g_scratch, sizeof g_scratch);

    co_await std::suspend_always{};

    touch(g_scratch, sizeof g_scratch);
}

/// The same, with a kilobyte of locals alive across the suspension -- a frame the
/// size a real job's would be, rather than the smallest one the compiler can make.
__declspec(noinline) fresh_task fresh_wide_job() {
    char locals[1024];
    touch(locals, sizeof locals);

    co_await std::suspend_always{};

    touch(locals, sizeof locals);
}

__declspec(noinline) pooled_task pooled_job() {
    touch(g_scratch, sizeof g_scratch);

    co_await std::suspend_always{};

    touch(g_scratch, sizeof g_scratch);
}

__declspec(noinline) pooled_task pooled_wide_job() {
    char locals[1024];
    touch(locals, sizeof locals);

    co_await std::suspend_always{};

    touch(locals, sizeof locals);
}

/// The eternal one: parks on a suspension, and every resumption is one more job.
__declspec(noinline) fresh_task eternal_worker() {
    for (;;) {
        touch(g_scratch, sizeof g_scratch);

        co_await std::suspend_always{};

        touch(g_scratch, sizeof g_scratch);
    }
}

struct measurement {
    double ns = 0;                  ///< per job
    unsigned long long frames = 0;  ///< frames the allocator was asked for
    std::size_t frame_size = 0;     ///< the size it was asked for, 0 if never
};

void reset_counters() {
    g_frames = 0;
    g_frame_size = 0;
}

measurement measured(std::chrono::steady_clock::time_point t0, unsigned count) {
    const auto t1 = std::chrono::steady_clock::now();

    return {std::chrono::duration<double, std::nano>(t1 - t0).count() / count, g_frames,
            g_frame_size};
}

measurement run_fresh(unsigned count) {
    reset_counters();
    const auto t0 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i) {
        fresh_task t = fresh_job();
        t.handle.resume();
        t.handle.destroy();
    }

    return measured(t0, count);
}

measurement run_pooled(unsigned count) {
    reset_counters();
    const auto t0 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i) {
        pooled_task t = pooled_job();
        t.handle.resume();
        t.handle.destroy();
    }

    return measured(t0, count);
}

measurement run_fresh_wide(unsigned count) {
    reset_counters();
    const auto t0 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i) {
        fresh_task t = fresh_wide_job();
        t.handle.resume();
        t.handle.destroy();
    }

    return measured(t0, count);
}

measurement run_pooled_wide(unsigned count) {
    reset_counters();
    const auto t0 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i) {
        pooled_task t = pooled_wide_job();
        t.handle.resume();
        t.handle.destroy();
    }

    return measured(t0, count);
}

measurement run_eternal(unsigned count) {
    fresh_task worker = eternal_worker();

    reset_counters();
    const auto t0 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i) worker.handle.resume();

    const measurement result = measured(t0, count);

    worker.handle.destroy();

    return result;
}

void report(const char* what, const measurement& m, const measurement& floor, unsigned count) {
    std::printf("%-36s %10.2f %12.2f %12.2f %10zu\n", what, m.ns, m.ns - floor.ns,
                double(m.frames) / count, m.frame_size);
}

}  // namespace

int main() {
    constexpr unsigned count = 5'000'000;

    // A warm-up run of each: the first pass pays for cold pages and a cold branch
    // predictor, and neither is what is being compared.
    run_fresh(count / 10);
    run_pooled(count / 10);
    run_fresh_wide(count / 10);
    run_pooled_wide(count / 10);
    run_eternal(count / 10);

    const measurement fresh = run_fresh(count);
    const measurement pooled = run_pooled(count);
    const measurement wide = run_fresh_wide(count);
    const measurement pooled_wide = run_pooled_wide(count);
    const measurement eternal = run_eternal(count);

    std::printf("%-36s %10s %12s %12s %10s\n", "one job", "ns", "vs eternal", "frames/job",
                "frame, B");
    report("a fresh coroutine (new/delete)", fresh, eternal, count);
    report("a fresh coroutine (sta pool)", pooled, eternal, count);
    report("a fresh coroutine, 1K of locals", wide, eternal, count);
    report("a pooled coroutine, 1K of locals", pooled_wide, eternal, count);
    report("an eternal coroutine, resumed", eternal, eternal, count);

    return 0;
}
