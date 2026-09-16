// What `spsc_queue` costs: against the reference implementation of its own block handover,
// and against `mpsc_queue`, the structure everything above wxl.core queued through
// before it existed.
//
// The two queues differ in one thing -- how a drained block gets from the reader back to
// the writer, through a slot with one writer per word or through a CAS and an exchange --
// and that shows in some regimes and not others, so there are four measurements:
//
//   steady        two threads, the reader keeping up. What a handover costs per element.
//   recovered     the same, after a backlog has grown the queue past every cache.
//   burst         one thread fills, then drains. What growing costs and what walking what
//                 was grown costs, with no inter-core traffic in the number at all.
//   after a burst one thread grows the queue with a burst, drains it, and then runs a long
//                 stretch of write-one-read-one, over a fresh heap and over a fragmented
//                 one. What it costs to keep going once the burst is over.
//
// The last table runs the whole machine: one writer/reader pair per physical core, the two
// sides of a pair on different cores, every queue grown past the caches by its own backlog
// first. What all of them share there is the L3 and the path to memory.
//
// mpsc_queue is measured in FIFO mode, one node per element, and it is flattered
// twice over: in the threaded run its nodes are all allocated up front, and in the
// single-threaded runs they are recycled through a private list. A real user of it needs a
// pool for that and pays for the pool, so read that row as a floor rather than as its cost.
//
// Usage: wxl.async.spsc-queues-benchmark [rounds [pairs [backlog]]]

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

namespace {

using bench_clock = std::chrono::steady_clock;
using seconds = std::chrono::duration<double>;

using element_t = std::uint64_t;

/// Logical processors the two sides are pinned to: sibling threads of one physical core
/// would share the cache the handover is being measured through.
constexpr std::size_t reader_cpu = 0;
constexpr std::size_t producer_cpu = 2;

/// The smallest of several runs rather than the average: the interesting quantity is what
/// the machine does when nothing interrupts it, and noise only ever adds.
class best_rate
{
public:
    void add(const bench_clock::duration duration, const std::size_t elements) noexcept {
        const double per_element = seconds(duration).count() * 1e9 / static_cast<double>(elements);

        if (per_element < best_) best_ = per_element;
    }

    double nanoseconds_per_element() const noexcept { return best_; }

private:
    double best_ = 1e30;
};

/// Keeps the reads from being optimized away, and says so if a queue ever came back empty
/// where the caller knew it could not be.
struct sink {
    element_t sum{};
    std::size_t misses{};

    void report(const char* const what) const {
        if (misses) std::printf("  %s: %zu reads came back empty\n", what, misses);
        if (sum == 0xdead'beef'dead'beefULL) std::printf("  (%llu)\n", sum);
    }
};

// ---------------------------------------------------------------------------------------
// The contenders, behind one shape: write, read, and a capacity that the queues growing on
// their own have no use for.

template <class queue_t>
class spsc_handle : public wxl::core::noncopyable
{
public:
    explicit spsc_handle(std::size_t) {}

    void write(const element_t value) { queue_.write(value); }

    bool read(element_t& value) { return reader_.read(value); }

private:
    queue_t queue_;
    typename queue_t::reader reader_{queue_};
};

struct node : wxl::core::intrusive_slist_node<node> {
    element_t value{};
};

/// `mpsc_queue` in FIFO mode. With `recycled`, a node the reader is done with goes
/// straight back to the writer through a private list -- the cheapest stand-in there is for
/// the pool a real user would need, and legal only because those runs are single-threaded.
template <bool recycled>
class queue_handle : public wxl::core::noncopyable
{
public:
    explicit queue_handle(const std::size_t capacity)
        : nodes_(std::make_unique<node[]>(capacity)), capacity_(capacity) {
        // Out of the way of the measurement: every page touched once, so no run pays for
        // the first use of memory the others were handed ready.
        for (std::size_t i = 0; i < capacity_; ++i) nodes_[i].value = 0;
    }

    ~queue_handle() {
        while (queue_.pop_one()) {
        }
    }

    void write(const element_t value) {
        node* taken = nullptr;

        if constexpr (recycled) {
            taken = free_;

            if (taken) free_ = taken->next_;
        }

        if (!taken) taken = &nodes_[next_++];

        taken->value = value;
        queue_.push_one(taken);
    }

    bool read(element_t& value) {
        node* const taken = queue_.pop_one();

        if (!taken) return false;

        value = taken->value;

        if constexpr (recycled) {
            taken->next_ = free_;
            free_ = taken;
        }

        return true;
    }

private:
    mpsc_queue<node> queue_;
    std::unique_ptr<node[]> nodes_;
    std::size_t capacity_;
    std::size_t next_{};
    node* free_{};
};

/// The heap a long-lived program actually has. Blocks of exactly the size and alignment the
/// queues allocate are taken in bulk and half of them given back in a shuffled order, so the
/// allocator's free list for that size class holds holes scattered across the whole range.
/// A queue growing afterwards is handed those holes, which is what makes its blocks land
/// nowhere near each other -- the state every measurement above quietly assumed away by
/// growing into a fresh heap, where blocks come out back to back and a sweep over them is a
/// sequential scan the prefetcher covers.
class fragmented_heap : public wxl::core::noncopyable
{
public:
    fragmented_heap(const std::size_t block_bytes, const std::size_t holes)
        : alignment_(std::align_val_t{std::hardware_destructive_interference_size}),
          block_bytes_(block_bytes) {
        held_.resize(holes * 2);

        for (void*& slot : held_) slot = ::operator new(block_bytes_, alignment_);

        std::vector<std::size_t> order(held_.size());
        std::iota(order.begin(), order.end(), std::size_t{0});

        // A fixed seed: the point is a heap that is not in address order, and a different
        // one each run would make the numbers unrepeatable for no gain.
        std::mt19937_64 shuffling{20260822};
        std::shuffle(order.begin(), order.end(), shuffling);

        order.resize(holes);

        for (const std::size_t index : order) {
            ::operator delete(held_[index], alignment_);
            held_[index] = nullptr;
        }
    }

    ~fragmented_heap() {
        for (void* const slot : held_)
            if (slot) ::operator delete(slot, alignment_);
    }

    /// How far apart the allocator now puts one block from the next, in blocks. One means
    /// back to back, which is the answer that says the fragmenting did not take.
    double scatter(const std::size_t sample) const {
        std::vector<void*> taken(sample);

        for (void*& slot : taken) slot = ::operator new(block_bytes_, alignment_);

        double total = 0;

        for (std::size_t i = 1; i < sample; ++i) {
            const auto previous = reinterpret_cast<std::intptr_t>(taken[i - 1]);
            const auto current = reinterpret_cast<std::intptr_t>(taken[i]);

            total += std::abs(static_cast<double>(current - previous));
        }

        for (void* const slot : taken) ::operator delete(slot, alignment_);

        return sample > 1 ? total / static_cast<double>((sample - 1) * block_bytes_) : 0.0;
    }

private:
    std::align_val_t alignment_;
    std::size_t block_bytes_;
    std::vector<void*> held_;
};

/// What one row needs to fragment the heap the way its queue allocates.
struct geometry {
    std::size_t block_bytes;
    std::size_t blocks;
};

// ---------------------------------------------------------------------------------------
// The three measurements.

/// What one write costs while a reader is draining on another core. The clock is the
/// producer's own: measuring the pair from outside says how far apart the two threads
/// happened to drift, which on a hot queue swings by a factor of five between runs -- the
/// reader either sits on the block the writer is filling, paying a coherence round trip per
/// element, or falls a block behind and pays nothing. The writer's own time is the quantity
/// a caller can act on, and it is measured against the worst reader there is: one that never
/// waits and never lets go of the line.
template <class handle_t>
double steady(const std::size_t elements, const int rounds) {
    best_rate best;

    for (int round = 0; round < rounds; ++round) {
        handle_t handle(elements);
        std::atomic<bool> go{false};
        bench_clock::duration writing{};

        std::thread producer([&] {
            // Two distinct physical cores, fixed. Without this the number says which pair
            // of logical processors the scheduler happened to pick, and two siblings of one
            // core are a different machine for this measurement.
            thread::set_affinity(producer_cpu);
            go.wait(false, std::memory_order_acquire);

            const auto started = bench_clock::now();

            for (element_t i = 0; i < elements; ++i) handle.write(i);

            writing = bench_clock::now() - started;
        });

        go.store(true, std::memory_order_release);
        go.notify_one();

        std::size_t out_of_order = 0;

        for (element_t expected = 0; expected < elements;) {
            element_t value;

            if (handle.read(value)) {
                out_of_order += value != expected;
                ++expected;
            }
        }

        producer.join();
        best.add(writing, elements);

        if (out_of_order) std::printf("  %zu elements arrived out of order\n", out_of_order);
    }

    return best.nanoseconds_per_element();
}

/// The scenario the agile queues exist for, with both threads running. The reader is held
/// off while the writer runs ahead, so the queue grows to whatever the backlog needs; then
/// the reader is let go and catches up; and only then is anything measured -- a long stretch
/// of ordinary traffic through a structure that is now far larger than any cache.
///
/// A ring has to keep walking all of it: its traversal order is the ring, and the blocks the
/// burst added are still in it. The agile queues go back to the few blocks that stay warm
/// and leave the rest alone. The number is the writer's own time per element, for the same
/// reason as in `steady`.
template <class handle_t>
double recovered(const std::size_t grown, const std::size_t elements, const int rounds) {
    best_rate best;
    sink into;

    for (int round = 0; round < rounds; ++round) {
        handle_t handle(grown + elements);
        std::atomic<bool> catch_up{false};
        std::atomic<bool> grew{false};
        bench_clock::duration writing{};

        std::thread producer([&] {
            thread::set_affinity(producer_cpu);

            // The backlog: the reader is not reading yet, so every one of these grows the
            // queue by whatever it has to grow by.
            for (element_t i = 0; i < grown; ++i) handle.write(i);

            grew.store(true, std::memory_order_release);
            grew.notify_one();
            catch_up.wait(false, std::memory_order_acquire);

            const auto started = bench_clock::now();

            for (element_t i = 0; i < elements; ++i) handle.write(i);

            writing = bench_clock::now() - started;
        });

        grew.wait(false, std::memory_order_acquire);

        // Caught up before the clock starts: from here the queue is as big as the backlog
        // made it and as empty as it was to begin with.
        for (std::size_t taken = 0; taken < grown;) {
            element_t value{};

            if (handle.read(value)) {
                into.sum += value;
                ++taken;
            }
        }

        catch_up.store(true, std::memory_order_release);
        catch_up.notify_one();

        for (std::size_t taken = 0; taken < elements;) {
            element_t value{};

            if (handle.read(value)) {
                into.sum += value;
                ++taken;
            }
        }

        producer.join();
        best.add(writing, elements);
    }

    into.report("recovered");
    return best.nanoseconds_per_element();
}

/// One thread, so the number carries no inter-core traffic: what the writer pays to grow,
/// and what the reader pays to walk what it grew.
template <class handle_t>
std::pair<double, double> burst(const std::size_t elements, const int rounds) {
    best_rate fill;
    best_rate drain;
    sink into;

    for (int round = 0; round < rounds; ++round) {
        handle_t handle(elements);

        const auto fill_started = bench_clock::now();

        for (element_t i = 0; i < elements; ++i) handle.write(i);

        fill.add(bench_clock::now() - fill_started, elements);

        const auto drain_started = bench_clock::now();

        for (std::size_t i = 0; i < elements; ++i) {
            element_t value{};
            into.misses += !handle.read(value);
            into.sum += value;
        }

        drain.add(bench_clock::now() - drain_started, elements);
    }

    into.report("burst");
    return {fill.nanoseconds_per_element(), drain.nanoseconds_per_element()};
}

/// The point of the agile queues. A burst grows the structure, the burst is drained, and
/// then traffic goes back to one element at a time -- which the ring answers by walking
/// every block it grew, again and again. The halves are reported apart because a queue that
/// shrinks needs a lap to do it.
///
/// Both halves come from the SAME round, the one whose two together were fastest, and not
/// from two independent minima: the queues run within 10-20% of themselves from round to
/// round, so picking each half's best on its own manufactures differences between the halves
/// that are pure round selection -- in either direction, which is how one reads as a queue
/// getting slower over time when nothing of the sort is happening.
///
/// `where` decides what kind of heap the burst grows into: a fresh one, where the blocks
/// come out back to back, or a fragmented one, where they do not. The second is the honest
/// one and the reason this benchmark exists at all -- a sweep over blocks laid out in
/// address order is a sequential scan, and a prefetcher hides most of what it costs.
template <class handle_t>
std::pair<double, double> after_burst(const std::size_t grown, const std::size_t pairs,
                                      const int rounds, const geometry& where) {
    double best_early = 0;
    double best_late = 0;
    double best_total = 1e30;
    sink into;

    const std::unique_ptr<fragmented_heap> heap =
        where.blocks ? std::make_unique<fragmented_heap>(where.block_bytes, where.blocks) : nullptr;

    for (int round = 0; round < rounds; ++round) {
        handle_t handle(grown);

        for (element_t i = 0; i < grown; ++i) handle.write(i);

        for (std::size_t i = 0; i < grown; ++i) {
            element_t value{};
            into.misses += !handle.read(value);
            into.sum += value;
        }

        const auto started = bench_clock::now();

        for (element_t i = 0; i < pairs / 2; ++i) {
            element_t value{};
            handle.write(i);
            into.misses += !handle.read(value);
            into.sum += value;
        }

        const auto middle = bench_clock::now();

        for (element_t i = 0; i < pairs / 2; ++i) {
            element_t value{};
            handle.write(i);
            into.misses += !handle.read(value);
            into.sum += value;
        }

        const auto ended = bench_clock::now();

        const double half = static_cast<double>(pairs / 2);
        const double first = seconds(middle - started).count() * 1e9 / half;
        const double second = seconds(ended - middle).count() * 1e9 / half;

        if (first + second < best_total) {
            best_total = first + second;
            best_early = first;
            best_late = second;
        }
    }

    into.report("after a burst");
    return {best_early, best_late};
}

/// Every hardware thread busy: as many writer/reader pairs as the machine has physical
/// cores, and the two sides of a pair deliberately on different cores -- each core ends up
/// carrying the writer of one queue and the reader of another. That is what a loaded machine
/// looks like, and it is the only arrangement in which the question can be asked: what all
/// these queues share is the L3 and the path to memory, and the blocks a queue never
/// revisits are blocks it never evicts from under its neighbours. One queue sweeping a
/// structure larger than the L3 is a nuisance; six of them sweeping at once is a different
/// machine.
///
/// Each pair grows its own queue with a backlog first -- so the aggregate footprint is
/// several times the L3 -- then drains it, and only then is the traffic measured. A barrier
/// across all the threads keeps the phases together, so nobody is measured against
/// neighbours that are still filling.
struct parallel_result {
    double nanoseconds_per_element;      ///< The mean of what each writer paid.
    double million_elements_per_second;  ///< All the pairs together, which is the machine.
};

template <class handle_t>
parallel_result all_cores(const std::size_t pairs, const std::size_t grown,
                          const std::size_t elements, const int rounds, const geometry& where) {
    const std::unique_ptr<fragmented_heap> heap =
        where.blocks ? std::make_unique<fragmented_heap>(where.block_bytes, where.blocks * pairs)
                     : nullptr;

    double best_mean = 1e30;
    double best_total = 0;

    for (int round = 0; round < rounds; ++round) {
        std::vector<std::unique_ptr<handle_t>> queues;
        std::vector<bench_clock::duration> writing(pairs);
        std::vector<std::uint64_t> sums(pairs);
        std::vector<std::thread> threads;

        queues.reserve(pairs);
        threads.reserve(pairs * 2);

        for (std::size_t pair = 0; pair < pairs; ++pair)
            queues.push_back(std::make_unique<handle_t>(grown + elements));

        std::barrier phases(static_cast<std::ptrdiff_t>(pairs * 2));

        for (std::size_t pair = 0; pair < pairs; ++pair) {
            threads.emplace_back([&, pair] {
                thread::set_affinity(pair * 2);
                handle_t& queue = *queues[pair];

                for (element_t i = 0; i < grown; ++i) queue.write(i);

                phases.arrive_and_wait();  // grown, everywhere
                phases.arrive_and_wait();  // drained, everywhere

                const auto started = bench_clock::now();

                for (element_t i = 0; i < elements; ++i) queue.write(i);

                writing[pair] = bench_clock::now() - started;
            });

            threads.emplace_back([&, pair] {
                // The reader of this queue goes on the second thread of a DIFFERENT core, so
                // every core carries one writer and one reader belonging to two different
                // queues. Putting a pair on the two threads of one core would make its
                // handover free and the two sides fight over one core's execution units --
                // a measurement of hyper-threading rather than of queues.
                thread::set_affinity(((pair + pairs / 2) % pairs) * 2 + 1);
                handle_t& queue = *queues[pair];
                std::uint64_t sum = 0;

                phases.arrive_and_wait();  // wait for the backlog to be there

                for (std::size_t taken = 0; taken < grown;) {
                    element_t value{};

                    if (queue.read(value)) {
                        sum += value;
                        ++taken;
                    }
                }

                phases.arrive_and_wait();  // caught up, everywhere

                for (std::size_t taken = 0; taken < elements;) {
                    element_t value{};

                    if (queue.read(value)) {
                        sum += value;
                        ++taken;
                    }
                }

                sums[pair] = sum;
            });
        }

        for (std::thread& one : threads) one.join();

        double total_nanoseconds = 0;
        double slowest = 0;

        for (const bench_clock::duration& one : writing) {
            const double nanoseconds = seconds(one).count() * 1e9;

            total_nanoseconds += nanoseconds;
            slowest = std::max(slowest, nanoseconds);
        }

        const double mean = total_nanoseconds / static_cast<double>(pairs * elements);
        const double throughput = static_cast<double>(pairs * elements) / slowest * 1000.0;

        if (mean < best_mean) best_mean = mean;
        if (throughput > best_total) best_total = throughput;

        if (sums[0] == 0xdead'beef'dead'beefULL) std::printf("  (%llu)\n", sums[0]);
    }

    return {best_mean, best_total};
}

// ---------------------------------------------------------------------------------------
// One contender, two rows: what it costs to move an element, and what a burst leaves behind.

struct settings {
    std::size_t steady_elements;
    std::size_t burst_elements;
    std::size_t grown_elements;
    std::size_t pair_elements;
    std::size_t pairs;              ///< One writer/reader pair per physical core.
    std::size_t parallel_grown;     ///< The backlog each of them grows, per queue.
    std::size_t parallel_elements;  ///< What is measured afterwards, per queue.
    int rounds;
};

template <class threaded_handle_t, class single_handle_t = threaded_handle_t>
void throughput_row(const char* const name, const settings& how) {
    const double hot = steady<threaded_handle_t>(how.steady_elements, how.rounds);
    const double after_backlog =
        recovered<threaded_handle_t>(how.grown_elements, how.steady_elements, how.rounds);
    const auto [fill, drained] = burst<single_handle_t>(how.burst_elements, how.rounds);

    std::printf("  %-22s %8.2f %10.2f %8.2f %8.2f\n", name, hot, after_backlog, fill, drained);
}

/// The decision table. Two heaps, the same queue: one grown into fresh memory, where blocks
/// come out back to back, and one grown into a heap already holed the way a long-lived
/// program holes it. `spread` says how far apart the allocator puts consecutive blocks in
/// the second, in blocks -- if it came out at 1, the fragmenting did not take and the row
/// below it means nothing.
template <class handle_t>
void sweep_row(const char* const name, const geometry& where, const settings& how) {
    const auto [fresh_early, fresh_late] =
        after_burst<handle_t>(how.grown_elements, how.pair_elements, how.rounds, geometry{});

    const geometry holed{where.block_bytes, where.blocks};
    const double spread = fragmented_heap(holed.block_bytes, holed.blocks).scatter(4096);

    const auto [holed_early, holed_late] =
        after_burst<handle_t>(how.grown_elements, how.pair_elements, how.rounds, holed);

    const double megabytes =
        static_cast<double>(where.blocks) * where.block_bytes / (1024.0 * 1024.0);

    std::printf("  %-22s %8.2f %8.2f %10.2f %8.2f %8.1f %7.0f\n", name, fresh_early, fresh_late,
                holed_early, holed_late, spread, megabytes);
}

template <std::size_t block_size>
geometry geometry_of(const std::size_t elements) {
    return {sizeof(element_t) * block_size + std::hardware_destructive_interference_size,
            elements / block_size};
}

template <template <class, std::size_t> class queue_tt>
void queue_rows(const char* const name, const settings& how) {
    const std::string label = name;

    throughput_row<spsc_handle<queue_tt<element_t, 16>>>((label + "/16").c_str(), how);
    throughput_row<spsc_handle<queue_tt<element_t, 64>>>((label + "/64").c_str(), how);
    throughput_row<spsc_handle<queue_tt<element_t, 128>>>((label + "/128").c_str(), how);
    throughput_row<spsc_handle<queue_tt<element_t, 256>>>((label + "/256").c_str(), how);
}

template <template <class, std::size_t> class queue_tt>
void queue_sweeps(const char* const name, const settings& how) {
    const std::string label = name;

    sweep_row<spsc_handle<queue_tt<element_t, 16>>>((label + "/16").c_str(),
                                                    geometry_of<16>(how.grown_elements), how);
    sweep_row<spsc_handle<queue_tt<element_t, 64>>>((label + "/64").c_str(),
                                                    geometry_of<64>(how.grown_elements), how);
    sweep_row<spsc_handle<queue_tt<element_t, 128>>>((label + "/128").c_str(),
                                                     geometry_of<128>(how.grown_elements), how);
    sweep_row<spsc_handle<queue_tt<element_t, 256>>>((label + "/256").c_str(),
                                                     geometry_of<256>(how.grown_elements), how);
}

/// One row of the machine-wide table: the same queue on every core at once, grown into a
/// fresh heap and then into a fragmented one.
template <class handle_t>
void machine_row(const char* const name, const geometry& where, const settings& how) {
    const parallel_result fresh = all_cores<handle_t>(
        how.pairs, how.parallel_grown, how.parallel_elements, how.rounds, geometry{});

    const parallel_result holed =
        all_cores<handle_t>(how.pairs, how.parallel_grown, how.parallel_elements, how.rounds,
                            geometry{where.block_bytes, where.blocks});

    const double megabytes =
        static_cast<double>(where.blocks * how.pairs) * where.block_bytes / (1024.0 * 1024.0);

    std::printf("  %-22s %8.2f %9.1f %10.2f %9.1f %7.0f\n", name, fresh.nanoseconds_per_element,
                fresh.million_elements_per_second, holed.nanoseconds_per_element,
                holed.million_elements_per_second, megabytes);
}

template <template <class, std::size_t> class queue_tt>
void queue_machines(const char* const name, const settings& how) {
    const std::string label = name;

    machine_row<spsc_handle<queue_tt<element_t, 16>>>((label + "/16").c_str(),
                                                      geometry_of<16>(how.parallel_grown), how);
    machine_row<spsc_handle<queue_tt<element_t, 64>>>((label + "/64").c_str(),
                                                      geometry_of<64>(how.parallel_grown), how);
    machine_row<spsc_handle<queue_tt<element_t, 128>>>((label + "/128").c_str(),
                                                       geometry_of<128>(how.parallel_grown), how);
    machine_row<spsc_handle<queue_tt<element_t, 256>>>((label + "/256").c_str(),
                                                       geometry_of<256>(how.parallel_grown), how);
}

}  // namespace

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 5;

    // One pair per physical core, both of its threads on that core's two hardware threads,
    // which is what `all_cores` assumes when it pins them to 2*pair and 2*pair + 1.
    const std::size_t pairs =
        argc > 2 ? static_cast<std::size_t>(std::atoi(argv[2]))
                 : std::max<std::size_t>(1, std::thread::hardware_concurrency() / 2);

    // How big a backlog grows the queue before anything is measured. It is an argument
    // because the answer to "does walking what a burst grew cost anything" depends on
    // whether what it grew still fits in a cache, and that is worth being able to ask.
    const std::size_t grown = argc > 3 ? static_cast<std::size_t>(std::atoi(argv[3])) : 4'000'000;

    thread::set_affinity(reader_cpu);

    // The backlog is deliberately larger than any cache on this machine: what the tables
    // after the first ask is what it costs to keep walking everything a backlog made the
    // queue grow, and inside a cache that question has no answer. The single-threaded run
    // measures several laps of that structure, so a queue that shrinks has the time to; the
    // machine-wide one grows a smaller queue per core, because there its footprints add up.
    const settings how{.steady_elements = 1'000'000,
                       .burst_elements = 1'000'000,
                       .grown_elements = grown,
                       .pair_elements = 20'000'000,
                       .pairs = pairs,
                       .parallel_grown = 1'000'000,
                       .parallel_elements = 8'000'000,
                       .rounds = rounds};

    std::printf("wxl.async SPSC queues -- nanoseconds per element, best of %d rounds\n", rounds);
    std::printf("%zu logical processors, %zu writer/reader pairs\n\n",
                static_cast<std::size_t>(std::thread::hardware_concurrency()), pairs);

    std::printf("moving an element\n\n");
    std::printf("  %-22s %8s %10s %8s %8s\n", "", "steady", "recovered", "fill", "drain");
    std::printf("  %-22s %8s %10s %8s %8s\n", "", "2 thr", "2 thr", "1 thr", "1 thr");
    std::putchar('\n');

    queue_rows<spsc_queue>("queue", how);
    std::putchar('\n');
    queue_rows<spsc_queue_reference_implementation>("reference", how);
    std::putchar('\n');
    throughput_row<queue_handle<false>, queue_handle<true>>("mpsc_queue/fifo", how);

    std::printf("\n\nwhat a burst leaves behind: write-one-read-one afterwards, one thread\n\n");
    std::printf("  %-22s %17s %19s %8s %7s\n", "", "fresh heap", "fragmented heap", "spread",
                "grown");
    std::printf("  %-22s %8s %8s %10s %8s %8s %7s\n", "", "1st", "2nd", "1st", "2nd", "blocks",
                "MB");
    std::putchar('\n');

    queue_sweeps<spsc_queue>("queue", how);
    std::putchar('\n');
    queue_sweeps<spsc_queue_reference_implementation>("reference", how);

    std::printf("\n\nthe whole machine: %zu pairs, each grown by its own backlog first\n\n", pairs);
    std::printf("  %-22s %18s %20s %7s\n", "", "fresh heap", "fragmented heap", "grown");
    std::printf("  %-22s %8s %9s %10s %9s %7s\n", "", "ns/elem", "Melem/s", "ns/elem", "Melem/s",
                "MB");
    std::putchar('\n');

    queue_machines<spsc_queue>("queue", how);
    std::putchar('\n');
    queue_machines<spsc_queue_reference_implementation>("reference", how);

    std::printf(
        "\n  steady and recovered are the writer's own time per element with a reader\n"
        "  draining on another core; recovered has a backlog behind it that grew the\n"
        "  queue past every cache first. In the last table every core is busy and the\n"
        "  grown column is what all the queues hold together, against a shared L3.\n"
        "  mpsc_queue is flattered -- preallocated nodes in the threaded runs,\n"
        "  hand-recycled ones in the single-threaded ones, where a real user would pay\n"
        "  for a pool -- and it is left out of the tables whose question is about\n"
        "  blocks, which it does not have.\n");

    return 0;
}
