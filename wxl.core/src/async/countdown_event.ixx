export module wxl.core:async.countdown_event;

import :async.future;
import :async.future_shared_state;
import wxl.stdint;
import :noncopyable;
import :time;
import std;

export namespace wxl::async {

/// Signals once when the counter, decremented by signal(), reaches zero.
class countdown_event : public core::noncopyable
{
public:
    /// Constructs the event with the counter set to \p initial_count.
    inline explicit countdown_event(size_t initial_count = 0) noexcept : counter_(initial_count) {}

    /// Sets the counter to \p initial_count and replaces the promise with a new one, so that
    /// the event can signal again.
    inline void reset(size_t initial_count) {
        counter_.store(initial_count);
        ready_promise_ = promise<void>();
    }

    /// Decrements the counter, and signals the event when the counter reaches zero.
    inline void signal() {
        if (--counter_ == 0) ready_promise_.set_value();
    }

    /// Blocks until the counter reaches zero.
    inline void wait() { ready_promise_.get_future().wait(); }

    /// Blocks until the counter reaches zero or \p timeout elapses.
    /// \return \c false only on timeout.
    inline bool wait_for(core::duration timeout) {
        return ready_promise_.get_future().wait_for(timeout) == future_status::ready;
    }

private:
    std::atomic<ssize_t> counter_;
    promise<void> ready_promise_;
};

}  // namespace wxl::async
