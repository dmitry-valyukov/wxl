export module wxl.async:countdown_event;

import :future;
import :future_shared_state;
import wxl.core;
import std;

export namespace wxl::async {

/// Signals once when the counter, decremented by signal(), reaches zero.
class countdown_event : public core::noncopyable
{
public:
    explicit countdown_event(size_t initial_count = 0) noexcept : counter_(initial_count) {}

    void reset(size_t initial_count) {
        counter_.store(initial_count);
        ready_promise_ = promise<void>();
    }

    void signal() {
        if (--counter_ == 0) ready_promise_.set_value();
    }

    /// Blocks until the counter reaches zero.
    void wait() { ready_promise_.get_future().wait(); }

    /// Blocks until the counter reaches zero or \p timeout elapses.
    /// \return \c false only on timeout.
    bool wait_for(core::duration timeout) {
        return ready_promise_.get_future().wait_for(timeout) == future_status::ready;
    }

private:
    std::atomic<ssize_t> counter_;
    promise<void> ready_promise_;
};

}  // namespace wxl::async
