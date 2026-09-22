// What one asynchronous operation costs through sta_loop against what it costs through
// the coroutine vocabulary WinRT ships with -- side by side, on one real
// DispatcherQueue pumped by the STA thread, the way a WinUI application's thread is.
//
// Three ways to run a body off the UI thread and come back with its answer:
//
//   sta_loop, driven    the loop in the shape an application starts it: the body goes to
//                       the one worker through the channel, and the worker's callback
//                       asks the DispatcherQueue to call run_pending() on the STA thread.
//                       A burst of finished operations is one post.
//   resume pair         written by hand from the two cppwinrt awaitables: co_await
//                       resume_background() moves the coroutine to the thread pool, the
//                       body runs there, co_await resume_foreground(queue) posts it back.
//                       One thread pool submission and one post per operation.
//   IAsyncOperation     what a framework call is: an IAsyncOperation<T> coroutine that
//                       moves itself to the pool and co_returns, awaited from the STA
//                       coroutine. cppwinrt completes it on the pool thread and resumes
//                       the awaiter in its own apartment through IContextCallback.
//
// The scenarios are those of sta_loop_benchmark.cpp, with the file left out:
//
//   ping-pong   one coroutine, one operation at a time -- the round trip, split into the
//               trip out and the trip back.
//   fan-out     1, 8 and 64 coroutines in flight together, each with its operations one
//               after another -- what amortises when there is more than one.
//
// Every row prints its proofs beside the time: posts to the dispatcher per operation,
// which the loop's own callback counts and which the two WinRT shapes make one per
// operation by construction; and how many times the global operator new was called per
// operation, counted on every thread in a pass of its own, because a scheme that
// allocates per operation pays that on the process heap whether or not it shows in the
// nanoseconds. The count is of C++ new only: what the runtime takes through HeapAlloc,
// and what wxl takes from the STA pool, is not in it.
//
// Usage: wxl.async.winrt-coroutine-benchmark [rounds]

// The winrt headers first, and with them the standard library they pull in textually:
// a standard header included after a module import is one MSVC has already seen
// through the std module.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <windows.h>

#include <DispatcherQueue.h>  // CreateDispatcherQueueController -- the OS queue, coremessaging.lib

#include <algorithm>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <numeric>
#include <thread>
#include <vector>

import wxl.core;
import wxl.async;

namespace {

using namespace wxl::async;
namespace ws = winrt::Windows::System;
namespace wf = winrt::Windows::Foundation;

using bench_clock = std::chrono::steady_clock;
using nanoseconds = std::chrono::duration<double, std::nano>;

// ---------------------------------------------------------------------------------------
// The proofs.

/// Posts the loop's callback made to the dispatcher. Bumped on the worker thread, read
/// on the STA thread after the round.
std::atomic<std::uint64_t> g_posts{0};

/// Calls of the global operator new on any thread, and whether they are being counted
/// right now: the counter is on for a pass of its own and off for the timed rounds, so
/// that its atomic is not in the numbers it is a proof for.
std::atomic<std::uint64_t> g_news{0};
std::atomic<bool> g_counting_news{false};

}  // namespace

// The ordinary allocator with the counter in front of it. Replacing the global pair
// changes nothing about where the memory comes from; it is the one way to see every
// allocation the WinRT shapes make -- their delegates, their async objects -- since
// those are made deep inside cppwinrt.
void* operator new(std::size_t size) {
    if (g_counting_news.load(std::memory_order_relaxed))
        g_news.fetch_add(1, std::memory_order_relaxed);

    if (void* const block = std::malloc(size != 0 ? size : 1)) return block;

    throw std::bad_alloc{};
}

void operator delete(void* block) noexcept { std::free(block); }

void operator delete(void* block, std::size_t) noexcept { std::free(block); }

namespace {

// ---------------------------------------------------------------------------------------
// The STA thread's message loop.

/// Run until the scenario says it is done. Everything that comes back to this thread
/// comes through here: the DispatcherQueue's posts and COM's calls into this apartment
/// both arrive as messages.
void pump_until(auto done) {
    MSG msg;

    while (!done()) {
        // WM_QUIT or an error: neither is sent here, and a round without its pump has
        // no number to print.
        if (::GetMessageW(&msg, nullptr, 0, 0) <= 0) std::abort();

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

// ---------------------------------------------------------------------------------------
// Where the time of one round trip goes: from the send to the body's first instruction
// off the thread, and from the body's last to the coroutine's next.

struct latency_samples {
    std::vector<double> out;   ///< STA send -> body, ns
    std::vector<double> back;  ///< body -> STA resumed, ns

    void reserve(std::size_t n) {
        out.reserve(n);
        back.reserve(n);
    }
};

struct summary {
    double mean;
    double median;
};

summary summarize(std::vector<double> samples) {
    if (samples.empty()) return {0, 0};

    std::sort(samples.begin(), samples.end());

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);

    return {sum / static_cast<double>(samples.size()), samples[samples.size() / 2]};
}

// ---------------------------------------------------------------------------------------
// The coroutines, three shapes each. The body is the same everywhere: a timestamp for the
// ping-pong, the operation's own number for the fan-out.

// sta_loop, driven: async_call() sends the body to the worker; run_pending(), called
// from the dispatcher, resumes the coroutine here.

task loop_ping(int count, latency_samples& samples, int& remaining) {
    for (int i = 0; i < count; ++i) {
        const bench_clock::time_point sent = bench_clock::now();
        const bench_clock::time_point seen =
            co_await sta_loop::async_call([] { return bench_clock::now(); });
        const bench_clock::time_point back = bench_clock::now();

        samples.out.push_back(nanoseconds(seen - sent).count());
        samples.back.push_back(nanoseconds(back - seen).count());
    }

    --remaining;
}

task loop_fan(int count, std::uint64_t& sum, int& remaining) {
    for (int i = 0; i < count; ++i)
        sum += co_await sta_loop::async_call([i] { return static_cast<std::uint64_t>(i); });

    --remaining;
}

// The resume pair: the coroutine itself goes to the thread pool and posts itself back.
// The queue is a parameter so that the frame holds the reference resume_foreground()
// keeps across the suspension.

winrt::fire_and_forget pair_ping(ws::DispatcherQueue queue, int count, latency_samples& samples,
                                 int& remaining) {
    for (int i = 0; i < count; ++i) {
        const bench_clock::time_point sent = bench_clock::now();
        co_await winrt::resume_background();
        const bench_clock::time_point seen = bench_clock::now();
        co_await winrt::resume_foreground(queue);
        const bench_clock::time_point back = bench_clock::now();

        samples.out.push_back(nanoseconds(seen - sent).count());
        samples.back.push_back(nanoseconds(back - seen).count());
    }

    --remaining;
}

winrt::fire_and_forget pair_fan(ws::DispatcherQueue queue, int count, std::uint64_t& sum,
                                int& remaining) {
    for (int i = 0; i < count; ++i) {
        co_await winrt::resume_background();
        const std::uint64_t value = static_cast<std::uint64_t>(i);
        co_await winrt::resume_foreground(queue);

        sum += value;
    }

    --remaining;
}

// IAsyncOperation: the body is an operation of its own, the shape every framework call
// has, and the STA coroutine awaits it. Completion happens on the pool thread; cppwinrt
// brings the awaiter back to this apartment itself.

wf::IAsyncOperation<std::int64_t> stamp_on_the_pool() {
    co_await winrt::resume_background();
    co_return bench_clock::now().time_since_epoch().count();
}

wf::IAsyncOperation<std::uint64_t> value_on_the_pool(std::uint64_t value) {
    co_await winrt::resume_background();
    co_return value;
}

winrt::fire_and_forget operation_ping(int count, latency_samples& samples, int& remaining) {
    for (int i = 0; i < count; ++i) {
        const bench_clock::time_point sent = bench_clock::now();
        const bench_clock::time_point seen{bench_clock::duration{co_await stamp_on_the_pool()}};
        const bench_clock::time_point back = bench_clock::now();

        samples.out.push_back(nanoseconds(seen - sent).count());
        samples.back.push_back(nanoseconds(back - seen).count());
    }

    --remaining;
}

winrt::fire_and_forget operation_fan(int count, std::uint64_t& sum, int& remaining) {
    for (int i = 0; i < count; ++i)
        sum += co_await value_on_the_pool(static_cast<std::uint64_t>(i));

    --remaining;
}

// ---------------------------------------------------------------------------------------
// The schemes, behind one pair of calls so that the rounds are written once. A `task` is
// kept and asked for its result at the end; a fire_and_forget hands back nothing to keep.

struct nothing_kept {
    void result() const noexcept {}
};

struct via_sta_loop {
    static constexpr const char* name = "sta_loop, driven";
    static constexpr bool posts_measured = true;

    using keeper = task;

    static keeper ping(ws::DispatcherQueue const&, int count, latency_samples& samples,
                       int& remaining) {
        return loop_ping(count, samples, remaining);
    }

    static keeper fan(ws::DispatcherQueue const&, int count, std::uint64_t& sum, int& remaining) {
        return loop_fan(count, sum, remaining);
    }
};

struct via_resume_pair {
    static constexpr const char* name = "resume_background/foreground";
    static constexpr bool posts_measured = false;

    using keeper = nothing_kept;

    static keeper ping(ws::DispatcherQueue const& queue, int count, latency_samples& samples,
                       int& remaining) {
        pair_ping(queue, count, samples, remaining);
        return {};
    }

    static keeper fan(ws::DispatcherQueue const& queue, int count, std::uint64_t& sum,
                      int& remaining) {
        pair_fan(queue, count, sum, remaining);
        return {};
    }
};

struct via_async_operation {
    static constexpr const char* name = "co_await IAsyncOperation";
    static constexpr bool posts_measured = false;

    using keeper = nothing_kept;

    static keeper ping(ws::DispatcherQueue const&, int count, latency_samples& samples,
                       int& remaining) {
        operation_ping(count, samples, remaining);
        return {};
    }

    static keeper fan(ws::DispatcherQueue const&, int count, std::uint64_t& sum, int& remaining) {
        operation_fan(count, sum, remaining);
        return {};
    }
};

// ---------------------------------------------------------------------------------------
// One round of each scenario.

struct ping_result {
    double round_trip_ns;  ///< mean per operation over the round
    summary out;
    summary back;
    double posts_per_op;
};

template <class scheme>
ping_result ping_round(ws::DispatcherQueue const& queue, int count) {
    latency_samples samples;
    samples.reserve(static_cast<std::size_t>(count));
    int remaining = 1;

    g_posts.store(0, std::memory_order_relaxed);

    const bench_clock::time_point started = bench_clock::now();
    typename scheme::keeper kept = scheme::ping(queue, count, samples, remaining);
    pump_until([&] { return remaining == 0; });
    const double per_op = nanoseconds(bench_clock::now() - started).count() / count;

    kept.result();

    return {per_op, summarize(samples.out), summarize(samples.back),
            double(g_posts.load(std::memory_order_relaxed)) / count};
}

struct fan_result {
    double per_op_ns;
    double posts_per_op;
};

template <class scheme>
fan_result fan_round(ws::DispatcherQueue const& queue, int tasks, int count) {
    const std::uint64_t expected =
        static_cast<std::uint64_t>(tasks) * (static_cast<std::uint64_t>(count) * (count - 1) / 2);

    std::vector<typename scheme::keeper> running;
    running.reserve(static_cast<std::size_t>(tasks));
    std::uint64_t sum = 0;
    int remaining = tasks;

    g_posts.store(0, std::memory_order_relaxed);

    const bench_clock::time_point started = bench_clock::now();

    for (int i = 0; i < tasks; ++i) running.push_back(scheme::fan(queue, count, sum, remaining));

    pump_until([&] { return remaining == 0; });

    const double per_op =
        nanoseconds(bench_clock::now() - started).count() / (double(tasks) * count);

    for (typename scheme::keeper& kept : running) kept.result();

    if (sum != expected) std::printf("  fan-out: checksum mismatch\n");

    return {per_op, double(g_posts.load(std::memory_order_relaxed)) / (double(tasks) * count)};
}

/// The allocation count of one round, taken with the counter on. It doubles as the
/// warm-up of the row: the first round pays for cold pages and for the thread pool
/// spinning up, and neither is what is being compared.
double news_per_op(auto round, double ops) {
    g_news.store(0, std::memory_order_relaxed);
    g_counting_news.store(true, std::memory_order_relaxed);

    round();

    g_counting_news.store(false, std::memory_order_relaxed);

    return double(g_news.load(std::memory_order_relaxed)) / ops;
}

// ---------------------------------------------------------------------------------------
// The rows.

struct settings {
    int ping_count;
    int fan_count;
    int rounds;
};

template <class scheme>
void ping_row(ws::DispatcherQueue const& queue, const settings& how) {
    const double news =
        news_per_op([&] { ping_round<scheme>(queue, how.ping_count); }, how.ping_count);

    ping_result best{1e30, {}, {}, 0};
    std::vector<double> trips;

    for (int round = 0; round < how.rounds; ++round) {
        const ping_result r = ping_round<scheme>(queue, how.ping_count);

        trips.push_back(r.round_trip_ns);

        if (r.round_trip_ns < best.round_trip_ns) best = r;
    }

    std::printf("  %-30s %8.2f %8.2f %9.2f %9.2f %9.2f %9.2f", scheme::name,
                best.round_trip_ns / 1000.0, summarize(trips).median / 1000.0,
                best.out.mean / 1000.0, best.out.median / 1000.0, best.back.mean / 1000.0,
                best.back.median / 1000.0);

    if constexpr (scheme::posts_measured)
        std::printf(" %9.3f", best.posts_per_op);
    else
        std::printf(" %9s", "1 (shape)");

    std::printf(" %8.2f\n", news);
}

template <class scheme>
void fan_row(ws::DispatcherQueue const& queue, int tasks, const settings& how) {
    const double ops = double(tasks) * how.fan_count;
    const double news = news_per_op([&] { fan_round<scheme>(queue, tasks, how.fan_count); }, ops);

    fan_result best{1e30, 0};

    for (int round = 0; round < how.rounds; ++round) {
        const fan_result r = fan_round<scheme>(queue, tasks, how.fan_count);

        if (r.per_op_ns < best.per_op_ns) best = r;
    }

    char label[64];
    std::snprintf(label, sizeof label, "%s, %d", scheme::name, tasks);

    std::printf("  %-34s %9.1f", label, best.per_op_ns);

    if constexpr (scheme::posts_measured)
        std::printf(" %9.3f", best.posts_per_op);
    else
        std::printf(" %9s", "1 (shape)");

    std::printf(" %8.2f\n", news);
}

}  // namespace

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 5;

    // The thread a WinUI application's coroutines run on: a single-threaded apartment
    // with a Windows.System.DispatcherQueue of its own, fed by the message loop above.
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    ws::DispatcherQueueController controller{nullptr};
    {
        DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT,
                                       DQTAT_COM_STA};
        ABI::Windows::System::IDispatcherQueueController* raw = nullptr;
        winrt::check_hresult(::CreateDispatcherQueueController(options, &raw));
        winrt::attach_abi(controller, raw);
    }
    const ws::DispatcherQueue queue = controller.DispatcherQueue();

    // The loop in its driven shape. The callback is the one TryEnqueue an application
    // writes, with the count in front of it; it runs on the worker's thread. TryEnqueue
    // refuses only a queue that is shutting down, and this one outlives the loop.
    sta_loop::start_driven(
        [queue]() noexcept {
            g_posts.fetch_add(1, std::memory_order_relaxed);

            if (!queue.TryEnqueue([] { sta_loop::run_pending(); })) std::abort();
        },
        "winrt benchmark: I/O");

    const settings how{.ping_count = 20'000, .fan_count = 2'000, .rounds = rounds};

    std::printf(
        "wxl.async sta_loop against the WinRT coroutine scheme -- one real DispatcherQueue "
        "pumped by the STA thread, best of %d rounds\n",
        rounds);
    std::printf("%u logical processors\n\n", std::thread::hardware_concurrency());

    std::printf(
        "ping-pong: one coroutine, %d operations one after another -- microseconds per round "
        "trip\n\n",
        how.ping_count);
    std::printf("  %-30s %8s %8s %9s %9s %9s %9s %9s %8s\n", "", "trip", "med rnd", "out mean",
                "out med", "back mean", "back med", "posts/op", "new/op");
    std::putchar('\n');

    ping_row<via_sta_loop>(queue, how);
    ping_row<via_resume_pair>(queue, how);
    ping_row<via_async_operation>(queue, how);

    std::printf(
        "\n\nfan-out: coroutines in flight together, %d operations each -- nanoseconds per "
        "operation\n\n",
        how.fan_count);
    std::printf("  %-34s %9s %9s %8s\n", "scheme, coroutines", "ns/op", "posts/op", "new/op");
    std::putchar('\n');

    for (const int tasks : {1, 8, 64}) {
        fan_row<via_sta_loop>(queue, tasks, how);
        fan_row<via_resume_pair>(queue, tasks, how);
        fan_row<via_async_operation>(queue, tasks, how);
        std::putchar('\n');
    }

    sta_loop::stop();

    std::printf(
        "  trip is the whole round trip; out is from the send to the first instruction of the\n"
        "  body off the STA thread, back from the last to the coroutine running again.\n"
        "  posts/op: how many times the dispatcher was asked to call the STA thread back, per\n"
        "  operation -- counted in the loop's own callback; the WinRT shapes make one per\n"
        "  operation by construction, a TryEnqueue for the pair and an IContextCallback for\n"
        "  the operation. new/op: calls of the global operator new per operation, on every\n"
        "  thread, in a counted pass of its own; coroutine frames are in it, the STA pool and\n"
        "  the runtime's HeapAlloc are not.\n");

    return 0;
}
