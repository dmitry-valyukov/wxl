module;

#include "abi.h"

export module wxl.async:completion_counter;

import :future;
import :future_shared_state;
import wxl.core;
import std;

export namespace wxl::async {

/// Counts asynchronous operations that have not finished yet and resolves a future when
/// the last one does: start_one() registers an operation, complete_one() retires it, and
/// the future from on_all_completed() becomes ready the moment the count reaches zero.
///
/// **The default `expected` of 1 is the owner's own place, and the owner has to give it
/// up.** It is what keeps the future from resolving in the middle of registration -- with
/// a count of zero, the first operation to finish before its siblings are registered would
/// fire the future while work is still being handed out. So the owner starts holding one
/// place and calls complete_one() once it is done registering.
///
/// The counter fires once and is not reusable: the promise underneath is one-shot, and
/// starting an operation on a counter that has already fired is a contract violation
/// rather than a state to recover from.
class completion_counter : core::noncopyable
{
public:
    explicit completion_counter(ssize_t expected = 1) : counter_(expected) { ensure(expected > 0); }

    /// The future is resolved once every started operation has completed. Safe to ask for
    /// before or after that happens; a future taken afterwards is already ready.
    future<void> on_all_completed() const { return completed_promise_.get_future(); }

    /// Registers one more operation to wait for. The caller must hold a place of its own
    /// while doing so -- see the note on `expected` above.
    void start_one() {
        // One `lock xadd` and a look at what it returned. Landing on 1 means the counter
        // stood at zero, i.e. it has already fired -- and since ensure() does not return,
        // the count left behind never gets a chance to matter.
        const ssize_t outstanding = counter_.fetch_add(1, std::memory_order_relaxed) + 1;

        ensure(outstanding > 1);
    }

    /// Retires one operation, resolving the future if it was the last one.
    void complete_one() {
        // release, not acq_rel: every operation has to publish what it did before it
        // retired, but only one of them ever collects that -- the one that finds zero and
        // resolves the future. Paying for the acquire on every retirement buys nothing.
        const ssize_t outstanding = counter_.fetch_sub(1, std::memory_order_release) - 1;

        ensure(outstanding >= 0);

        if (outstanding == 0) {
            // Read the zero, then take everything that led to it. The fence pairs with the
            // releases above -- the fetch_sub read out of their release sequence -- and puts
            // the work of all the retired operations into this thread's history just before
            // set_value() publishes that history to the waiter. Synchronisation is
            // per-object, so the release inside set_value() cannot stand in for this one:
            // it orders `state_`, and says nothing about what was released on `counter_`.
            std::atomic_thread_fence(std::memory_order_acquire);
            completed_promise_.set_value();
        }
    }

private:
    std::atomic<ssize_t> counter_;
    promise<void> completed_promise_;
};

/// RAII place in a completion_counter: takes one on construction, gives it back on
/// destruction.
class completion_counter_guard : core::noncopyable
{
public:
    explicit completion_counter_guard(completion_counter& counter) : counter_(counter) {
        counter.start_one();
    }

    ~completion_counter_guard() { counter_.complete_one(); }

private:
    completion_counter& counter_;
};

}  // namespace wxl::async
