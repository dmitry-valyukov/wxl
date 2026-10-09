// What a co_await of one task from another costs on the path with no error, and what
// cancellation by request adds to it -- the token passed as an argument and named at
// the wait.
//
// One thread, no loop: a wait that stands for an operation is resumed by hand, so what
// is timed is the coroutines and nothing under them. Every row does n iterations of the
// same shape inside one outer task, and the best of the rounds is reported, in
// nanoseconds per iteration:
//
//   ended at once     co_await f(i), where f returns without suspending;
//   after a pause     co_await f(slot, i), where f suspends once and is resumed by hand;
//   three links       co_await of a task that co_awaits one that co_awaits the pause.
//
// Each shape is timed plain and under a token: the token is an argument copied into
// every frame of the chain, and the pause is `co_await cancellable(...)`, a wait that
// stands on the token's list while it is suspended. Nothing is cancelled: this is what
// asking costs those that are never asked.
//
// And one operation of this module's kind -- an async_op behind an awaitable, which the
// loop would resume and here a hand does (`come_back()`) -- awaited in its three forms:
// plain; the form with a token, `co_await read(..., stop)`, which the operations that
// can be cut short have; and `co_await cancellable(read(...), stop)`.
//
// The callees are noinline: a frame whose whole life the caller can see is elided onto
// the stack, and a benchmark of an elided frame measures nothing (see
// coroutine_frame_benchmark.cpp).
import std;
import wxl.core;
import wxl.async;

namespace {

using namespace wxl::async;

/// A wait resumed by hand, standing for an operation that comes back.
struct parked {
    std::coroutine_handle<>* slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { *slot = here; }
    void await_resume() const noexcept {}
};

/// The same wait, one a token can reach -- as an operation of this module can be.
struct parked_cancellable : parked {
    void cancel() noexcept {}
};

__declspec(noinline) task<int> at_once(int i) {
    co_return i;
}

__declspec(noinline) task<int> at_once_with(int i, cancellation_token) {
    co_return i;
}

__declspec(noinline) task<int> after_a_pause(std::coroutine_handle<>* slot, int i) {
    co_await parked_cancellable{{slot}};
    co_return i;
}

__declspec(noinline) task<int> after_a_pause_under(std::coroutine_handle<>* slot, int i,
                                                  cancellation_token stop) {
    co_await cancellable(parked_cancellable{{slot}}, stop);
    co_return i;
}

__declspec(noinline) task<int> second_link(std::coroutine_handle<>* slot, int i) {
    co_return co_await after_a_pause(slot, i);
}

__declspec(noinline) task<int> first_link(std::coroutine_handle<>* slot, int i) {
    co_return co_await second_link(slot, i);
}

__declspec(noinline) task<int> second_link_under(std::coroutine_handle<>* slot, int i,
                                                cancellation_token stop) {
    co_return co_await after_a_pause_under(slot, i, stop);
}

__declspec(noinline) task<int> first_link_under(std::coroutine_handle<>* slot, int i,
                                               cancellation_token stop) {
    co_return co_await second_link_under(slot, i, stop);
}

__declspec(noinline) task<long long> loop_at_once(int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await at_once(i);
    co_return sum;
}

__declspec(noinline) task<long long> loop_at_once_with(int n, cancellation_token stop) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await at_once_with(i, stop);
    co_return sum;
}

__declspec(noinline) task<long long> loop_pause(std::coroutine_handle<>* slot, int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await after_a_pause(slot, i);
    co_return sum;
}

__declspec(noinline) task<long long> loop_pause_under(std::coroutine_handle<>* slot, int n,
                                                     cancellation_token stop) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await after_a_pause_under(slot, i, stop);
    co_return sum;
}

__declspec(noinline) task<long long> loop_links(std::coroutine_handle<>* slot, int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await first_link(slot, i);
    co_return sum;
}

__declspec(noinline) task<long long> loop_links_under(std::coroutine_handle<>* slot, int n,
                                                     cancellation_token stop) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await first_link_under(slot, i, stop);
    co_return sum;
}

/// An operation that is over when it is made and is delivered by hand: what the loop
/// does when one comes back, without the loop.
class handed_op : public async_op_t<int>
{
public:
    explicit handed_op(int value) { set_value(int(value)); }

protected:
    bool execute() override { return true; }
};

awaitable<int> handed(handed_op*& slot, int i) {
    auto* const op = new handed_op(i);
    slot = op;
    return awaitable<int>(std::unique_ptr<async_op_t<int>>(op));
}

/// The form with a token, as the operations of async_file have it.
cancellable_awaitable<int> handed(handed_op*& slot, int i, cancellation_token stop) {
    return cancellable_awaitable<int>(std::move(stop), [&] { return handed(slot, i); });
}

__declspec(noinline) task<long long> loop_op(handed_op** slot, int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await handed(*slot, i);
    co_return sum;
}

__declspec(noinline) task<long long> loop_op_with(handed_op** slot, int n, cancellation_token stop) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await handed(*slot, i, stop);
    co_return sum;
}

__declspec(noinline) task<long long> loop_op_cancellable(handed_op** slot, int n, cancellation_token stop) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) sum += co_await cancellable(handed(*slot, i), stop);
    co_return sum;
}

constexpr int iterations = 2'000'000;
constexpr int rounds = 15;
constexpr long long expected = static_cast<long long>(iterations) * (iterations - 1) / 2;

/// Runs one shape to its end, resuming the pause by hand, and answers the time per
/// iteration -- or a negative number if the sum came out wrong.
template <class Start>
double time_one(Start start) {
    std::coroutine_handle<> slot;

    const auto from = std::chrono::steady_clock::now();
    task<long long> outer = start(&slot);
    while (!outer.done()) slot.resume();
    const auto to = std::chrono::steady_clock::now();

    if (outer.result() != expected) return -1;

    return std::chrono::duration<double, std::nano>(to - from).count() / iterations;
}

template <class Start>
double best_of(Start start) {
    double best = 1e30;
    for (int r = 0; r < rounds; ++r) best = std::min(best, time_one(start));
    return best;
}

/// The same for the operations: each one delivered by hand as it is awaited.
template <class Start>
double best_of_ops(Start start) {
    double best = 1e30;

    for (int r = 0; r < rounds; ++r) {
        handed_op* slot = nullptr;

        const auto from = std::chrono::steady_clock::now();
        task<long long> outer = start(&slot);
        while (!outer.done()) slot->come_back();
        const auto to = std::chrono::steady_clock::now();

        if (outer.result() != expected) return -1;

        best = std::min(best, std::chrono::duration<double, std::nano>(to - from).count() / iterations);
    }

    return best;
}

void row(const char* name, double plain, double under) {
    std::printf("  %-16s %8.2f %8.2f %+8.2f\n", name, plain, under, under - plain);
}

}  // namespace

int main() {
    cancellation_source stop;
    const cancellation_token token = stop.token();

    std::printf("explicit token: ns per iteration, plain and under a token\n");

    row("ended at once", best_of([](std::coroutine_handle<>*) { return loop_at_once(iterations); }),
        best_of([&](std::coroutine_handle<>*) { return loop_at_once_with(iterations, token); }));
    row("after a pause", best_of([](std::coroutine_handle<>* slot) { return loop_pause(slot, iterations); }),
        best_of([&](std::coroutine_handle<>* slot) { return loop_pause_under(slot, iterations, token); }));
    row("three links", best_of([](std::coroutine_handle<>* slot) { return loop_links(slot, iterations); }),
        best_of([&](std::coroutine_handle<>* slot) { return loop_links_under(slot, iterations, token); }));

    std::printf("an operation: ns per iteration, plain and under a token\n");

    const double plain = best_of_ops([](handed_op** slot) { return loop_op(slot, iterations); });

    row("its own form", plain, best_of_ops([&](handed_op** slot) { return loop_op_with(slot, iterations, token); }));
    row("cancellable()", plain,
        best_of_ops([&](handed_op** slot) { return loop_op_cancellable(slot, iterations, token); }));
}
