#pragma once

// wxl::WaitableTimer -- a wait on the thread's queue that ends when its interval is up,
// when its owner says now, or when its token is cancelled; and ends the same way each
// time, as far as the coroutine waiting can tell.
//
// For the work that follows a change after a quiet moment, and must still be done when
// there is no more waiting to be had: a position written to disk a second after the
// last page turned, written at once when the reader leaves the book, and written on
// the way out whatever the timer says.
//
//     WaitableTimer timer{window.dispatcherQueue(), 1500ms, source.token()};
//
//     async::task<> writes(WaitableTimer& timer, async::cancellation_token token) {
//         do {
//             co_await timer;
//             co_await save();
//         } while (!token.is_canceled());
//     }
//
//     timer.restart();   // a change: the interval begins again from now
//     timer.signal();    // now: the wait ends, and nothing is left to come later
//
// A tick or a signal() that comes while the coroutine is in its save is kept for the
// next wait, not lost; one that comes before a restart() is forgotten with it. The
// cancellation ends the wait on the queue's next turn, like nothing happened -- the
// coroutine saves once more and reads the token. How it is built, and what differs
// on the way down, is in impl/waitable_timer.h.

#include <chrono>
#include <utility>

#include <wxl/Microsoft.UI.Dispatching.h>
#include "impl/waitable_timer.h"

namespace wxl {

namespace impl {

/// DispatcherQueueTimer as the wait has it: the four calls, over the wrapper.
struct dispatcher_timer {
    DispatcherQueueTimer timer;

    template <typename Handler>
    EventToken add(Handler&& handler) const {
        return timer.add_onTick(std::forward<Handler>(handler));
    }

    void remove(EventToken& token) const { timer.remove_onTick(token); }
    void start() const { timer.start(); }
    void stop() const { timer.stop(); }
};

}  // namespace impl

class WaitableTimer : public impl::waitable_timer<impl::dispatcher_timer>
{
public:
    /// A one-shot timer of the queue, stopped until restart(). The token, given last as
    /// everywhere in wxl, is the one every wait on the timer is registered with.
    WaitableTimer(DispatcherQueue const& queue, std::chrono::milliseconds interval,
                  async::cancellation_token token = {});
};

}  // namespace wxl
