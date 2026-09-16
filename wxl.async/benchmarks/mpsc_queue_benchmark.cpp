// The MPSC path everything above wxl.core queues through -- task_queue, pool and
// mpsc_channel each hold an mpsc_queue -- measured against the two locks it exists to
// replace, and against the two knobs it has of its own.
//
// Four scenarios:
//   1. uncontended round-trip: one thread pushing and popping, batched and then one at a
//      time. The floor cost of the structure, with no cache line ever travelling.
//   2. many writers, one reader: the shape the class is for. Reports ns per item and the
//      average size of the chain pop_all() claims, which is what makes the reader side
//      amortize down to a pointer read.
//   3. backlog drain: the writer runs ahead, then the reader takes the whole chain at once.
//      Prices reverse_linked_list per node with nothing else moving.
//   4. push-only contention: the CAS retry storm on its own, release publish against the
//      seq_cst one push_one_ordered() uses.
//
// Both fifo modes are measured wherever the mode reaches the code: in every scenario that
// reads, since the reversal is the reader's work. Scenario 4 has no reader and push_one() is
// the same instruction stream either way, so a second mode there would time the same code
// twice -- and the drain that scenario ends with is deliberately outside its clock.
//
// The reader spins instead of waiting on purpose. mpsc_queue carries no wakeup of its
// own -- a caller pairs it with an event, the way mpsc_channel does -- and a throughput
// number wants the cost of the queue, not of a signalling primitive next to it.
import std;
import wxl.core;
import wxl.async;

namespace {

using wxl::async::drain_order;
using wxl::async::mpsc_queue;
using wxl::async::mpsc_stack;
using wxl::core::cpu_pause;

using bench_clock = std::chrono::steady_clock;

/// What every queue variant moves: one intrusive link plus a payload the reader adds up, so
/// that no run can be folded away and nothing can go missing unnoticed.
struct node : wxl::core::intrusive_slist_node<node> {
    std::uint64_t value{};
};

struct run_result {
    double ms = 0.0;
    std::uint64_t checksum = 0;
};

double ms_since(bench_clock::time_point start) noexcept {
    return std::chrono::duration<double, std::milli>(bench_clock::now() - start).count();
}

/// Fastest of `repeats` runs, the way the other benchmarks in this tree report: the minimum
/// is the measurement least polluted by the scheduler.
template <typename Fn>
run_result best_of(int repeats, Fn&& run) {
    run_result best = run();
    for (int i = 1; i < repeats; ++i) {
        const run_result r = run();
        if (r.ms < best.ms) best = r;
    }
    return best;
}

// ---------------------------------------------------------------------------------------
// The two baselines. Both move the very same nodes through the very same links as
// mpsc_queue, so what the comparison prices is the synchronization and nothing else.

/// The textbook answer to this problem: one mutex over an intrusive FIFO list.
class mutex_queue : public wxl::core::noncopyable
{
public:
    void push_one(node* element) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        element->next_ = nullptr;
        (tail_ ? tail_->next_ : head_) = element;
        tail_ = element;
    }

    node* pop_one() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        node* const element = head_;
        if (!element) return nullptr;
        if ((head_ = element->next_) == nullptr) tail_ = nullptr;
        return element;
    }

private:
    std::mutex mutex_;
    node* head_ = nullptr;
    node* tail_ = nullptr;
};

/// The same list under the projects own spin_lock -- the middle ground a hand-rolled fast
/// queue usually ends up as, and the one worth beating rather than the kernel mutex above.
class spin_queue : public wxl::core::noncopyable
{
public:
    void push_one(node* element) noexcept {
        wxl::core::spin_lock guard(flag_);
        element->next_ = nullptr;
        (tail_ ? tail_->next_ : head_) = element;
        tail_ = element;
    }

    node* pop_one() noexcept {
        wxl::core::spin_lock guard(flag_);
        node* const element = head_;
        if (!element) return nullptr;
        if ((head_ = element->next_) == nullptr) tail_ = nullptr;
        return element;
    }

private:
    alignas(std::hardware_destructive_interference_size) std::atomic<std::ptrdiff_t> flag_{0};
    node* head_ = nullptr;
    node* tail_ = nullptr;
};

// ---------------------------------------------------------------------------------------
// Reader side.

/// How many chains the reader had to claim to get through a run: pop_one() restarts
/// pops_since_claim() at one whenever it takes a fresh chain off the stack, so counting the
/// ones is counting the claims. Only mpsc_queue offers that counter -- for the locked
/// baselines every element is its own claim by construction.
struct batch_stats {
    std::uint64_t items = 0;
    std::uint64_t batches = 0;

    template <class Queue>
    void observe(const Queue& queue) noexcept {
        ++items;
        if constexpr (requires { queue.pops_since_claim(); })
            batches += (queue.pops_since_claim() == 1);
        else
            ++batches;
    }

    double average_batch() const noexcept {
        return batches ? double(items) / double(batches) : 0.0;
    }
};

/// Reads exactly `count` nodes out of the queue, adding their payloads up. The statistics
/// pass is a separate instantiation rather than a flag, so the timed loop carries no test
/// that only the untimed one ever takes.
template <bool with_stats, class Queue>
std::uint64_t drain(Queue& queue, std::uint64_t count, batch_stats* stats = nullptr) {
    std::uint64_t sum = 0;

    for (std::uint64_t taken = 0; taken < count;) {
        if (node* element = queue.pop_one()) [[likely]] {
            sum += element->value;
            ++taken;
            if constexpr (with_stats) stats->observe(queue);
        } else {
            cpu_pause();
        }
    }

    return sum;
}

/// Empties whatever is left, ignoring it: drain_stack asserts on destruction that it was
/// drained, so a scenario that stops the clock early still has to finish the job.
template <class Queue>
void discard_rest(Queue& queue) noexcept {
    while (queue.pop_one()) {}
}

// ---------------------------------------------------------------------------------------
// Scenario 1: one thread, no contention.

/// Push everything, then take everything back. The buffer claims the whole chain in a single
/// exchange here, which is its best case; the locks pay per element either way.
template <class Queue>
run_result batched_round_trip(node* nodes, unsigned count) {
    Queue queue;

    const auto start = bench_clock::now();
    for (unsigned i = 0; i < count; ++i)
        queue.push_one(nodes + i);
    const std::uint64_t sum = drain<false>(queue, count);
    const double ms = ms_since(start);

    return {ms, sum};
}

/// One push and one pop at a time. Every claim the buffer makes is a chain of one, so nothing
/// amortizes and what is left is the bare cost of a CAS against the cost of a lock.
template <class Queue>
run_result interleaved_round_trip(node* nodes, unsigned count) {
    Queue queue;
    std::uint64_t sum = 0;

    const auto start = bench_clock::now();
    for (unsigned i = 0; i < count; ++i) {
        queue.push_one(nodes + i);
        sum += queue.pop_one()->value;
    }
    const double ms = ms_since(start);

    return {ms, sum};
}

// ---------------------------------------------------------------------------------------
// Scenario 2: many writers, one reader.

/// `writers` threads push their own slice of `nodes` while this thread reads every element
/// back. The gate keeps thread creation out of the measurement without any of the threads
/// spinning to reach the start line.
template <class Queue>
run_result mpsc_run(unsigned writers, unsigned per_writer, node* nodes, batch_stats* stats) {
    Queue queue;
    const std::uint64_t total = std::uint64_t(writers) * per_writer;

    std::latch gate(writers + 1);
    std::vector<std::jthread> producers;
    producers.reserve(writers);

    for (unsigned w = 0; w < writers; ++w) {
        producers.emplace_back([&, w] {
            node* const mine = nodes + std::size_t(w) * per_writer;
            gate.arrive_and_wait();
            for (unsigned i = 0; i < per_writer; ++i)
                queue.push_one(mine + i);
        });
    }

    gate.arrive_and_wait();

    const auto start = bench_clock::now();
    const std::uint64_t sum =
        stats ? drain<true>(queue, total, stats) : drain<false>(queue, total);
    const double ms = ms_since(start);

    return {ms, sum};
}

// ---------------------------------------------------------------------------------------
// Scenario 3: a backlog claimed in one go.

/// Pushes the whole backlog first and times only the drain, so the two fifo modes differ by
/// reverse_linked_list and by nothing else.
template <drain_order order>
run_result backlog_drain(node* nodes, unsigned count) {
    mpsc_queue<node, order> queue;
    for (unsigned i = 0; i < count; ++i)
        queue.push_one(nodes + i);

    const auto start = bench_clock::now();
    const std::uint64_t sum = drain<false>(queue, count);
    const double ms = ms_since(start);

    return {ms, sum};
}

// ---------------------------------------------------------------------------------------
// Scenario 4: the writers alone.

/// Every writer pushes its slice as fast as it can with nobody reading, so the number is the
/// contended CAS and the retries it loses. `strong` picks the seq_cst publish, the one a
/// writer that reads its own wait trigger straight afterwards has to use. The fifo mode is
/// left at its default because it belongs to the reader: push_one() never looks at it.
template <bool strong>
run_result push_storm(unsigned writers, unsigned per_writer, node* nodes) {
    mpsc_queue<node> queue;

    std::latch gate(writers + 1);
    std::vector<std::jthread> pushers;
    pushers.reserve(writers);

    for (unsigned w = 0; w < writers; ++w) {
        pushers.emplace_back([&, w] {
            node* const mine = nodes + std::size_t(w) * per_writer;
            gate.arrive_and_wait();
            for (unsigned i = 0; i < per_writer; ++i) {
                if constexpr (strong)
                    queue.push_one_ordered(mine + i);
                else
                    queue.push_one(mine + i);
            }
        });
    }

    gate.arrive_and_wait();
    const auto start = bench_clock::now();
    pushers.clear();  // joins every writer
    const double ms = ms_since(start);

    discard_rest(queue);
    return {ms, 0};
}

// ---------------------------------------------------------------------------------------
// Reporting.

double ns_per_item(double ms, std::uint64_t items) noexcept {
    return items ? ms * 1e6 / double(items) : 0.0;
}

std::unique_ptr<node[]> make_nodes(std::uint64_t count) {
    auto nodes = std::make_unique<node[]>(count);
    for (std::uint64_t i = 0; i < count; ++i)
        nodes[i].value = i + 1;
    return nodes;
}

/// The sum the reader has to arrive at: the payloads are 1..count, so a lost or a doubled
/// node shows up immediately.
std::uint64_t expected_checksum(std::uint64_t count) noexcept {
    return count * (count + 1) / 2;
}

const char* verdict(std::uint64_t got, std::uint64_t want) noexcept {
    return got == want ? "" : "  <-- CHECKSUM MISMATCH";
}

void bench_single_threaded(unsigned count, int repeats) {
    const auto nodes = make_nodes(count);
    node* const n = nodes.get();
    const std::uint64_t want = expected_checksum(count);

    const auto report = [&](const char* label, const run_result& r, const run_result& base) {
        std::printf("  %-24s %8.3f ms  (%6.1f ns/item)  %5.2fx%s\n", label, r.ms,
                    ns_per_item(r.ms, 2ull * count), base.ms / r.ms, verdict(r.checksum, want));
    };

    batched_round_trip<mutex_queue>(n, count);  // warm-up, discarded

    std::printf("push all %u, then pop all (%d repeats, best-of)\n", count, repeats);
    const run_result batched_base =
        best_of(repeats, [&] { return batched_round_trip<mutex_queue>(n, count); });
    report("mutex + list", batched_base, batched_base);
    report("spin_lock + list",
           best_of(repeats, [&] { return batched_round_trip<spin_queue>(n, count); }), batched_base);
    report("mpsc_queue (fifo)",
           best_of(repeats, [&] { return batched_round_trip<mpsc_queue<node>>(n, count); }),
           batched_base);
    report("mpsc_stack (lifo)",
           best_of(repeats, [&] { return batched_round_trip<mpsc_stack<node>>(n, count); }),
           batched_base);

    std::printf("\npush one, pop one, %u times (%d repeats, best-of)\n", count, repeats);
    const run_result inter_base =
        best_of(repeats, [&] { return interleaved_round_trip<mutex_queue>(n, count); });
    report("mutex + list", inter_base, inter_base);
    report("spin_lock + list",
           best_of(repeats, [&] { return interleaved_round_trip<spin_queue>(n, count); }), inter_base);
    report("mpsc_queue (fifo)",
           best_of(repeats, [&] { return interleaved_round_trip<mpsc_queue<node>>(n, count); }),
           inter_base);
    report("mpsc_stack (lifo)",
           best_of(repeats, [&] { return interleaved_round_trip<mpsc_stack<node>>(n, count); }),
           inter_base);
    std::printf("\n");
}

void bench_mpsc(unsigned writers, unsigned per_writer, int repeats) {
    const std::uint64_t total = std::uint64_t(writers) * per_writer;
    const auto nodes = make_nodes(total);
    node* const n = nodes.get();
    const std::uint64_t want = expected_checksum(total);

    mpsc_run<mutex_queue>(writers, per_writer, n, nullptr);  // warm-up, discarded

    const run_result mutexed =
        best_of(repeats, [&] { return mpsc_run<mutex_queue>(writers, per_writer, n, nullptr); });
    const run_result spinning =
        best_of(repeats, [&] { return mpsc_run<spin_queue>(writers, per_writer, n, nullptr); });
    const run_result fifo = best_of(
        repeats, [&] { return mpsc_run<mpsc_queue<node>>(writers, per_writer, n, nullptr); });
    const run_result lifo = best_of(repeats, [&] {
        return mpsc_run<mpsc_stack<node>>(writers, per_writer, n, nullptr);
    });

    // Separate, untimed passes: reading the counter belongs nowhere near the loops the numbers
    // above come from. Both modes claim their chains the same way, so the two lines also say
    // whether the writers happened to run alike in the two timed runs.
    batch_stats fifo_stats, lifo_stats;
    mpsc_run<mpsc_queue<node>>(writers, per_writer, n, &fifo_stats);
    mpsc_run<mpsc_stack<node>>(writers, per_writer, n, &lifo_stats);

    const auto report = [&](const char* label, const run_result& r) {
        std::printf("  %-24s %8.3f ms  (%6.1f ns/item)  %5.2fx%s\n", label, r.ms,
                    ns_per_item(r.ms, total), mutexed.ms / r.ms, verdict(r.checksum, want));
    };

    std::printf("%u writer%s x %u items = %llu items (%d repeats, best-of)\n", writers,
                writers == 1 ? "" : "s", per_writer, static_cast<unsigned long long>(total),
                repeats);
    std::printf("  %-24s %8.3f ms  (%6.1f ns/item)%s\n", "mutex + list", mutexed.ms,
                ns_per_item(mutexed.ms, total), verdict(mutexed.checksum, want));
    report("spin_lock + list", spinning);
    report("mpsc_queue (fifo)", fifo);
    report("mpsc_stack (lifo)", lifo);
    std::printf("  claims: fifo %llu chains, %.1f items per pop_all();  lifo %llu, %.1f\n\n",
                static_cast<unsigned long long>(fifo_stats.batches), fifo_stats.average_batch(),
                static_cast<unsigned long long>(lifo_stats.batches), lifo_stats.average_batch());
}

void bench_backlog(unsigned count, int repeats) {
    const auto nodes = make_nodes(count);
    node* const n = nodes.get();
    const std::uint64_t want = expected_checksum(count);

    backlog_drain<drain_order::lifo>(n, count);  // warm-up, discarded

    const run_result lifo =
        best_of(repeats, [&] { return backlog_drain<drain_order::lifo>(n, count); });
    const run_result fifo =
        best_of(repeats, [&] { return backlog_drain<drain_order::fifo>(n, count); });

    std::printf("drain a backlog of %u (%d repeats, best-of; the push is not timed)\n", count,
                repeats);
    std::printf("  %-24s %8.3f ms  (%6.1f ns/item)%s\n", "mpsc_stack (lifo)", lifo.ms,
                ns_per_item(lifo.ms, count), verdict(lifo.checksum, want));
    std::printf("  %-24s %8.3f ms  (%6.1f ns/item)%s\n", "mpsc_queue (fifo)", fifo.ms,
                ns_per_item(fifo.ms, count), verdict(fifo.checksum, want));
    std::printf("  reversal costs %.1f ns per node\n\n", ns_per_item(fifo.ms - lifo.ms, count));
}

void bench_push_storm(unsigned writers, unsigned per_writer, int repeats) {
    const std::uint64_t total = std::uint64_t(writers) * per_writer;
    const auto nodes = make_nodes(total);
    node* const n = nodes.get();

    push_storm<false>(writers, per_writer, n);  // warm-up, discarded

    const run_result released =
        best_of(repeats, [&] { return push_storm<false>(writers, per_writer, n); });
    const run_result ordered =
        best_of(repeats, [&] { return push_storm<true>(writers, per_writer, n); });

    std::printf("%u writer%s x %u pushes, nobody reading (%d repeats, best-of)\n", writers,
                writers == 1 ? "" : "s", per_writer, repeats);
    std::printf("  %-24s %8.3f ms  (%6.1f ns/push)\n", "push_one (release)", released.ms,
                ns_per_item(released.ms, total));
    std::printf("  %-24s %8.3f ms  (%6.1f ns/push)  %5.2fx\n", "push_one_ordered (seq_cst)",
                ordered.ms, ns_per_item(ordered.ms, total), released.ms / ordered.ms);
    std::printf("\n");
}

/// Writer counts to run the contended scenarios at, capped so the reader still has a core of
/// its own -- oversubscribing turns the measurement into one of the scheduler.
std::vector<unsigned> writer_grid() {
    const unsigned cores = std::max(2u, std::thread::hardware_concurrency());
    std::vector<unsigned> grid;

    for (unsigned writers : {1u, 2u, 4u, 8u, 16u})
        if (writers <= cores - 1) grid.push_back(writers);

    if (grid.empty()) grid.push_back(1);
    return grid;
}

}  // namespace

int main() {
    constexpr unsigned N = 1000;
    constexpr int Repeats = 10;

#ifndef NDEBUG
    std::printf(
        "NOTE: this is a Debug build - nothing is inlined, and std::mutex carries its\n"
        "debug checks, which flatters every lock-free number below. Build Release for\n"
        "meaningful ones.\n\n");
#endif

    std::printf("hardware_concurrency: %u\n\n", std::thread::hardware_concurrency());

    std::printf("=== Scenario 1: one thread, no contention ===\n");
    bench_single_threaded(N * 100, Repeats);

    std::printf("=== Scenario 2: many writers, one reader ===\n");
    for (unsigned writers : writer_grid())
        bench_mpsc(writers, N * 50, Repeats);

    std::printf("=== Scenario 3: a backlog claimed in one go ===\n");
    bench_backlog(N * 100, Repeats);

    std::printf("=== Scenario 4: the writers alone ===\n");
    for (unsigned writers : writer_grid())
        bench_push_storm(writers, N * 50, Repeats);

    return 0;
}
