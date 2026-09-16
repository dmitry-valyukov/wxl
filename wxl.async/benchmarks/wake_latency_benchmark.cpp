// How long a sleeping thread takes to be woken, by primitive and by how long it slept.
//
// The question behind the loop benchmark: a round trip through sta_loop is two wakes,
// and the wake is the whole cost. But a wake is not one number. A thread that slept for
// two hundred nanoseconds is picked up by a core that is still spinning in its idle loop;
// one that slept for a millisecond is picked up by a core that has gone into a deeper
// idle state and has to be brought out of it. So each primitive is measured over a range
// of sleeps, and the shape of that curve is what a loop can plan around.
//
// Two threads on two physical cores. The ponger waits on the primitive, notes the time it
// woke and answers; the pinger spins for the sleep being measured, notes the time, wakes
// the ponger and waits for the answer. The wake latency is the ponger's time minus the
// pinger's, on one clock (QueryPerformanceCounter, invariant across cores here).
//
// Usage: wxl.async.wake-latency-benchmark [samples [pinger_cpu ponger_cpu]]

#include <windows.h>

import std;
import wxl.core;
import wxl.async;

namespace {

using bench_clock = std::chrono::steady_clock;
using nanoseconds = std::chrono::duration<double, std::nano>;

/// The kernel event, auto-reset: what sta_loop's worker sleeps on today.
class event_signal
{
public:
    void set() noexcept { event_.set(); }
    void wait() { event_.wait(); }

private:
    wxl::core::hevent event_{false};
};

/// WaitOnAddress / WakeByAddressSingle, the address wait Windows 8 added.
class address_signal
{
public:
    void set() noexcept {
        state_.store(1, std::memory_order_release);
        ::WakeByAddressSingle(&state_);
    }

    void wait() noexcept {
        std::uint32_t zero = 0;

        while (state_.exchange(0, std::memory_order_acquire) == 0)
            ::WaitOnAddress(&state_, &zero, sizeof state_, INFINITE);
    }

private:
    std::atomic<std::uint32_t> state_{0};
};

/// std::atomic::wait / notify_one -- the standard spelling of the same thing, kept as a
/// row because the STL is free to spin before it sleeps, and if it does that shows.
class atomic_signal
{
public:
    void set() noexcept {
        state_.store(1, std::memory_order_release);
        state_.notify_one();
    }

    void wait() noexcept {
        while (state_.exchange(0, std::memory_order_acquire) == 0)
            state_.wait(0, std::memory_order_acquire);
    }

private:
    std::atomic<std::uint32_t> state_{0};
};

struct stats {
    double mean;
    double median;
    double p90;
    double max;
};

stats summarize(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const std::size_t n = samples.size();

    return {sum / static_cast<double>(n), samples[n / 2], samples[n * 9 / 10], samples[n - 1]};
}

void spin_for(std::chrono::nanoseconds how_long) {
    const bench_clock::time_point until = bench_clock::now() + how_long;

    while (bench_clock::now() < until) wxl::core::cpu_pause();
}

constexpr int sleeps_us[] = {0, 1, 2, 3, 5, 7, 10, 20, 50, 200, 1000, 5000};

/// The pinger either sleeps on the primitive for the answer, or spins on a flag for it.
/// The second keeps the pinger's own core awake, so that the sleep in the column is the
/// only thing the ponger's wake can depend on -- and it is the shape of a caller that
/// is busy until the moment it needs the worker.
enum class answer
{
    sleeping,
    spinning
};

template <class signal_t>
stats measure(int samples, std::chrono::nanoseconds sleep, answer how, std::size_t pinger_cpu,
              std::size_t ponger_cpu) {
    signal_t to_ponger;
    signal_t to_pinger;
    std::atomic<bool> stop{false};
    std::atomic<bench_clock::time_point> woke_at{};
    std::atomic<std::uint32_t> answered{0};

    std::thread ponger([&] {
        wxl::async::thread::set_affinity(ponger_cpu);

        for (;;) {
            to_ponger.wait();

            if (stop.load(std::memory_order_acquire)) return;

            woke_at.store(bench_clock::now(), std::memory_order_release);

            if (how == answer::sleeping)
                to_pinger.set();
            else
                answered.store(1, std::memory_order_release);
        }
    });

    wxl::async::thread::set_affinity(pinger_cpu);

    const auto wait_for_answer = [&] {
        if (how == answer::sleeping) {
            to_pinger.wait();
        } else {
            while (answered.exchange(0, std::memory_order_acquire) == 0) wxl::core::cpu_pause();
        }
    };

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(samples));

    // A warm-up the table does not see: thread start, first touches, first wakes.
    for (int i = 0; i < 200; ++i) {
        to_ponger.set();
        wait_for_answer();
    }

    for (int i = 0; i < samples; ++i) {
        spin_for(sleep);

        const bench_clock::time_point set_at = bench_clock::now();
        to_ponger.set();
        wait_for_answer();

        latencies.push_back(nanoseconds(woke_at.load(std::memory_order_acquire) - set_at).count());
    }

    stop.store(true, std::memory_order_release);
    to_ponger.set();
    ponger.join();

    return summarize(std::move(latencies));
}

template <class signal_t>
void row(const char* name, int samples, answer how, std::size_t pinger_cpu,
         std::size_t ponger_cpu) {
    std::printf("  %-14s", name);

    for (const int sleep_us : sleeps_us) {
        const stats s = measure<signal_t>(samples, std::chrono::microseconds(sleep_us), how,
                                          pinger_cpu, ponger_cpu);
        std::printf(" %5.1f/%-5.1f", s.median / 1000.0, s.p90 / 1000.0);
    }

    std::putchar('\n');
}

void table(const char* title, int samples, answer how, std::size_t pinger_cpu,
           std::size_t ponger_cpu) {
    std::printf("%s\n\n", title);
    std::printf("  %-14s", "slept, us:");

    for (const int sleep_us : sleeps_us) std::printf(" %11d", sleep_us);

    std::printf("\n\n");

    row<event_signal>("event", samples, how, pinger_cpu, ponger_cpu);
    row<address_signal>("address", samples, how, pinger_cpu, ponger_cpu);
    row<atomic_signal>("std::atomic", samples, how, pinger_cpu, ponger_cpu);

    std::putchar('\n');
}

}  // namespace

int main(int argc, char** argv) {
    const int samples = argc > 1 ? std::atoi(argv[1]) : 3000;
    const std::size_t pinger_cpu = argc > 3 ? static_cast<std::size_t>(std::atoi(argv[2])) : 0;
    const std::size_t ponger_cpu = argc > 3 ? static_cast<std::size_t>(std::atoi(argv[3])) : 2;

    std::printf(
        "wake latency by primitive and by how long the waiter slept -- microseconds, median/p90 of "
        "%d wakes\n",
        samples);
    std::printf("pinger on cpu %zu, ponger on cpu %zu\n\n", pinger_cpu, ponger_cpu);

    table("the pinger sleeps for the answer on the same primitive (both cores go idle)", samples,
          answer::sleeping, pinger_cpu, ponger_cpu);
    table("the pinger spins for the answer (only the ponger's core goes idle)", samples,
          answer::spinning, pinger_cpu, ponger_cpu);

    std::printf(
        "  A wake is the time from the waker's call to the first instruction the woken thread\n"
        "  runs. The columns are how long the waiter had been asleep before that call: the\n"
        "  longer it slept, the deeper the idle state its core is in when the wake arrives.\n");

    return 0;
}
