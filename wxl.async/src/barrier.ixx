module;

#include "abi.h"

export module wxl.async:barrier;

import :future;
import :future_shared_state;
import wxl.core;
import std;

export namespace wxl::async {

/// Threads enter the barrier and block until initial_count threads have entered.
class barrier : public core::noncopyable
{
public:
    inline explicit barrier(size_t initial_count) : counter_(initial_count) {
        assert(initial_count);
    }

    inline ~barrier() { assert(counter_.load() == 0); }

    /// The calling thread waits until initial_count threads have entered.
    inline void enter() {
        // local copy to make sure the shared state is safe to use until we leave this method.
        future<void> ready_signal = ready_promise_.get_future();

        if (--counter_ > 0)
            ready_signal.get();
        else
            ready_promise_.set_value();
    }

private:
    std::atomic<ssize_t> counter_;
    promise<void> ready_promise_;
};

}  // namespace wxl::async
