// One writer, one reader: what the two channels of wxl.async cost against the textbook
// bounded buffer -- a ring over std::vector, a mutex and condition variables.
//
// Two measurements.
//
//   latency     the writer sends one element, pauses, sends the next: a steady stream with a
//               fixed pause, from a few hundred nanoseconds, where the reader barely has the
//               time to fall asleep, to fifty microseconds, where it sleeps every time and
//               its core has the time to go idle. The number is the time from the writer's
//               call to the reader holding the element; beside it, what the send cost the
//               writer, since waking a sleeping reader is paid on the writer's side.
//   throughput  the writer sends as fast as it can and the reader takes as fast as it can.
//               The number is elements delivered per second, from the first send to the
//               last receive.
//
// Every reader takes through the blocking call of its queue -- receive(), or the wait on the
// condition variable -- because a reader that sleeps while there is nothing to take is what
// all three are for, and what that sleep costs is the question.
//
// The channels go to sleep in the kernel the moment they find nothing, while the ring's
// condition variable, by its numbers, wakes its reader without the kernel at short pauses --
// so each channel has two more rows, with a short wait before its own sleep: a number of
// attempts to take an element without blocking, and only then receive(). Between two
// attempts the reader either executes the pause instruction or goes straight on. The two
// rows wait equally long, not equally many times: `tries` is the count with the pause, and
// the count without it is matched at start by timing both waits through the reader's own
// code, on its own core, with nothing coming. The waits that run out in the streams are
// timed as well and printed at the end, which is what says the two really waited alike.
//
// Nothing is allocated while a number is being taken. spsc_channel and the ring keep the
// elements by value, and spsc_channel's blocks are grown by a warm-up and recycled after it.
// mpsc_channel carries intrusive nodes, and those come from one preallocated array which the
// reader hands back through a single counter: the cheapest supply there is, cheaper than the
// pool a real user would put there, so read that row as a floor.
//
// The clock is the TSC rather than steady_clock: QueryPerformanceCounter ticks at 10 MHz on
// this machine, and 100 ns is as long as the shortest latencies being measured. The TSC is
// invariant and shared by the cores, so a stamp taken by the writer and one taken by the
// reader subtract; its rate is calibrated against steady_clock at start.
//
// Both threads run at THREAD_PRIORITY_TIME_CRITICAL. At normal priority, on a machine busy
// with other work, the reader was kept off its core for milliseconds at a time and even the
// medians came out in milliseconds, for every queue alike -- numbers about the scheduler, not
// about any queue.
//
// Usage: wxl.async.channels-benchmark [samples [elements [rounds [tries [writer_cpu reader_cpu]]]]]
//        samples per latency round, elements per throughput round, attempts with the pause
//        before sleeping

#include "platform.h"

#include <intrin.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

namespace {

using bench_clock = std::chrono::steady_clock;
using nanoseconds = std::chrono::duration<double, std::nano>;

std::uint64_t ticks() noexcept { return __rdtsc(); }

/// The reader's stamp waits for the receive before it to finish, which a plain rdtsc does
/// not: it may execute ahead of the loads that brought the element in.
std::uint64_t ticks_after() noexcept {
    unsigned int processor;
    return __rdtscp(&processor);
}

double calibrate_ticks_per_ns() {
    const bench_clock::time_point clock_started = bench_clock::now();
    const std::uint64_t ticks_started = ticks();

    while (bench_clock::now() - clock_started < std::chrono::milliseconds(250)) {
    }

    const std::uint64_t ticks_finished = ticks();
    const bench_clock::time_point clock_finished = bench_clock::now();

    return static_cast<double>(ticks_finished - ticks_started) /
           nanoseconds(clock_finished - clock_started).count();
}

/// The pause between two sends is spun rather than slept: a sleep is not precise below a
/// millisecond, and a spinning writer keeps its own core awake, so that the only core with
/// the time to go idle is the reader's.
void spin_until(const std::uint64_t deadline) noexcept {
    while (ticks() < deadline) wxl::core::cpu_pause();
}

/// What travels: when the writer sent it, and which one it is, so that the reader can say
/// whether anything arrived out of order.
struct sample {
    std::uint64_t stamp;
    std::uint64_t sequence;
};

// ---------------------------------------------------------------------------------------
// The contenders, behind one shape: send() on the writing thread, a reader built on the
// reading thread whose receive() blocks until there is an element, and the count of the
// times either side had to wait -- which is what the latency and the writer's cost follow,
// and which says why a number is what it is.

/// The kernel event both channels sleep on by default, with the reader's waits counted. Both
/// take the primitive as a template parameter, so the count rides the channel's own path:
/// one plain increment on the reader's thread, next to a call into the kernel.
class counted_event
{
public:
    explicit counted_event(const bool manual_reset) : event_(manual_reset) {}

    void set() noexcept { event_.set(); }

    void wait() {
        ++waits_;
        event_.wait();
    }

    bool wait_for(const wxl::core::duration timeout) {
        ++waits_;
        return event_.wait_for(timeout);
    }

    std::size_t waits() const noexcept { return waits_; }

private:
    wxl::core::hevent event_;
    std::size_t waits_{};
};

/// spsc_channel with the block size sta_loop runs it with.
class spsc_contender : public wxl::core::noncopyable
{
    using channel_t = spsc_channel<sample, 256, counted_event>;

public:
    void send(const sample& element) { channel_.send(element); }

    std::size_t reader_waits() noexcept { return channel_.wakeup().waits(); }

    std::size_t writer_waits() const noexcept { return 0; }

    class reader
    {
    public:
        explicit reader(spsc_contender& contender) : reader_(contender.channel_) {}

        /// Takes an element if one is there, never waiting.
        bool try_receive(sample& element) { return reader_.read(element); }

        sample receive() {
            sample element;

            // A false is a spurious wakeup under the channel's contract, never an empty
            // answer to act on: the loop is the caller's half of that contract.
            while (!reader_.receive(element)) {
            }

            return element;
        }

    private:
        channel_t::reader reader_;
    };

private:
    channel_t channel_{false};  // auto-reset, the shape sta_loop uses
};

struct node : wxl::core::intrusive_slist_node<node> {
    sample value{};
};

/// A pointer that owns nothing. send() and receive() want something with get(), release()
/// and reset(), and the nodes here belong to the node_ring, not to whoever holds one.
struct lent_node {
    node* pointer{};

    node* get() const noexcept { return pointer; }
    node* release() noexcept { return std::exchange(pointer, nullptr); }
    void reset(node* taken) noexcept { pointer = taken; }
};

/// The nodes mpsc_channel carries, out of one array allocated up front. The channel keeps
/// them in the order they were sent, so the reader returns a node by counting: once it has
/// taken n of them, the first n are free, and the writer may reuse a slot as soon as the count
/// has moved past it. One release store per element on the reader's side, and a load on the
/// writer's side only once per lap of the array.
///
/// A writer that has lapped the reader waits for the count to move, spinning. That wait is
/// the harness's and not the channel's, which is why it is counted and reported rather than
/// hidden in the number.
class node_ring : public wxl::core::noncopyable
{
public:
    /// The size is rounded up to a power of two: the ring is indexed by a mask.
    explicit node_ring(const std::size_t size)
        : nodes_(std::make_unique<node[]>(std::bit_ceil(size))), mask_(std::bit_ceil(size) - 1) {}

    /// The writing thread's call.
    node* take() noexcept {
        if (taken_ == limit_) [[unlikely]] wait_for_room();

        return &nodes_[taken_++ & mask_];
    }

    /// The reading thread's call, once it is done with the oldest node it holds.
    void give_back() noexcept { returned_.store(++returned_by_reader_, std::memory_order_release); }

    std::size_t stalls() const noexcept { return stalls_; }

private:
    void wait_for_room() noexcept {
        limit_ = returned_.load(std::memory_order_acquire) + mask_ + 1;

        if (taken_ != limit_) return;

        ++stalls_;

        do {
            wxl::core::cpu_pause();
            limit_ = returned_.load(std::memory_order_acquire) + mask_ + 1;
        } while (taken_ == limit_);
    }

    std::unique_ptr<node[]> nodes_;
    std::size_t mask_;

    // The writer's own.
    std::size_t taken_{};
    std::size_t limit_{mask_ + 1};
    std::size_t stalls_{};

    // The reader's: its count, and the copy of it the writer reads, on a line of their own.
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> returned_{};
    std::size_t returned_by_reader_{};
};

class mpsc_contender : public wxl::core::noncopyable
{
public:
    explicit mpsc_contender(const std::size_t nodes) : nodes_(nodes) {}

    void send(const sample& element) {
        lent_node sent{nodes_.take()};
        sent.pointer->value = element;
        channel_.send(sent);
    }

    std::size_t reader_waits() noexcept { return channel_.get_wait_object()->waits(); }

    /// The channel never makes its writer wait; the node supply does, once it is lapped.
    std::size_t writer_waits() const noexcept { return nodes_.stalls(); }

    class reader
    {
    public:
        explicit reader(mpsc_contender& contender) : contender_(&contender) {}

        /// Takes an element if one is there, never waiting.
        bool try_receive(sample& element) {
            lent_node taken;

            if (!contender_->channel_.try_receive(taken)) return false;

            element = take_from(taken);
            return true;
        }

        sample receive() {
            lent_node taken;

            // The condition-variable contract, as with spsc_channel above.
            while (!contender_->channel_.receive(taken)) {
            }

            return take_from(taken);
        }

    private:
        sample take_from(const lent_node taken) {
            const sample element = taken.pointer->value;
            contender_->nodes_.give_back();
            return element;
        }

        mpsc_contender* contender_;
    };

private:
    node_ring nodes_;
    mpsc_channel<node, counted_event> channel_;
};

/// What the reader does between two attempts of its wait before sleeping.
enum class between_tries
{
    pause,   // the pause instruction
    nothing  // straight on to the next attempt
};

/// A channel whose reader waits a little before the channel's own sleep: up to `tries`
/// attempts to take an element without blocking, and only then receive(). The channel itself
/// is untouched -- the wait is what a caller can put in front of receive() today, and the
/// question is whether it pays.
///
/// A wait that runs out is timed, from the first failed attempt to the last: those are the
/// waits that ended in a sleep, and their length says whether two rows that were meant to
/// wait equally long really did, in the stream rather than on an idle channel. The clock is
/// read only once the first attempt has failed, so a reader that finds its element at once
/// pays nothing for it.
template <class contender_t, between_tries how>
class waiting_first : public contender_t
{
public:
    template <class... args_t>
    explicit waiting_first(const std::size_t tries, args_t&&... args)
        : contender_t(std::forward<args_t>(args)...), tries_(tries) {}

    /// Read by the reader at every wait, so it may change between two jobs.
    void tries(const std::size_t count) noexcept { tries_ = count; }

    std::size_t tries() const noexcept { return tries_; }

    /// How many waits ran out in the streams so far, and how long they lasted on average.
    std::size_t ran_out() const noexcept { return ran_out_; }

    std::uint64_t ran_out_ticks() const noexcept { return ran_out_ticks_; }

    class reader
    {
    public:
        explicit reader(waiting_first& contender) : reader_(contender), contender_(&contender) {}

        sample receive() {
            sample element;

            if (reader_.try_receive(element)) return element;

            const std::uint64_t started = ticks();

            if (keep_trying(element)) return element;

            contender_->ran_out_ticks_ += ticks() - started;
            ++contender_->ran_out_;

            return reader_.receive();
        }

        /// One whole wait on the empty channel, timed exactly as receive() times it: the
        /// figure the counts of the two rows are matched by.
        std::uint64_t idle_wait() {
            sample element;
            [[maybe_unused]] const bool taken = reader_.try_receive(element);

            const std::uint64_t started = ticks();
            [[maybe_unused]] const bool kept = keep_trying(element);

            return ticks() - started;
        }

    private:
        /// The attempts after the first failed one, the pause or nothing before each.
        bool keep_trying(sample& element) {
            const std::size_t tries = contender_->tries_;

            for (std::size_t attempt = 1; attempt < tries; ++attempt) {
                if constexpr (how == between_tries::pause) wxl::core::cpu_pause();

                if (reader_.try_receive(element)) return true;
            }

            return false;
        }

        typename contender_t::reader reader_;
        waiting_first* contender_;
    };

private:
    std::size_t tries_;
    std::size_t ran_out_{};
    std::uint64_t ran_out_ticks_{};
};

/// The textbook bounded buffer: a ring of slots in a std::vector, one mutex over all of it,
/// and a condition variable for each side to sleep on -- the reader while the ring is empty,
/// the writer while it is full. Each side notifies the other after every operation, outside
/// the lock, which is the form the textbooks give.
///
/// The waits are counted under the lock, once per operation that found the ring empty or
/// full: how many wakeups the condition variable took to satisfy it is its own business.
class ring_contender : public wxl::core::noncopyable
{
public:
    explicit ring_contender(const std::size_t capacity) : slots_(capacity) {}

    void send(const sample& element) {
        {
            std::unique_lock lock(mutex_);

            if (count_ == slots_.size()) ++writer_waits_;

            not_full_.wait(lock, [this] { return count_ != slots_.size(); });

            slots_[tail_] = element;
            tail_ = tail_ + 1 == slots_.size() ? 0 : tail_ + 1;
            ++count_;
        }

        not_empty_.notify_one();
    }

    std::size_t reader_waits() const noexcept { return reader_waits_; }

    std::size_t writer_waits() const noexcept { return writer_waits_; }

    class reader
    {
    public:
        explicit reader(ring_contender& contender) : contender_(&contender) {}

        sample receive() { return contender_->take(); }

    private:
        ring_contender* contender_;
    };

private:
    sample take() {
        sample element;

        {
            std::unique_lock lock(mutex_);

            if (count_ == 0) ++reader_waits_;

            not_empty_.wait(lock, [this] { return count_ != 0; });

            element = slots_[head_];
            head_ = head_ + 1 == slots_.size() ? 0 : head_ + 1;
            --count_;
        }

        not_full_.notify_one();
        return element;
    }

    std::vector<sample> slots_;
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t count_{};
    std::size_t reader_waits_{};
    std::size_t writer_waits_{};
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

// ---------------------------------------------------------------------------------------
// One queue with its reader thread, kept for the whole run: spsc_channel takes exactly one
// reader in its life, and one warm-up then serves every measurement after it.

struct distribution {
    double min;
    double median;
    double p90;
    double p99;
    double p999;
    double max;
};

distribution describe(std::vector<double> values) {
    std::sort(values.begin(), values.end());

    const std::size_t n = values.size();
    const auto at = [&](const double quantile) {
        return values[std::min(n - 1, static_cast<std::size_t>(quantile * static_cast<double>(n)))];
    };

    return {values.front(), at(0.5), at(0.9), at(0.99), at(0.999), values.back()};
}

/// How many times each side went to wait during one stream.
struct waits {
    std::size_t reader;
    std::size_t writer;
};

struct latency_result {
    distribution latency_ns;
    distribution send_ns;
    waits waited;  // over the whole stream, warm-up included
    std::size_t out_of_order;
};

struct throughput_result {
    double ns_per_element;         // first send to last receive
    double writer_ns_per_element;  // first send to last send
    waits waited;
    std::size_t out_of_order;
};

template <class contender_t>
class session : public wxl::core::noncopyable
{
public:
    template <class... args_t>
    session(std::string name, const double ticks_per_ns, const std::size_t reader_cpu,
            args_t&&... args)
        : name_(std::move(name)),
          ticks_per_ns_(ticks_per_ns),
          reader_cpu_(reader_cpu),
          queue_(std::forward<args_t>(args)...),
          reader_thread_([this] { serve(); }) {}

    ~session() {
        job_ = {job_kind::stop, 0};
        job_ready_.release();
        reader_thread_.join();
    }

    const char* name() const noexcept { return name_.c_str(); }

    void rename(std::string name) { name_ = std::move(name); }

    /// The queue itself, for what only some contenders have. Touched only between jobs.
    contender_t& queue() noexcept { return queue_; }

    /// How long one whole wait before sleeping lasts with nothing coming, in nanoseconds: the
    /// median of `count` of them, on the reader's own thread and core and through its own
    /// code, since that is where the wait runs in the stream.
    double idle_wait_ns(const std::size_t count) {
        arrived_.assign(count, 0);
        start({job_kind::idle_waits, count});
        job_done_.acquire();

        std::vector<std::uint64_t> taken = arrived_;
        std::sort(taken.begin(), taken.end());

        return static_cast<double>(taken[taken.size() / 2]) / ticks_per_ns_;
    }

    /// `warmup` elements go first at the same pause and are left out of the numbers: they
    /// carry the reader's start and whatever the queue still had to grow.
    latency_result latency(const std::size_t warmup, const std::size_t samples,
                           const std::chrono::nanoseconds pause) {
        const std::size_t total = warmup + samples;
        const auto pause_ticks = static_cast<std::uint64_t>(
            static_cast<double>(pause.count()) * ticks_per_ns_);

        // Sized and touched before the stream starts, so that nothing in it allocates.
        arrived_.assign(total, 0);
        sent_.assign(total, 0);

        const waits before = waited();
        start({job_kind::latency, total});

        for (std::size_t i = 0; i < total; ++i) {
            const std::uint64_t stamp = ticks();
            queue_.send(sample{stamp, i});
            const std::uint64_t sent = ticks();

            sent_[i] = sent - stamp;
            spin_until(sent + pause_ticks);
        }

        job_done_.acquire();

        return {describe(to_ns(arrived_, warmup)), describe(to_ns(sent_, warmup)),
                waited_since(before), out_of_order_};
    }

    throughput_result throughput(const std::size_t elements) {
        const waits before = waited();
        start({job_kind::throughput, elements});

        const std::uint64_t started = ticks();

        for (std::size_t i = 0; i < elements; ++i) queue_.send(sample{0, i});

        const std::uint64_t written = ticks();

        job_done_.acquire();

        const auto per_element = [&](const std::uint64_t from, const std::uint64_t to) {
            return static_cast<double>(to - from) / ticks_per_ns_ / static_cast<double>(elements);
        };

        return {per_element(started, finished_), per_element(started, written),
                waited_since(before), out_of_order_};
    }

private:
    enum class job_kind
    {
        latency,
        throughput,
        idle_waits,
        stop
    };

    struct job {
        job_kind kind;
        std::size_t count;
    };

    /// Read only while the reader is between jobs, so that its counts are still.
    waits waited() noexcept { return {queue_.reader_waits(), queue_.writer_waits()}; }

    waits waited_since(const waits before) noexcept {
        const waits now = waited();
        return {now.reader - before.reader, now.writer - before.writer};
    }

    /// Hands the reader its job and waits until it has picked it up, so that the writer's
    /// first send does not race the reader's own start.
    void start(const job next) {
        job_ = next;
        job_ready_.release();
        reader_started_.acquire();
    }

    void serve() {
        thread::set_affinity(reader_cpu_);
        thread::priority(THREAD_PRIORITY_TIME_CRITICAL);

        typename contender_t::reader reader(queue_);

        for (;;) {
            job_ready_.acquire();

            const job current = job_;

            if (current.kind == job_kind::stop) return;

            out_of_order_ = 0;
            reader_started_.release();

            if (current.kind == job_kind::latency) {
                for (std::size_t i = 0; i < current.count; ++i) {
                    const sample element = reader.receive();
                    arrived_[i] = ticks_after() - element.stamp;
                    out_of_order_ += element.sequence != i;
                }
            } else if (current.kind == job_kind::idle_waits) {
                if constexpr (requires { reader.idle_wait(); })
                    for (std::size_t i = 0; i < current.count; ++i) arrived_[i] = reader.idle_wait();
            } else {
                for (std::size_t i = 0; i < current.count; ++i)
                    out_of_order_ += reader.receive().sequence != i;

                finished_ = ticks_after();
            }

            job_done_.release();
        }
    }

    std::vector<double> to_ns(const std::vector<std::uint64_t>& ticks_taken,
                              const std::size_t skipped) const {
        std::vector<double> converted;
        converted.reserve(ticks_taken.size() - skipped);

        for (std::size_t i = skipped; i < ticks_taken.size(); ++i)
            converted.push_back(static_cast<double>(ticks_taken[i]) / ticks_per_ns_);

        return converted;
    }

    std::string name_;
    double ticks_per_ns_;
    std::size_t reader_cpu_;
    contender_t queue_;

    // Written by one thread and read by the other only across the semaphores below, whose
    // release and acquire carry them.
    job job_{};
    std::vector<std::uint64_t> arrived_;
    std::vector<std::uint64_t> sent_;
    std::uint64_t finished_{};
    std::size_t out_of_order_{};

    std::binary_semaphore job_ready_{0};
    std::binary_semaphore reader_started_{0};
    std::binary_semaphore job_done_{0};

    // Last, so that it starts with everything it reads already built.
    std::thread reader_thread_;
};

// ---------------------------------------------------------------------------------------
// The tables.

constexpr std::chrono::nanoseconds pauses[] = {
    std::chrono::nanoseconds(100), std::chrono::nanoseconds(200), std::chrono::nanoseconds(500),
    std::chrono::microseconds(1),  std::chrono::microseconds(5),  std::chrono::microseconds(10),
    std::chrono::microseconds(20), std::chrono::microseconds(50)};

void print_pause(const std::chrono::nanoseconds pause) {
    if (pause < std::chrono::microseconds(1))
        std::printf("pause %lld ns\n", static_cast<long long>(pause.count()));
    else
        std::printf("pause %lld us\n",
                    static_cast<long long>(
                        std::chrono::duration_cast<std::chrono::microseconds>(pause).count()));
}

/// Each figure of a latency row is the median of that figure over the rounds. A stall of the
/// machine -- the reader kept off its core for milliseconds by something that is not the
/// queue -- lands in one round and moves from queue to queue between runs; the median drops
/// it, while a tail the queue itself has shows in every round and stays.
struct latency_rounds {
    std::vector<latency_result> taken;

    double median(const auto figure) const {
        std::vector<double> values;

        for (const latency_result& r : taken) values.push_back(figure(r));

        std::sort(values.begin(), values.end());
        return values[values.size() / 2];
    }
};

template <class contender_t>
void latency_row(const session<contender_t>& queue, const latency_rounds& rounds,
                 const std::size_t per_round) {
    const auto m = [&](const auto figure) { return rounds.median(figure); };

    std::size_t waited = 0;
    std::size_t out_of_order = 0;

    for (const latency_result& r : rounds.taken) {
        waited += r.waited.reader;
        out_of_order += r.out_of_order;
    }

    std::printf("  %-24s %7.2f %7.2f %7.2f %7.2f %7.2f %8.2f   %6.0f %7.0f   %5.1f\n",
                queue.name(), m([](auto& r) { return r.latency_ns.min; }) / 1000,
                m([](auto& r) { return r.latency_ns.median; }) / 1000,
                m([](auto& r) { return r.latency_ns.p90; }) / 1000,
                m([](auto& r) { return r.latency_ns.p99; }) / 1000,
                m([](auto& r) { return r.latency_ns.p999; }) / 1000,
                m([](auto& r) { return r.latency_ns.max; }) / 1000,
                m([](auto& r) { return r.send_ns.median; }),
                m([](auto& r) { return r.send_ns.p99; }),
                100.0 * static_cast<double>(waited) /
                    static_cast<double>(per_round * rounds.taken.size()));

    if (out_of_order) std::printf("  %zu elements arrived out of order\n", out_of_order);
}

/// One pause of the stream, every queue given: the rounds go round the queues in turn, so
/// that whatever else the machine does during the run falls on all of them alike.
template <class... sessions_t>
void latency_table(const std::chrono::nanoseconds pause, const int rounds,
                   const std::size_t warmup, const std::size_t samples, sessions_t&... queues) {
    std::array<latency_rounds, sizeof...(sessions_t)> taken;

    for (int round = 0; round < rounds; ++round) {
        std::size_t row = 0;
        (taken[row++].taken.push_back(queues.latency(warmup, samples, pause)), ...);
    }

    print_pause(pause);
    std::printf("  %-24s %7s %7s %7s %7s %7s %8s   %6s %7s   %5s\n", "", "min", "median", "p90",
                "p99", "p99.9", "max", "send", "p99", "waits");

    std::size_t row = 0;
    (latency_row(queues, taken[row++], warmup + samples), ...);

    std::putchar('\n');
}

struct throughput_rounds {
    std::vector<double> total;
    std::vector<double> writer;
    waits waited{};
    std::size_t elements{};
    std::size_t out_of_order{};

    void add(const throughput_result& r, const std::size_t sent) {
        total.push_back(r.ns_per_element);
        writer.push_back(r.writer_ns_per_element);
        waited.reader += r.waited.reader;
        waited.writer += r.waited.writer;
        elements += sent;
        out_of_order += r.out_of_order;
    }
};

void throughput_row(const char* name, throughput_rounds rounds) {
    std::sort(rounds.total.begin(), rounds.total.end());
    std::sort(rounds.writer.begin(), rounds.writer.end());

    const double best = rounds.total.front();
    const double median = rounds.total[rounds.total.size() / 2];
    const auto per_thousand = [&](const std::size_t count) {
        return 1000.0 * static_cast<double>(count) / static_cast<double>(rounds.elements);
    };

    std::printf("  %-24s %9.2f %9.1f %9.2f %9.1f %9.2f   %7.2f %7.2f\n", name, best, 1000.0 / best,
                median, 1000.0 / median, rounds.writer.front(), per_thousand(rounds.waited.reader),
                per_thousand(rounds.waited.writer));

    if (rounds.out_of_order)
        std::printf("  %zu elements arrived out of order\n", rounds.out_of_order);
}

template <class... sessions_t>
void throughput_table(const std::size_t elements, const int rounds, sessions_t&... queues) {
    // One lap first, untimed: it grows spsc_channel to whatever backlog a writer at full
    // speed builds, so that no timed round pays for a block.
    (queues.throughput(elements), ...);

    std::array<throughput_rounds, sizeof...(sessions_t)> taken;

    for (int round = 0; round < rounds; ++round) {
        std::size_t row = 0;
        (taken[row++].add(queues.throughput(elements), elements), ...);
    }

    std::printf(
        "throughput: %zu elements as fast as the writer can send them, %d rounds; delivered\n"
        "is the first send to the last receive, writer is the first send to the last send;\n"
        "waits are per thousand elements, over all the rounds\n\n",
        elements, rounds);
    std::printf("  %-24s %19s %19s %9s   %15s\n", "", "best", "median", "writer", "waits");
    std::printf("  %-24s %9s %9s %9s %9s %9s   %7s %7s\n", "", "ns/elem", "Melem/s", "ns/elem",
                "Melem/s", "ns/elem", "reader", "writer");

    std::size_t row = 0;
    (throughput_row(queues.name(), std::move(taken[row++])), ...);
}

// ---------------------------------------------------------------------------------------
// How long the wait before sleeping lasts: a number of attempts says nothing on its own,
// since both the pause instruction and an attempt cost what the machine makes them cost.

/// What one pause instruction holds the core for.
double pause_ns(const double ticks_per_ns) {
    constexpr int count = 1'000'000;

    const std::uint64_t started = ticks();

    for (int i = 0; i < count; ++i) wxl::core::cpu_pause();

    return static_cast<double>(ticks() - started) / ticks_per_ns / count;
}

/// The waits of the two rows of one channel, matched by time.
struct matched {
    double with_pause_ns;
    double without_pause_ns;
    std::size_t tries;
};

/// Gives the row without the pause as many attempts as make its wait as long as the row's
/// with it. Both waits are timed through the reader's own code on its own thread and core,
/// since an attempt timed anywhere else -- in a loop of its own, on another core -- came out
/// shorter than the same attempt in the stream; the count is scaled by the ratio and timed
/// again until the two agree within a percent.
template <class paused_t, class running_t>
matched match_waits(session<paused_t>& paused, session<running_t>& running,
                    const double pause) {
    constexpr std::size_t waits = 2'001;

    const double target = paused.idle_wait_ns(waits);

    // A first guess that is roughly right on any machine: an attempt costs a nanosecond or so.
    auto tries = static_cast<std::size_t>(
        std::max(1.0, static_cast<double>(paused.queue().tries()) * (pause + 1.0)));
    double reached = 0;

    for (int step = 0;; ++step) {
        running.queue().tries(tries);
        reached = running.idle_wait_ns(waits);

        // A wait of one attempt has nothing after the first one to time, and nothing to scale.
        if (reached <= 0 || std::abs(reached - target) <= target / 100 || step == 8) break;

        tries = static_cast<std::size_t>(
            std::max(1.0, std::round(static_cast<double>(tries) * target / reached)));
    }

    running.rename(std::format("  + {} tries, no pause", tries));
    return {target, reached, tries};
}

/// How long the waits that ran out in the streams really lasted: whether the two rows of a
/// channel waited equally long in the stream, not only on an idle channel.
template <class contender_t>
void ran_out_row(const char* channel, session<contender_t>& queue, const double ticks_per_ns) {
    const std::size_t count = queue.queue().ran_out();
    const double mean = count ? static_cast<double>(queue.queue().ran_out_ticks()) /
                                    static_cast<double>(count) / ticks_per_ns
                              : 0.0;

    std::printf("  %-12s %-24s %10zu %10.0f\n", channel, queue.name(), count, mean);
}

}  // namespace

int main(int argc, char** argv) {
    const std::size_t samples = argc > 1 ? static_cast<std::size_t>(std::atoll(argv[1])) : 4'000;
    const std::size_t elements =
        argc > 2 ? static_cast<std::size_t>(std::atoll(argv[2])) : 4'000'000;
    const int rounds = argc > 3 ? std::atoi(argv[3]) : 5;
    const std::size_t tries = argc > 4 ? static_cast<std::size_t>(std::atoll(argv[4])) : 10;

    // Logical processors of two different physical cores: two siblings of one core would
    // share the cache the elements travel through, and that is a different machine.
    const std::size_t writer_cpu = argc > 6 ? static_cast<std::size_t>(std::atoi(argv[5])) : 2;
    const std::size_t reader_cpu = argc > 6 ? static_cast<std::size_t>(std::atoi(argv[6])) : 0;

    constexpr std::size_t warmup = 1'000;
    constexpr std::size_t small_ring = 1'024;
    constexpr std::size_t large_ring = 65'536;

    thread::set_affinity(writer_cpu);
    thread::priority(THREAD_PRIORITY_TIME_CRITICAL);

    const double ticks_per_ns = calibrate_ticks_per_ns();
    const double pause = pause_ns(ticks_per_ns);

    using spsc_pausing = waiting_first<spsc_contender, between_tries::pause>;
    using spsc_running = waiting_first<spsc_contender, between_tries::nothing>;
    using mpsc_pausing = waiting_first<mpsc_contender, between_tries::pause>;
    using mpsc_running = waiting_first<mpsc_contender, between_tries::nothing>;

    const std::string with_pause = std::format("  + {} tries, pause", tries);

    // The rows without the pause get their count from match_waits() below.
    session<spsc_contender> spsc("spsc_channel<256>", ticks_per_ns, reader_cpu);
    session<spsc_pausing> spsc_paused(with_pause, ticks_per_ns, reader_cpu, tries);
    session<spsc_running> spsc_unpaused("", ticks_per_ns, reader_cpu, tries);
    session<mpsc_contender> mpsc("mpsc_channel", ticks_per_ns, reader_cpu, large_ring);
    session<mpsc_pausing> mpsc_paused(with_pause, ticks_per_ns, reader_cpu, tries, large_ring);
    session<mpsc_running> mpsc_unpaused("", ticks_per_ns, reader_cpu, tries, large_ring);
    session<ring_contender> ring("ring/1024", ticks_per_ns, reader_cpu, small_ring);
    session<ring_contender> big_ring("ring/65536", ticks_per_ns, reader_cpu, large_ring);

    const matched spsc_waits = match_waits(spsc_paused, spsc_unpaused, pause);
    const matched mpsc_waits = match_waits(mpsc_paused, mpsc_unpaused, pause);

    std::printf("wxl.async channels, one writer and one reader\n");
    std::printf("writer on cpu %zu, reader on cpu %zu, TSC at %.3f GHz\n\n", writer_cpu,
                reader_cpu, ticks_per_ns);

    std::printf(
        "the wait before sleeping: attempts to take an element, then receive(). The pause\n"
        "instruction holds the core for %.1f ns. Timed on the reader's core with nothing coming:\n"
        "  spsc_channel  %zu attempts with the pause %.0f ns, %zu without it %.0f ns\n"
        "  mpsc_channel  %zu attempts with the pause %.0f ns, %zu without it %.0f ns\n\n",
        pause, tries, spsc_waits.with_pause_ns, spsc_waits.tries, spsc_waits.without_pause_ns,
        tries, mpsc_waits.with_pause_ns, mpsc_waits.tries, mpsc_waits.without_pause_ns);

    std::printf(
        "latency: a steady stream, one element per pause; microseconds from the writer's call\n"
        "to the reader holding the element; what the send cost the writer, in nanoseconds; and\n"
        "how often the reader called the wait of its event or condition variable -- a sleep,\n"
        "unless a signal was already there -- in percent of the elements. %d rounds of %zu\n"
        "elements after %zu of warm-up, taking turns between the queues; each figure is its\n"
        "median over the rounds\n\n",
        rounds, samples, warmup);

    for (const std::chrono::nanoseconds pause_between : pauses)
        latency_table(pause_between, rounds, warmup, samples, spsc, spsc_paused, spsc_unpaused,
                      mpsc, mpsc_paused, mpsc_unpaused, ring);

    throughput_table(elements, rounds, spsc, spsc_paused, spsc_unpaused, mpsc, mpsc_paused,
                     mpsc_unpaused, ring, big_ring);

    std::printf(
        "\nthe waits that ran out in all the streams above and ended in a sleep: how many, and\n"
        "how long they lasted on average, in nanoseconds\n\n");
    std::printf("  %-12s %-24s %10s %10s\n", "", "", "ran out", "lasted");

    ran_out_row("spsc_channel", spsc_paused, ticks_per_ns);
    ran_out_row("", spsc_unpaused, ticks_per_ns);
    ran_out_row("mpsc_channel", mpsc_paused, ticks_per_ns);
    ran_out_row("", mpsc_unpaused, ticks_per_ns);

    std::printf(
        "\n  The channels never make their writer wait. mpsc_channel's writer column is the\n"
        "  node supply: a preallocated ring of %zu nodes the reader hands back through one\n"
        "  counter, where a real user would pay for a pool -- so those rows are a floor. The\n"
        "  ring's writer waits while the ring is full. A reader's wait in the rows with tries\n"
        "  is counted only when the tries came to nothing and it went to sleep.\n",
        large_ring);

    return 0;
}
