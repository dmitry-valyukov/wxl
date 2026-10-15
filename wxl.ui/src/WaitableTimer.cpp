#include "WaitableTimer.h"

namespace wxl {

namespace {

/// A one-shot timer of the queue with its interval set and nothing started.
DispatcherQueueTimer one_shot(DispatcherQueue const& queue, std::chrono::milliseconds interval) {
    DispatcherQueueTimer timer = queue.createTimer();
    timer.interval(interval);
    timer.isRepeating(false);
    return timer;
}

}  // namespace

WaitableTimer::WaitableTimer(DispatcherQueue const& queue, std::chrono::milliseconds interval,
                             async::cancellation_token stop)
    : impl::waitable_timer<impl::dispatcher_timer>({one_shot(queue, interval)}, std::move(stop)) {}

}  // namespace wxl
