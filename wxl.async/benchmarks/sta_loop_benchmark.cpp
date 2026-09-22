// What one co_await costs through the loop -- the whole round trip from the STA thread
// to the worker and back, measured end to end -- and then taken apart along the axes a
// caller could change: how the two threads are woken, where the operation lives, whether
// the worker looks before it sleeps, what priority it runs at, and how the driven shape
// tells its dispatcher.
//
// sta_loop itself is static and cannot be built twice, so the variants are a copy of it
// as an object, `loop_t<config>`, with the same two channels, the same worker loop and
// the same run_one/run_pending. The first row of every table is the real sta_loop and
// the second is the copy at its production settings: the two have to agree before any
// other row means anything, exactly as the allocator study kept a byte-for-byte copy of
// the pool beside it.
//
// The scenarios, each answering one question:
//
//   ping-pong   one coroutine, one operation at a time. Both threads sleep and are woken
//               for every operation, so this is the price of the wake path, split into
//               the trip out and the trip back.
//   fan-out     many coroutines in flight. The wakes amortize over a batch and what is
//               left is the lock-free machinery: the send, the op, the resume -- and the
//               tail of the wakes that still happen.
//   floor       the same fan-out with nobody sleeping: the machinery alone, and the only
//               regime in which a ten-nanosecond difference can be seen.
//   file        a cached file read through the loop in chunks, against the same reads on
//               the calling thread. What the loop adds to a real operation.
//   driven      the shape a GUI application runs: the STA thread is told through a
//               dispatcher and drains with run_pending(). Posts per operation is the
//               number, since every post is a real work item on the UI thread.
//
// Every row prints how many operations it executed and how many kernel wakes it cost,
// because a number without that proof measures whatever the optimiser left of the
// scenario rather than the scenario.
//
// Usage: wxl.async.sta-loop-benchmark [rounds [pin|nopin [ping|fan|floor|file|driven]]]
//        "pin" puts the STA thread on cpu 0 and every variant's worker on cpu 2, to tell
//        the mechanism from the scheduler; the third argument runs one table alone.

#include <windows.h>  // WaitOnAddress / WakeByAddressSingle -- the address wait the "address" rows stand on

import std;
import wxl.core;
import wxl.async;

namespace {

using namespace wxl::async;
using wxl::core::cpu_pause;

using bench_clock = std::chrono::steady_clock;
using nanoseconds = std::chrono::duration<double, std::nano>;

// ---------------------------------------------------------------------------------------
// The signals: what a thread sleeps on, and what wakes it.

/// The production wake, a kernel event: one syscall to set, one to wait. Auto-reset, so
/// a wake means "look again", the same contract every signal below keeps.
class event_signal
{
public:
    void set() noexcept { event_.set(); }
    void wait() { event_.wait(); }

private:
    wxl::core::hevent event_{false};
};

/// The address wait Windows 8 added under the name of a futex: the waiter compares the
/// word in user mode before it sleeps, and the waker looks for a registered waiter in
/// user mode and enters the kernel only when there is one. What is asked here is whether
/// that path is any shorter than the event's when a thread really has to be woken.
class address_signal
{
public:
    void set() noexcept {
        state_.store(1, std::memory_order_release);
        ::WakeByAddressSingle(&state_);
    }

    void wait() noexcept {
        // Consume a wake already there, otherwise sleep until one arrives -- the
        // condition-variable contract, the same as the auto-reset event's.
        std::uint32_t zero = 0;

        while (state_.exchange(0, std::memory_order_acquire) == 0)
            ::WaitOnAddress(&state_, &zero, sizeof state_, INFINITE);
    }

private:
    std::atomic<std::uint32_t> state_{0};
};

/// Counts the sets on the way through. That is the proof every table wants: a signal is
/// a syscall, and how many of them a scenario cost is what the wake protocol is for.
template <class inner_t>
class counted_signal
{
public:
    void set() noexcept {
        sets_.fetch_add(1, std::memory_order_relaxed);
        inner_.set();
    }

    void wait() { inner_.wait(); }

    std::uint64_t sets() const noexcept { return sets_.load(std::memory_order_relaxed); }

    void reset_count() noexcept { sets_.store(0, std::memory_order_relaxed); }

private:
    inner_t inner_;
    std::atomic<std::uint64_t> sets_{0};
};

// ---------------------------------------------------------------------------------------
// Where the operation lives.

/// In the pool behind a unique_ptr, as production has it; in the coroutine frame, as the
/// awaitable's own member; or in the frame with a cache line of padding on either side,
/// so that nothing else the coroutine touches shares a line with what the worker writes.
/// The third exists to tell the cost of the placement from the cost of the sharing.
enum class op_home
{
    pool,
    frame,
    frame_padded
};

/// The awaitable and the operation as one object: it is built straight into the frame of
/// the coroutine that awaits it, and the channels carry a pointer into that frame. One
/// pool allocation per operation less, one indirection less -- and the send moves to
/// await_suspend(), where the object's address is final and the coroutine to resume is
/// known, so an awaitable that is never awaited never runs and never dangles.
template <class Fn, class loop_t, bool padded>
class inline_awaitable
{
    using op_t = async_op_f<Fn>;
    using result_t = std::invoke_result_t<Fn&>;

    struct pad {
        std::byte bytes[std::hardware_destructive_interference_size];
    };

    struct no_pad {
    };

    using pad_t = std::conditional_t<padded, pad, no_pad>;

public:
    template <class Fn2>
    inline_awaitable(loop_t& loop, Fn2&& fn) : op_(std::forward<Fn2>(fn)), loop_(&loop) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> coro) noexcept {
        op_.suspend(coro);
        loop_->enqueue(&op_);
    }

    result_t await_resume() { return op_.take_result(); }

private:
    pad_t front_;
    op_t op_;
    pad_t back_;
    loop_t* loop_;
};

// ---------------------------------------------------------------------------------------
// The loop, as an object.

/// One knob per axis. `spin_ns` is how long the worker looks at its channel before it
/// sleeps -- zero is production, and anything else is the rule of the tree being bent
/// on purpose, to see what it would buy. `force_post` is the driven shape's second
/// signal per operation, the one run_pending's own protocol makes unnecessary.
template <class signal_t, int spin_ns_, bool force_post_, int priority_ = THREAD_PRIORITY_NORMAL>
struct config {
    using to_worker_t = spsc_channel<async_op*, 256, counted_signal<signal_t>>;
    using from_worker_t = spsc_channel<async_op*, 256, counted_signal<signal_t>>;

    static constexpr int spin_ns = spin_ns_;
    static constexpr bool force_post = force_post_;
    static constexpr int priority = priority_;
};

/// Where the two threads run, when the caller pinned them. Production pins nothing, so
/// the default is nothing; pinning is for telling the mechanism from the scheduler.
constexpr std::size_t unpinned = static_cast<std::size_t>(-1);
std::size_t g_sta_cpu = unpinned;
std::size_t g_worker_cpu = unpinned;

template <class config_t>
class loop_t : public wxl::core::noncopyable
{
    using to_worker_t = typename config_t::to_worker_t;
    using from_worker_t = typename config_t::from_worker_t;

public:
    loop_t() : from_worker_reader_(from_worker_), worker_([this] { run(); }) {}

    ~loop_t() {
        to_worker_.close();
        to_worker_.signal(true);
        worker_.join();
    }

    void enqueue(async_op* op) {
        to_worker_.send(op);
        sent_.fetch_add(1, std::memory_order_relaxed);
    }

    /// The production shape: the op from the pool, the awaitable owning it by pointer,
    /// and the send made before the awaitable exists.
    template <class Fn>
    [[nodiscard]] awaitable<std::invoke_result_t<std::decay_t<Fn>&>> async_call(Fn&& fn) {
        using result_t = std::invoke_result_t<std::decay_t<Fn>&>;

        std::unique_ptr<async_op_t<result_t>> op(
            new async_op_f<std::decay_t<Fn>>(std::forward<Fn>(fn)));

        enqueue(op.get());

        return awaitable<result_t>(std::move(op));
    }

    /// The other shape: the op in the frame, sent when the coroutine suspends on it.
    template <bool padded, class Fn>
    [[nodiscard]] inline_awaitable<std::decay_t<Fn>, loop_t, padded> async_inline(Fn&& fn) {
        return inline_awaitable<std::decay_t<Fn>, loop_t, padded>(*this, std::forward<Fn>(fn));
    }

    bool run_one() {
        async_op* op = nullptr;

        if (!from_worker_reader_.receive(op)) return false;

        op->resume();
        return true;
    }

    /// run_one() without the sleep: for the floor table, where nobody is allowed to.
    bool try_run_one() {
        async_op* op = nullptr;

        if (!from_worker_reader_.read(op)) return false;

        op->resume();
        return true;
    }

    void run_until(auto done) {
        while (!done()) run_one();
    }

    /// run_pending() as sta_loop's sleeping shape has it: takes what is there and leaves.
    std::size_t run_pending() {
        std::size_t resumed = 0;

        for (async_op* op = nullptr; from_worker_reader_.read(op); ++resumed) op->resume();

        return resumed;
    }

    /// The same, running the sleeping shape's own protocol at the end: having found the
    /// channel empty, the STA side declares itself not looking, and the next send is the
    /// one that posts. A burst of finished operations then costs one post, not one each.
    ///
    /// It opens by taking the trigger back, because a post may find it still set -- the
    /// worker posts on the first send after the declaration, and a second post can
    /// arrive with nothing behind it -- and the drain below has to be ordered after that
    /// exchange the same way a sleeper's re-check is ordered after its arm().
    std::size_t run_pending_coalesced() {
        std::size_t resumed = 0;

        from_worker_.disarm();
        std::atomic_thread_fence(std::memory_order_seq_cst);

        for (;;) {
            for (async_op* op = nullptr; from_worker_reader_.read(op); ++resumed) op->resume();

            from_worker_.arm();
            std::atomic_thread_fence(std::memory_order_seq_cst);

            async_op* op = nullptr;

            if (!from_worker_reader_.read(op)) return resumed;

            // Something arrived between the drain and the declaration: take the right to
            // be woken back -- unless the worker already took it, in which case a post is
            // on its way and will find nothing, which is the ordinary spurious wakeup.
            from_worker_.disarm();
            op->resume();
            ++resumed;
        }
    }

    /// The driven shape's opening move: nobody is looking until the first post.
    void arm() { from_worker_.arm(); }

    from_worker_t& from_worker() noexcept { return from_worker_; }

    to_worker_t& to_worker() noexcept { return to_worker_; }

    std::uint64_t sent() const noexcept { return sent_.load(std::memory_order_relaxed); }

    std::uint64_t executed() const noexcept { return executed_.load(std::memory_order_relaxed); }

    void reset_counts() noexcept {
        sent_.store(0, std::memory_order_relaxed);
        executed_.store(0, std::memory_order_relaxed);
        to_worker_.wakeup().reset_count();
        from_worker_.wakeup().reset_count();
    }

private:
    /// sta_loop's worker loop, with the spin window in front of the receive when the
    /// config asks for one. closed() is asked on the empty path only, as production does:
    /// close() forces a wakeup, and that wakeup comes back through receive() empty.
    void run() {
        if (g_worker_cpu != unpinned) thread::set_affinity(g_worker_cpu);
        if (config_t::priority != THREAD_PRIORITY_NORMAL)
            ::SetThreadPriority(::GetCurrentThread(), config_t::priority);

        typename to_worker_t::reader reader(to_worker_);
        async_op* op = nullptr;

        for (;;) {
            if constexpr (config_t::spin_ns > 0) {
                const bench_clock::time_point until =
                    bench_clock::now() + std::chrono::nanoseconds(config_t::spin_ns);
                bool found = false;

                while (bench_clock::now() < until) {
                    if (reader.read(op)) {
                        found = true;
                        break;
                    }

                    cpu_pause();
                }

                if (found) {
                    execute(op);
                    continue;
                }
            }

            if (reader.receive(op)) {
                execute(op);
                continue;
            }

            if (to_worker_.closed()) break;
        }

        while (reader.read(op)) execute(op);
    }

    void execute(async_op* op) {
        executed_.fetch_add(1, std::memory_order_relaxed);

        if (!op->packaged_execute()) return;

        from_worker_.send(op);

        if constexpr (config_t::force_post) from_worker_.signal(true);
    }

    to_worker_t to_worker_;
    from_worker_t from_worker_;
    typename from_worker_t::reader from_worker_reader_;

    std::atomic<std::uint64_t> sent_{0};
    std::atomic<std::uint64_t> executed_{0};

    std::thread worker_;
};

/// The real thing, behind the same calls the copy answers to, so the scenarios are
/// written once. Its signals cannot be counted from outside, which the tables say.
class production_loop : public wxl::core::noncopyable
{
public:
    void enqueue(async_op* op) { sta_loop::enqueue(wxl::core::as_not_null<async_op>(op)); }

    template <class Fn>
    [[nodiscard]] auto async_call(Fn&& fn) {
        return sta_loop::async_call(std::forward<Fn>(fn));
    }

    template <bool padded, class Fn>
    [[nodiscard]] inline_awaitable<std::decay_t<Fn>, production_loop, padded> async_inline(
        Fn&& fn) {
        return inline_awaitable<std::decay_t<Fn>, production_loop, padded>(*this,
                                                                           std::forward<Fn>(fn));
    }

    bool run_one() { return sta_loop::run_one(); }

    void run_until(auto done) { sta_loop::run_until(done); }
};

/// Starts an operation where `where` says: one spelling at the await sites, three
/// placements behind it. Returns a prvalue all the way up, which is what lets the
/// frame-resident awaitable -- an object that cannot be moved -- be built in place.
template <op_home where, class loop_t, class Fn>
[[nodiscard]] auto start(loop_t& loop, Fn&& fn) {
    if constexpr (where == op_home::pool)
        return loop.async_call(std::forward<Fn>(fn));
    else
        return loop.template async_inline<where == op_home::frame_padded>(std::forward<Fn>(fn));
}

// ---------------------------------------------------------------------------------------
// The coroutines the scenarios run.

/// Where the time of one round trip goes: from the send to the worker's first
/// instruction of the body, and from the body's last to the coroutine's next.
struct latency_samples {
    std::vector<double> out;   ///< STA send -> worker body, ns
    std::vector<double> back;  ///< worker body -> STA resumed, ns

    void reserve(std::size_t n) {
        out.reserve(n);
        back.reserve(n);
    }

    void clear() {
        out.clear();
        back.clear();
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

template <class loop_t, op_home where>
task ping_pong(loop_t& loop, int count, latency_samples& samples, int& remaining) {
    for (int i = 0; i < count; ++i) {
        const bench_clock::time_point sent = bench_clock::now();
        const bench_clock::time_point seen =
            co_await start<where>(loop, [] { return bench_clock::now(); });
        const bench_clock::time_point back = bench_clock::now();

        samples.out.push_back(nanoseconds(seen - sent).count());
        samples.back.push_back(nanoseconds(back - seen).count());
    }

    --remaining;
}

template <class loop_t, op_home where>
task fan_task(loop_t& loop, int count, std::uint64_t& sum, int& remaining) {
    for (int i = 0; i < count; ++i)
        sum += co_await start<where>(loop, [i] { return static_cast<std::uint64_t>(i); });

    --remaining;
}

/// The file, read the way async_file reads it: open, chunks until empty, close -- each a
/// round trip. The buffer lives outside so the frame stays the size of a real one.
template <class loop_t>
task read_file(loop_t& loop, const wchar_t* path, std::span<std::byte> buffer, std::size_t& total,
               int& chunks, latency_samples& samples, int& remaining) {
    wxl::core::file f =
        co_await loop.async_call([path] { return wxl::core::file::open_read(path); });

    /// What one read answers with: the bytes, and when the worker started and finished --
    /// so the two legs of the trip can be told apart from the read in between.
    struct chunk_result {
        std::size_t got;
        bench_clock::time_point started;
        bench_clock::time_point finished;
    };

    for (;;) {
        const bench_clock::time_point sent = bench_clock::now();

        const chunk_result r = co_await loop.async_call([&f, buffer] {
            const bench_clock::time_point started = bench_clock::now();
            const std::size_t got = f.read(buffer);
            return chunk_result{got, started, bench_clock::now()};
        });

        const bench_clock::time_point back = bench_clock::now();

        if (r.got == 0) break;

        samples.out.push_back(nanoseconds(r.started - sent).count());
        samples.back.push_back(nanoseconds(back - r.finished).count());
        total += r.got;
        ++chunks;
    }

    co_await loop.async_call([&f] { f.close(); });

    --remaining;
}

// ---------------------------------------------------------------------------------------
// One round of each scenario.

struct ping_result {
    double round_trip_ns;  ///< mean per operation over the round
    summary out;
    summary back;
};

template <class loop_t, op_home where>
ping_result ping_pong_round(loop_t& loop, int count) {
    latency_samples samples;
    samples.reserve(static_cast<std::size_t>(count));
    int remaining = 1;

    const bench_clock::time_point started = bench_clock::now();
    task t = ping_pong<loop_t, where>(loop, count, samples, remaining);
    loop.run_until([&] { return remaining == 0; });
    const double per_op = nanoseconds(bench_clock::now() - started).count() / count;

    t.result();

    return {per_op, summarize(samples.out), summarize(samples.back)};
}

template <class loop_t, op_home where>
double fan_out_round(loop_t& loop, int tasks, int count) {
    const std::uint64_t expected =
        static_cast<std::uint64_t>(tasks) * (static_cast<std::uint64_t>(count) * (count - 1) / 2);

    std::vector<task> running;
    running.reserve(static_cast<std::size_t>(tasks));
    std::uint64_t sum = 0;
    int remaining = tasks;

    const bench_clock::time_point started = bench_clock::now();

    for (int i = 0; i < tasks; ++i)
        running.push_back(fan_task<loop_t, where>(loop, count, sum, remaining));

    loop.run_until([&] { return remaining == 0; });

    const double per_op =
        nanoseconds(bench_clock::now() - started).count() / (double(tasks) * count);

    for (task& t : running) t.result();

    if (sum != expected) std::printf("  fan-out: checksum mismatch\n");

    return per_op;
}

/// fan-out with nobody sleeping: the STA side spins on its channel and the worker's spin
/// window is longer than the STA side ever takes to answer. No wake enters the number,
/// so what is left is the machinery itself -- the send, the op, the resume.
template <class loop_t, op_home where>
double fan_out_hot_round(loop_t& loop, int tasks, int count) {
    std::vector<task> running;
    running.reserve(static_cast<std::size_t>(tasks));
    std::uint64_t sum = 0;
    int remaining = tasks;

    const bench_clock::time_point started = bench_clock::now();

    for (int i = 0; i < tasks; ++i)
        running.push_back(fan_task<loop_t, where>(loop, count, sum, remaining));

    while (remaining != 0)
        if (!loop.try_run_one()) cpu_pause();

    const double per_op =
        nanoseconds(bench_clock::now() - started).count() / (double(tasks) * count);

    for (task& t : running) t.result();

    return per_op;
}

struct file_result {
    double per_chunk_ns;
    double out_median_ns;   ///< the send to the read starting on the worker
    double back_median_ns;  ///< the read finishing to the coroutine running again
};

template <class loop_t>
file_result file_round(loop_t& loop, const wchar_t* path, std::size_t chunk) {
    std::vector<std::byte> buffer(chunk);
    latency_samples samples;
    std::size_t total = 0;
    int chunks = 0;
    int remaining = 1;

    const bench_clock::time_point started = bench_clock::now();
    task t = read_file<loop_t>(loop, path, buffer, total, chunks, samples, remaining);
    loop.run_until([&] { return remaining == 0; });
    const double per_chunk = nanoseconds(bench_clock::now() - started).count() / chunks;

    t.result();

    return {per_chunk, summarize(samples.out).median, summarize(samples.back).median};
}

/// The same reads with no loop at all, on the calling thread: the floor the file rows
/// are measured against.
double synchronous_file(const wchar_t* path, std::size_t chunk, int rounds) {
    double best = 1e30;
    std::vector<std::byte> buffer(chunk);

    for (int round = 0; round < rounds; ++round) {
        int chunks = 0;

        const bench_clock::time_point started = bench_clock::now();
        wxl::core::file f = wxl::core::file::open_read(path);

        while (f.read(buffer) != 0) ++chunks;

        f.close();
        const double per_chunk = nanoseconds(bench_clock::now() - started).count() / chunks;

        if (per_chunk < best) best = per_chunk;
    }

    return best;
}

struct driven_result {
    double per_op_ns;
    double posts_per_op;
};

/// The STA thread stands in for the UI thread: it sleeps on the post signal the way a
/// dispatcher sleeps on its queue, and drains with run_pending() when woken. What the
/// row counts is the posts, since in an application each one is a queued work item.
template <class loop_t>
driven_result driven_round(loop_t& loop, bool coalesced, int tasks, int count) {
    loop.reset_counts();

    std::vector<task> running;
    running.reserve(static_cast<std::size_t>(tasks));
    std::uint64_t sum = 0;
    int remaining = tasks;

    if (coalesced) loop.arm();

    const bench_clock::time_point started = bench_clock::now();

    for (int i = 0; i < tasks; ++i)
        running.push_back(fan_task<loop_t, op_home::pool>(loop, count, sum, remaining));

    while (remaining != 0) {
        loop.from_worker().wakeup().wait();

        if (coalesced)
            loop.run_pending_coalesced();
        else
            loop.run_pending();
    }

    const double per_op =
        nanoseconds(bench_clock::now() - started).count() / (double(tasks) * count);

    for (task& t : running) t.result();

    // Leave the trigger the way the sleeping shape expects to find it.
    if (coalesced) loop.from_worker().disarm();

    return {per_op, double(loop.from_worker().wakeup().sets()) / (double(tasks) * count)};
}

// ---------------------------------------------------------------------------------------
// The rows.

struct settings {
    int ping_count;
    int fan_tasks;
    int fan_count;
    int rounds;
    const wchar_t* file_path;
};

/// Where a row gets its loop: a fresh one per round, so that where the scheduler put the
/// worker is rolled again each time and best-of picks the best placement along with the
/// best run. The real sta_loop cannot be rebuilt: its provider hands out the same adapter
/// every time, and its row measures the one placement the process was given.
template <class config_t>
struct fresh {
    using loop_type = loop_t<config_t>;

    static std::unique_ptr<loop_type> make() { return std::make_unique<loop_type>(); }
};

struct production {
    using loop_type = production_loop;

    static std::unique_ptr<loop_type> make() { return std::make_unique<loop_type>(); }
};

/// Kernel wakes per operation, to the worker and back to the STA side. 1.00 is a sleep
/// and a wake for every operation; less means the next one was already there.
template <class config_t>
std::string wakes_of(loop_t<config_t>& loop) {
    const double ops = double(loop.executed());
    char text[64];

    std::snprintf(text, sizeof text, "%.2f / %.2f", double(loop.to_worker().wakeup().sets()) / ops,
                  double(loop.from_worker().wakeup().sets()) / ops);

    return text;
}

std::string wakes_of(production_loop&) { return "n/a"; }

template <class provider>
void ping_row(const char* name, const settings& how) {
    ping_result best{1e30, {}, {}};
    std::string wakes = "n/a";
    std::vector<double> trips;

    for (int round = 0; round < how.rounds; ++round) {
        std::unique_ptr<typename provider::loop_type> loop = provider::make();
        const ping_result r =
            ping_pong_round<typename provider::loop_type, op_home::pool>(*loop, how.ping_count);

        trips.push_back(r.round_trip_ns);

        if (r.round_trip_ns < best.round_trip_ns) {
            best = r;
            wakes = wakes_of(*loop);
        }
    }

    std::printf("  %-24s %8.2f %8.2f %9.2f %9.2f %9.2f %9.2f   %s\n", name,
                best.round_trip_ns / 1000.0, summarize(trips).median / 1000.0,
                best.out.mean / 1000.0, best.out.median / 1000.0, best.back.mean / 1000.0,
                best.back.median / 1000.0, wakes.c_str());
}

template <class provider, op_home where, bool hot>
std::pair<double, std::string> fan_best(const settings& how) {
    double best = 1e30;
    std::string wakes = "n/a";

    for (int round = 0; round < how.rounds; ++round) {
        std::unique_ptr<typename provider::loop_type> loop = provider::make();
        double per_op;

        if constexpr (hot)
            per_op = fan_out_hot_round<typename provider::loop_type, where>(*loop, how.fan_tasks,
                                                                            how.fan_count);
        else
            per_op = fan_out_round<typename provider::loop_type, where>(*loop, how.fan_tasks,
                                                                        how.fan_count);

        if (per_op < best) {
            best = per_op;
            wakes = wakes_of(*loop);
        }
    }

    return {best, wakes};
}

/// One row, three placements of the operation; the wakes are those of the pool run.
template <class provider, bool hot = false>
void fan_row(const char* name, const settings& how) {
    const auto [pooled, wakes] = fan_best<provider, op_home::pool, hot>(how);
    const auto [framed, framed_wakes] = fan_best<provider, op_home::frame, hot>(how);
    const auto [padded, padded_wakes] = fan_best<provider, op_home::frame_padded, hot>(how);

    std::printf("  %-24s %9.1f %9.1f %9.1f   %s\n", name, pooled, framed, padded, wakes.c_str());
}

template <class provider>
void file_row(const char* name, const settings& how, const double (&sync)[3]) {
    constexpr std::size_t chunks[3] = {4 * 1024, 64 * 1024, 1024 * 1024};

    std::printf("  %-24s", name);

    for (int i = 0; i < 3; ++i) {
        file_result best{1e30, 0, 0};

        for (int round = 0; round < how.rounds; ++round) {
            std::unique_ptr<typename provider::loop_type> loop = provider::make();
            const file_result r = file_round(*loop, how.file_path, chunks[i]);

            if (r.per_chunk_ns < best.per_chunk_ns) best = r;
        }

        std::printf(" %7.1f %+7.1f %6.1f %6.1f", best.per_chunk_ns / 1000.0,
                    (best.per_chunk_ns - sync[i]) / 1000.0, best.out_median_ns / 1000.0,
                    best.back_median_ns / 1000.0);
    }

    std::putchar('\n');
}

template <class provider>
void driven_rows(const char* name, bool coalesced, const settings& how) {
    for (const int tasks : {1, 8, 64}) {
        driven_result best{1e30, 0};

        for (int round = 0; round < how.rounds; ++round) {
            std::unique_ptr<typename provider::loop_type> loop = provider::make();
            const driven_result r = driven_round(*loop, coalesced, tasks, how.fan_count);

            if (r.per_op_ns < best.per_op_ns) best = r;
        }

        char label[64];
        std::snprintf(label, sizeof label, "%s, %d coroutines", name, tasks);
        std::printf("  %-24s %9.1f %12.3f\n", label, best.per_op_ns, best.posts_per_op);
    }
}

/// A file of the asked size, random enough that nothing collapses, in the temp directory.
std::wstring make_file(std::size_t bytes) {
    wchar_t dir[MAX_PATH];
    ::GetTempPathW(MAX_PATH, dir);

    std::wstring path = dir;
    path += L"wxl_sta_loop_benchmark.bin";

    std::vector<std::byte> content(bytes);
    std::mt19937_64 rng{20260903};

    for (std::size_t i = 0; i + 8 <= bytes; i += 8) {
        const std::uint64_t word = rng();
        std::memcpy(content.data() + i, &word, 8);
    }

    wxl::core::file out = wxl::core::file::create(path.c_str());
    out.write(content);
    out.flush();
    out.close();

    // Read once so the rows measure the cache and not the disk.
    wxl::core::file in = wxl::core::file::open_read(path.c_str());
    in.read(content);

    return path;
}

// The configurations, one per axis and the combinations worth having.
using copy_of_production = config<event_signal, 0, false>;
using address = config<address_signal, 0, false>;
using spin_5us = config<event_signal, 5'000, false>;
using spin_50us = config<event_signal, 50'000, false>;
using address_spin_5us = config<address_signal, 5'000, false>;
using above_normal = config<event_signal, 0, false, THREAD_PRIORITY_ABOVE_NORMAL>;
using address_above_normal = config<address_signal, 0, false, THREAD_PRIORITY_ABOVE_NORMAL>;

// The floor: a worker that stays awake long past anything the spinning STA side takes.
using hot = config<event_signal, 200'000, false>;

using driven_naive = config<event_signal, 0, true>;
using driven_coalesced = config<event_signal, 0, false>;

}  // namespace

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 5;

    // "pin" puts the STA thread on cpu 0 and every variant's worker on cpu 2 -- two
    // physical cores -- so that a row measures the mechanism rather than where the
    // scheduler happened to put the threads. The real sta_loop's worker cannot be
    // reached and stays wherever it lands.
    if (argc > 2 && std::string_view(argv[2]) == "pin") {
        g_sta_cpu = 0;
        g_worker_cpu = 2;
        thread::set_affinity(g_sta_cpu);
    }

    // A third argument names one table to run alone: ping, fan, floor, file or driven.
    const std::string_view only = argc > 3 ? argv[3] : "all";
    const auto wanted = [only](std::string_view table) { return only == "all" || only == table; };

    std::printf(
        "wxl.async sta_loop -- what one co_await costs through the loop, best of %d rounds\n",
        rounds);
    std::printf("%u logical processors, %s\n\n", std::thread::hardware_concurrency(),
                g_sta_cpu == unpinned ? "threads unpinned as in production"
                                      : "STA thread on cpu 0, variant workers on cpu 2");

    const std::wstring file_path = make_file(4 * 1024 * 1024);

    const settings how{.ping_count = 20'000,
                       .fan_tasks = 64,
                       .fan_count = 2'000,
                       .rounds = rounds,
                       .file_path = file_path.c_str()};

    sta_loop::start("sta_loop benchmark worker");

    if (wanted("ping")) {
        std::printf(
            "ping-pong: one coroutine, %d operations one after another -- microseconds per round "
            "trip\n\n",
            how.ping_count);
        std::printf("  %-24s %8s %8s %9s %9s %9s %9s   %s\n", "", "trip", "med rnd", "out mean",
                    "out med", "back mean", "back med", "wakes/op to / back");
        std::putchar('\n');

        ping_row<production>("sta_loop", how);
        ping_row<fresh<copy_of_production>>("copy", how);
        ping_row<fresh<address>>("address", how);
        ping_row<fresh<spin_5us>>("spin 5us", how);
        ping_row<fresh<spin_50us>>("spin 50us", how);
        ping_row<fresh<address_spin_5us>>("address+spin 5us", how);
        ping_row<fresh<above_normal>>("worker above normal", how);
        ping_row<fresh<address_above_normal>>("address, above normal", how);
    }

    if (wanted("fan")) {
        std::printf(
            "\n\nfan-out: %d coroutines x %d operations in flight together -- nanoseconds per "
            "operation, by where the operation lives\n\n",
            how.fan_tasks, how.fan_count);
        std::printf("  %-24s %9s %9s %9s   %s\n", "", "pool", "frame", "padded", "wakes/op (pool)");
        std::putchar('\n');

        fan_row<production>("sta_loop", how);
        fan_row<fresh<copy_of_production>>("copy", how);
        fan_row<fresh<address>>("address", how);
        fan_row<fresh<spin_5us>>("spin 5us", how);
        fan_row<fresh<address_spin_5us>>("address+spin 5us", how);
        fan_row<fresh<above_normal>>("worker above normal", how);
    }

    if (wanted("floor")) {
        std::printf(
            "\n\nfloor: the same fan-out with nobody sleeping -- the machinery alone, nanoseconds "
            "per operation, by where the operation lives\n\n");
        std::printf("  %-24s %9s %9s %9s   %s\n", "", "pool", "frame", "padded", "wakes/op (pool)");
        std::putchar('\n');

        fan_row<fresh<hot>, true>("nobody sleeps", how);
    }

    if (wanted("file")) {
        std::printf(
            "\n\nfile: a cached 4 MB file read in chunks -- microseconds per chunk, what the loop "
            "adds to the same read on the calling thread, and the two legs of the trip "
            "(medians)\n\n");

        double sync[3];
        sync[0] = synchronous_file(how.file_path, 4 * 1024, how.rounds);
        sync[1] = synchronous_file(how.file_path, 64 * 1024, how.rounds);
        sync[2] = synchronous_file(how.file_path, 1024 * 1024, how.rounds);

        std::printf("  %-24s %29s %29s %29s\n", "", "4 KB", "64 KB", "1 MB");
        std::printf("  %-24s %7s %7s %6s %6s %7s %7s %6s %6s %7s %7s %6s %6s\n", "", "chunk",
                    "added", "out", "back", "chunk", "added", "out", "back", "chunk", "added",
                    "out", "back");
        std::putchar('\n');
        std::printf("  %-24s %7.1f %7s %6s %6s %7.1f %7s %6s %6s %7.1f\n", "on the calling thread",
                    sync[0] / 1000.0, "", "", "", sync[1] / 1000.0, "", "", "", sync[2] / 1000.0);

        file_row<production>("sta_loop", how, sync);
        file_row<fresh<copy_of_production>>("copy", how, sync);
        file_row<fresh<address>>("address", how, sync);
        file_row<fresh<spin_5us>>("spin 5us", how, sync);
        file_row<fresh<spin_50us>>("spin 50us", how, sync);
        file_row<fresh<above_normal>>("worker above normal", how, sync);
        file_row<fresh<address_above_normal>>("address, above normal", how, sync);
    }

    if (wanted("driven")) {
        std::printf(
            "\n\ndriven: the STA thread told through a dispatcher and draining with run_pending() "
            "-- %d operations per coroutine\n\n",
            how.fan_count);
        std::printf("  %-24s %9s %12s\n", "", "ns/op", "posts/op");
        std::putchar('\n');

        driven_rows<fresh<driven_naive>>("post per op", false, how);
        driven_rows<fresh<driven_coalesced>>("coalesced", true, how);
    }

    sta_loop::stop();

    ::DeleteFileW(file_path.c_str());

    std::printf(
        "\n  trip is the whole round trip; out is from the send to the first instruction of the\n"
        "  body on the worker, back from the last to the coroutine running again. wakes/op is\n"
        "  how many kernel wakes an operation cost, to the worker and back to the STA side:\n"
        "  1.00 is a sleep and a wake every time, less means the next one was already there.\n"
        "  pool / frame / padded is where the operation lives: in the STA pool behind a\n"
        "  unique_ptr, in the coroutine frame as the awaitable itself, or in the frame with a\n"
        "  cache line of padding on either side of it.\n"
        "  Every variant row is the best of its rounds, each round on a freshly started worker,\n"
        "  so the scheduler's placement is rolled again every time; sta_loop's one worker is\n"
        "  started once and measured where it landed.\n"
        "  Rows named spin bend the no-polling rule on purpose, to price what it forbids.\n");

    return 0;
}
