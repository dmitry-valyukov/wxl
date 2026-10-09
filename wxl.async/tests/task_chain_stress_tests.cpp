// A short, seeded version of the randomized stress of coroutine chains: tasks and
// detached tasks calling one another on the loop, with operations on the worker writing
// into the frames that started them, waits on a queue of this thread, exceptions between
// the waits, and owners dropping or replacing what they hold at random moments. The
// program of a round comes from its seed; the interleaving with the worker does not, and
// need not -- what is checked holds for any.
//
// What is checked is one rule, the one an ordinary stack keeps: by the time a frame is
// gone, nobody writes into it and nobody resumes it, and nothing is resumed twice. And
// every round ends with nothing left waiting, and with every frame and every operation
// it made gone.
#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// splitmix64: a round's program out of one number, a coroutine's out of the round's and
/// its own id, so that what one coroutine draws does not shift what the next one does.
class random_stream
{
public:
    explicit random_stream(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::uint32_t below(std::size_t n) {
        return n == 0 ? 0 : static_cast<std::uint32_t>(next() % n);
    }

    bool one_in(std::uint32_t n) { return below(n) == 0; }

private:
    std::uint64_t state_;
};

std::uint64_t seed_of(std::uint64_t round_seed, std::uint32_t id) {
    random_stream r(round_seed ^ (id * 0xD6E8FEB86659FD93ull));
    r.next();
    return r.next();
}

/// What a frame stands on between two deliveries.
enum class waits_on : std::uint8_t { nothing, operation, idle, task };

/// One frame of a round, as seen from outside the frame -- which is what is left to look
/// at once the frame is gone.
struct frame_record {
    /// Read by the worker before it writes into the frame.
    std::atomic<bool> alive{false};
    waits_on waiting = waits_on::nothing;
};

/// A task kept where a field would keep it: it outlives whoever awaits it.
struct stored_slot {
    std::optional<task<int>> held;
    std::uint32_t id = 0;
    bool joined = false;
};

class idle_wait;

struct round_state {
    static constexpr std::uint32_t max_frames = 240;
    static constexpr std::uint32_t max_depth = 5;
    static constexpr std::uint32_t max_steps = 6;
    static constexpr std::uint32_t max_top = 12;

    round_state(std::uint64_t round_seed, bool tasks_join)
        : seed(round_seed), joins_by_tasks(tasks_join) {}

    const std::uint64_t seed;

    /// Whether a task may await a stored task, or only a detached task may: a task can be
    /// dropped by its owner while it waits, a detached task cannot.
    const bool joins_by_tasks;

    std::array<frame_record, max_frames + max_top + 1> frames;
    std::uint32_t next_id = 1;

    /// The owner of the top-level tasks, the way an application's I/O keeps them.
    std::vector<task<int>> io;
    std::vector<std::unique_ptr<stored_slot>> stored;
    std::deque<std::shared_ptr<idle_wait*>> idle;

    int frames_alive = 0;
    int detached_alive = 0;
    int ops_alive = 0;
    int resumed_gone = 0;
    int resumed_not_waiting = 0;
    std::atomic<int> wrote_into_gone_frame{0};

    /// Room for two more frames below this depth: a step starts at most two.
    bool can_spawn(std::uint32_t depth) const { return depth < max_depth && next_id + 2 < max_frames; }

    std::uint32_t new_id() { return next_id++; }

    void suspending(std::uint32_t id, waits_on what) { frames[id].waiting = what; }

    /// The first thing a coroutine does after a co_await, in the frame it was resumed in.
    void resumed(std::uint32_t id) {
        if (!frames[id].alive.load(std::memory_order_relaxed))
            ++resumed_gone;
        else if (frames[id].waiting == waits_on::nothing)
            ++resumed_not_waiting;

        frames[id].waiting = waits_on::nothing;
    }

    bool anyone_waits_on(waits_on what) const {
        for (std::uint32_t id = 1; id < next_id; ++id)
            if (frames[id].alive.load(std::memory_order_relaxed) && frames[id].waiting == what)
                return true;
        return false;
    }

    bool anyone_waits() const {
        for (std::uint32_t id = 1; id < next_id; ++id)
            if (frames[id].alive.load(std::memory_order_relaxed) &&
                frames[id].waiting != waits_on::nothing)
                return true;
        return false;
    }

    /// A stored task started after the caller, never awaited yet: waits go from older to
    /// younger only, so no two coroutines ever wait on each other.
    stored_slot* joinable(std::uint32_t me, random_stream& r) {
        std::vector<stored_slot*> found;
        for (const auto& slot : stored)
            if (slot->id > me && slot->held && !slot->joined) found.push_back(slot.get());
        return found.empty() ? nullptr : found[r.below(found.size())];
    }
};

/// Lives first in every frame of the round, so it goes last.
class frame_witness
{
public:
    frame_witness(round_state& c, std::uint32_t id) : c_(c), id_(id) {
        c_.frames[id_].alive.store(true, std::memory_order_release);
        ++c_.frames_alive;
    }

    ~frame_witness() {
        c_.frames[id_].alive.store(false, std::memory_order_release);
        c_.frames[id_].waiting = waits_on::nothing;
        --c_.frames_alive;
    }

    frame_witness(const frame_witness&) = delete;
    frame_witness& operator=(const frame_witness&) = delete;

private:
    round_state& c_;
    std::uint32_t id_;
};

/// An operation that writes into the frame that started it, the way a read fills a
/// buffer there: a while of work, then the write, then a value or a failure. Half of
/// them stop early when given up, the way a cancelled read does; the other half run on.
class frame_write : public async_op_t<int>
{
public:
    frame_write(round_state& c, std::uint32_t frame, std::span<std::byte> into,
                std::uint32_t work, bool fails, bool stops_when_given_up)
        : c_(c), frame_(frame), into_(into), work_(work), fails_(fails),
          stops_when_given_up_(stops_when_given_up) {
        ++c_.ops_alive;
    }

    ~frame_write() override { --c_.ops_alive; }

protected:
    bool execute() override {
        for (std::uint32_t i = 0; i < work_ && !cut_short(); ++i) std::this_thread::yield();

        if (cut_short()) throw operation_canceled_exception();

        if (c_.frames[frame_].alive.load(std::memory_order_acquire))
            std::ranges::fill(into_, std::byte{42});
        else
            ++c_.wrote_into_gone_frame;

        if (fails_) throw std::runtime_error("the operation failed");

        set_value(1);
        return true;
    }

private:
    bool cut_short() const noexcept { return stops_when_given_up_ && canceled(); }

    round_state& c_;
    std::uint32_t frame_;
    std::span<std::byte> into_;
    std::uint32_t work_;
    bool fails_;
    bool stops_when_given_up_;
};

awaitable<int> start_write(round_state& c, std::uint32_t frame, std::span<std::byte> into,
                           random_stream& r) {
    const std::uint32_t work = r.one_in(4) ? 0 : r.below(64);
    const bool fails = r.one_in(12);
    const bool stops = r.one_in(2);

    return sta_loop::async_run(std::unique_ptr<async_op_t<int>>(
        new frame_write(c, frame, into, work, fails, stops)));
}

/// A wait for this thread to have nothing more urgent, delivered by the round from its
/// own queue. A frame destroyed while it waits leaves an empty entry behind.
class idle_wait
{
public:
    explicit idle_wait(round_state& c) : c_(c) {}

    ~idle_wait() {
        if (self_) *self_ = nullptr;
    }

    idle_wait(const idle_wait&) = delete;
    idle_wait& operator=(const idle_wait&) = delete;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> waiter) {
        self_ = std::make_shared<idle_wait*>(this);
        waiter_ = waiter;
        c_.idle.push_back(self_);
    }

    void await_resume() const noexcept {}

    void deliver() const { waiter_.resume(); }

private:
    round_state& c_;
    std::shared_ptr<idle_wait*> self_;
    std::coroutine_handle<> waiter_;
};

std::span<std::byte> piece_of(std::span<std::byte> buffer, std::uint32_t i) {
    constexpr std::size_t piece = 8;
    return buffer.subspan((i * piece) % buffer.size(), piece);
}

detached_task scenario(round_state& c, std::uint32_t id, std::uint32_t depth);

/// One link of a chain, its program drawn from the round's seed and its id.
task<int> chain_link(round_state& c, std::uint32_t id, std::uint32_t depth) {
    const frame_witness witness(c, id);
    random_stream r(seed_of(c.seed, id));
    std::array<std::byte, 64> buffer{};
    std::vector<awaitable<int>> later;
    std::vector<task<int>> locals;
    int sum = 0;

    const std::uint32_t steps = 1 + r.below(round_state::max_steps);

    try {
        for (std::uint32_t i = 0; i < steps; ++i) {
            const std::uint32_t what = r.below(14);

            if (what == 0) {
                c.suspending(id, waits_on::operation);
                sum += co_await start_write(c, id, piece_of(buffer, i), r);
                c.resumed(id);
            } else if (what == 1) {
                later.push_back(start_write(c, id, piece_of(buffer, i), r));
            } else if (what == 2 && !later.empty()) {
                const std::uint32_t k = r.below(later.size());
                c.suspending(id, waits_on::operation);
                sum += co_await later[k];
                c.resumed(id);
                later.erase(later.begin() + k);
            } else if (what == 3 && !later.empty()) {
                // Assigned over one never awaited: that one is given up.
                const std::uint32_t k = r.below(later.size());
                later[k] = start_write(c, id, piece_of(buffer, i), r);
            } else if (what == 4) {
                c.suspending(id, waits_on::idle);
                co_await idle_wait(c);
                c.resumed(id);
            } else if (what == 5 && c.can_spawn(depth)) {
                c.suspending(id, waits_on::task);
                sum += co_await chain_link(c, c.new_id(), depth + 1);
                c.resumed(id);
            } else if (what == 6 && c.can_spawn(depth)) {
                locals.push_back(chain_link(c, c.new_id(), depth + 1));
            } else if (what == 7 && !locals.empty()) {
                const std::uint32_t k = r.below(locals.size());
                c.suspending(id, waits_on::task);
                sum += co_await locals[k];
                c.resumed(id);
                locals.erase(locals.begin() + k);
            } else if (what == 8 && !locals.empty() && c.can_spawn(depth)) {
                // Assigned over one never awaited: that frame goes with the temporary.
                const std::uint32_t k = r.below(locals.size());
                locals[k] = chain_link(c, c.new_id(), depth + 1);
            } else if (what == 9 && c.can_spawn(depth)) {
                auto slot = std::make_unique<stored_slot>();
                slot->id = c.new_id();
                stored_slot& kept = *slot;
                c.stored.push_back(std::move(slot));
                kept.held.emplace(chain_link(c, kept.id, depth + 1));
            } else if (what == 10 && c.joins_by_tasks) {
                if (stored_slot* const slot = c.joinable(id, r)) {
                    slot->joined = true;
                    c.suspending(id, waits_on::task);
                    sum += co_await *slot->held;
                    c.resumed(id);
                }
            } else if (what == 11 && r.one_in(3)) {
                if (r.one_in(4)) throw operation_canceled_exception();
                throw std::runtime_error("between two waits");
            } else if (what == 12 && c.can_spawn(depth)) {
                scenario(c, c.new_id(), depth + 1);
            } else if (what == 13 && c.can_spawn(depth)) {
                // Two in flight, awaited in the order opposite to the one they started in.
                task<int> first = chain_link(c, c.new_id(), depth + 1);
                task<int> second = chain_link(c, c.new_id(), depth + 1);
                c.suspending(id, waits_on::task);
                sum += co_await second;
                c.resumed(id);
                c.suspending(id, waits_on::task);
                sum += co_await first;
                c.resumed(id);
            }
        }

        // Some of what is still out is awaited, in no particular order; the rest goes with
        // the frame.
        while (!later.empty() && r.one_in(2)) {
            const std::uint32_t k = r.below(later.size());
            c.suspending(id, waits_on::operation);
            sum += co_await later[k];
            c.resumed(id);
            later.erase(later.begin() + k);
        }

        while (!locals.empty() && r.one_in(2)) {
            const std::uint32_t k = r.below(locals.size());
            c.suspending(id, waits_on::task);
            sum += co_await locals[k];
            c.resumed(id);
            locals.erase(locals.begin() + k);
        }
    } catch (...) {
        if (r.one_in(2)) throw;
    }

    co_return sum;
}

/// Counts a detached task of the round for as long as its frame is there.
class detached_mark
{
public:
    explicit detached_mark(round_state& c) : c_(c) { ++c_.detached_alive; }
    ~detached_mark() { --c_.detached_alive; }

    detached_mark(const detached_mark&) = delete;
    detached_mark& operator=(const detached_mark&) = delete;

private:
    round_state& c_;
};

/// A detached task: joins stored tasks, awaits chains, and lets whatever it does not
/// catch go -- a cancellation into silence, anything else to the failure handler.
detached_task scenario(round_state& c, std::uint32_t id, std::uint32_t depth) {
    const frame_witness witness(c, id);
    const detached_mark mark(c);
    random_stream r(seed_of(c.seed, id));
    std::array<std::byte, 16> buffer{};

    const std::uint32_t steps = 1 + r.below(4);

    for (std::uint32_t i = 0; i < steps; ++i) {
        const std::uint32_t what = r.below(5);

        if (what == 0) {
            if (stored_slot* const slot = c.joinable(id, r)) {
                slot->joined = true;
                c.suspending(id, waits_on::task);
                co_await *slot->held;
                c.resumed(id);
            }
        } else if (what == 1 && c.can_spawn(depth)) {
            c.suspending(id, waits_on::task);
            co_await chain_link(c, c.new_id(), depth + 1);
            c.resumed(id);
        } else if (what == 2) {
            c.suspending(id, waits_on::operation);
            co_await start_write(c, id, piece_of(buffer, i), r);
            c.resumed(id);
        } else if (what == 3) {
            c.suspending(id, waits_on::idle);
            co_await idle_wait(c);
            c.resumed(id);
        } else if (what == 4 && r.one_in(4)) {
            if (r.one_in(2)) throw operation_canceled_exception();
            throw std::runtime_error("out of a detached task");
        }
    }
}

/// What the owner of the top-level tasks does before it keeps a new one: reads and lets
/// go of those that have ended.
void sweep(round_state& c) {
    std::erase_if(c.io, [](task<int>& t) {
        if (!t.done()) return false;

        try {
            static_cast<void>(t.result());
        } catch (...) {
        }

        return true;
    });
}

void start_top(round_state& c, random_stream& r) {
    const std::uint32_t id = c.new_id();

    if (r.one_in(3)) {
        scenario(c, id, 0);
    } else {
        sweep(c);
        c.io.push_back(chain_link(c, id, 0));
    }
}

/// What happens to the round's tasks from outside every chain, between two deliveries:
/// a top-level task dropped, the ended ones swept, a stored one dropped or assigned over
/// -- never one somebody awaits.
void drive(round_state& c, random_stream& r) {
    const std::uint32_t what = r.below(4);

    if (what == 0 && !c.io.empty()) {
        c.io.erase(c.io.begin() + r.below(c.io.size()));
    } else if (what == 1) {
        sweep(c);
    } else if (what >= 2 && !c.stored.empty()) {
        stored_slot& slot = *c.stored[r.below(c.stored.size())];

        if (!slot.held || slot.joined) return;

        if (what == 2) {
            slot.held.reset();
        } else if (c.can_spawn(1)) {
            slot.id = c.new_id();
            *slot.held = chain_link(c, slot.id, 1);
        }
    }
}

/// Runs a round until nothing waits: starts the top-level coroutines at random moments,
/// drives the round from outside, and delivers what comes back from the worker or comes
/// up in the idle queue.
/// \return `false` if a coroutine was left waiting on nothing that could resume it.
bool run_round(round_state& c) {
    random_stream r(seed_of(c.seed, 0));
    const std::uint32_t top = 1 + r.below(round_state::max_top);
    std::uint32_t started = 0;

    for (;;) {
        if (started < top && r.one_in(3)) {
            start_top(c, r);
            ++started;
            continue;
        }

        if (r.one_in(10)) drive(c, r);

        if (r.one_in(2) && sta_loop::run_pending() != 0) continue;

        if (!c.idle.empty()) {
            const std::shared_ptr<idle_wait*> entry = std::move(c.idle.front());
            c.idle.pop_front();

            if (idle_wait* const wait = *entry) wait->deliver();
            continue;
        }

        if (c.anyone_waits_on(waits_on::operation)) {
            sta_loop::run_one();
            continue;
        }

        if (c.anyone_waits()) return false;

        if (started < top) {
            start_top(c, r);
            ++started;
            continue;
        }

        return true;
    }
}

/// What the failure handler saw: it is a bare function pointer, with nowhere of its own
/// to put it.
int detached_failures = 0;

class TaskChainStressTest : public ::testing::Test
{
protected:
    void SetUp() override {
        detached_failures = 0;
        previous_ = on_detached_task_failure();
        on_detached_task_failure() = [](std::exception_ptr) noexcept { ++detached_failures; };
    }

    void TearDown() override { on_detached_task_failure() = previous_; }

    /// Runs the rounds and stops at the first that breaks the rule, naming it.
    static void run_rounds(std::uint64_t seed, int rounds, bool joins_by_tasks) {
        for (int i = 0; i < rounds; ++i) {
            const std::uint64_t round_seed = seed_of(seed, static_cast<std::uint32_t>(i) + 1);
            auto c = std::make_unique<round_state>(round_seed, joins_by_tasks);

            if (!run_round(*c)) {
                // What is left waiting still names the round, which is therefore kept.
                static_cast<void>(c.release());
                FAIL() << "round " << i << ": a coroutine was left waiting on nothing";
            }

            sweep(*c);
            c->io.clear();
            for (auto& slot : c->stored) slot->held.reset();
            c->stored.clear();

            // Given-up operations wait in the return channel for the loop to delete them.
            EXPECT_EQ(sta_loop::run_pending(), 0u) << "round " << i << ": a given-up operation resumed somebody";

            ASSERT_EQ(c->wrote_into_gone_frame.load(), 0) << "round " << i << ": an operation wrote into a frame that was gone";
            ASSERT_EQ(c->resumed_gone, 0) << "round " << i << ": a frame was resumed after it was gone";
            ASSERT_EQ(c->resumed_not_waiting, 0) << "round " << i << ": a frame was resumed while not waiting";
            ASSERT_EQ(c->detached_alive, 0) << "round " << i << ": a detached task never ended";
            ASSERT_EQ(c->frames_alive, 0) << "round " << i << ": a frame outlived the round";
            ASSERT_EQ(c->ops_alive, 0) << "round " << i << ": an operation outlived the round";
            ASSERT_TRUE(c->idle.empty());
        }
    }

private:
    detached_task_failure_handler previous_ = nullptr;
};

constexpr std::uint64_t stress_seed = 20261009;
constexpr int stress_rounds = 300;

}  // namespace

TEST_F(TaskChainStressTest, ChainsKeepTheirFramesWhateverHappensToThem) {
    run_rounds(stress_seed, stress_rounds, false);
}

// A task that awaits a stored one may be dropped by its owner while it waits: its awaiter
// takes it off the stored task on the way out, so the stored one, ending, resumes nobody.
TEST_F(TaskChainStressTest, AJoinerDroppedFirstIsNotResumed) {
    run_rounds(stress_seed, stress_rounds, true);
}
