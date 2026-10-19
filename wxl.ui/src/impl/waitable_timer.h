#pragma once

// The wait behind wxl::WaitableTimer, over any timer that has the four calls -- start,
// stop, and the pair that subscribes to its tick. The application's timer is the
// dispatcher queue's (WaitableTimer.h); a test hands in a timer of its own and raises
// the tick itself, as event_cancel_test does with an event.
//
// A synchronization timer, in the words of the Win32 object it is modelled on. Set, it
// is nonsignaled and counting; when its time is up it is signaled, and stays signaled
// until one wait takes that; set again before then, it is nonsignaled and counting
// again, the tick it had not given forgotten. Here restart() is the setting, and
// signal() is the owner saying the time is up now: signaled at once, and the timer
// stopped, so that nothing is left to come later. To the coroutine waiting the two are
// one and the same, and so is the cancellation of the token the timer was given: the
// wait ends, with nothing thrown and nothing to read, and the coroutine goes on to what
// it waited in order to do. Whether to wait again it asks the token itself.
//
// A wait begun while the timer is signaled does not suspend. That is what keeps a tick,
// or a signal(), that came while the coroutine was busy elsewhere -- in the write it
// does between two waits, say -- from being lost: it is kept for the next wait. A wait
// on an event works the other way round (event_awaitable.h), because an event's args
// are worthless once its handler has returned; a tick has none.
//
// The one end that differs is the application going down: then this wait, as every
// wait of wxl, ends with async::operation_canceled_exception -- so that a loop under no
// cancelled token cannot spin on a wait that no longer suspends.
//
// Who resumes the coroutine. A tick does so inside the tick handler, as an event does
// inside its handler. signal() does so inside the call: the owner said now, and the
// coroutine reads what it reads at that moment, before the caller goes on to change
// it. The token's cancellation resumes nobody -- the queue's next turn does
// (event_waits.h).

#include <coroutine>
#include <utility>

#include "../event_token.h"
#include "event_waits.h"

namespace wxl::impl {

template <typename Timer>
class waitable_timer : waiting_event
{
public:
    /// \param timer one-shot and stopped: restart() starts it, a tick and signal() stop it.
    /// \param stop the token every wait on this timer stands under; none by default.
    explicit waitable_timer(Timer timer, async::cancellation_token stop = {})
        : timer_(std::move(timer)), stop_(std::move(stop)) {
        token_ = timer_.add([this](auto&&...) { tick(); });
    }

    /// Unsubscribing and stopping can be refused by a runtime going down around the
    /// timer, and a destructor may not pass that on: the timer is going away either way.
    ~waitable_timer() {
        try {
            timer_.remove(token_);
            timer_.stop();
        } catch (...) {
        }
    }

    waitable_timer(waitable_timer const&) = delete;
    waitable_timer& operator=(waitable_timer const&) = delete;

    /// The interval begins again from now; a tick not yet taken is forgotten with it.
    void restart() {
        signaled_ = false;
        timer_.stop();
        timer_.start();
    }

    /// The time is up now: the timer is stopped, and the wait ends -- the one in
    /// progress, inside this call, or the next one, before it suspends.
    void signal() {
        timer_.stop();
        signaled_ = true;

        // Last statement: the coroutine may run to its end inside the call, and take
        // this object with it.
        if (waiting()) deliver();
    }

    /// One wait. It lives in the frame of the coroutine that wrote the co_await, and
    /// taking itself off the timer is what its destructor is for: a frame destroyed
    /// while suspended unhooks on the way out.
    class Wait
    {
    public:
        explicit Wait(waitable_timer& timer) noexcept : timer_(&timer) {}

        ~Wait() {
            if (armed_) timer_->disarm();
        }

        Wait(Wait const&) = delete;
        Wait& operator=(Wait const&) = delete;

        /// Signaled already, cancelled before it began, or the application going down:
        /// the wait does not suspend.
        bool await_ready() const noexcept {
            return timer_->signaled_ || canceled_ || events_closing();
        }

        // Declared twice, as every hook that checks how it is used is: a strict build's
        // takes the place of the co_await, any other build's has the signature it
        // always had.

        void await_suspend(std::coroutine_handle<> waiter) noexcept
            requires(!async::detail::strict)
        {
            timer_->arm(waiter);
            armed_ = true;
        }

        void await_suspend(std::coroutine_handle<> waiter,
                           async::detail::site where = async::detail::site::current()) noexcept
            requires(async::detail::strict)
        {
            timer_->arm(waiter, where);
            armed_ = true;
        }

        /// Takes the signal, if that is what ended the wait. Otherwise the application is
        /// going down, and the wait ends as every wait of wxl does then. A cancelled token
        /// never gets here: the wait under it answers that itself
        /// (async::detail::cancellable_wait, await_canceled() below).
        void await_resume() {
            armed_ = false;

            if (std::exchange(timer_->signaled_, false)) return;

            timer_->throw_ended();
        }

        /// Cancelled by the token (async::detail::cancellable_awaiter): a wait that is
        /// suspended is resumed by the queue's next turn, or by a tick or signal() if
        /// one comes first -- never by cancel() itself; one that has not begun never
        /// suspends.
        void cancel() noexcept {
            if (armed_) {
                timer_->cancel();
            } else {
                canceled_ = true;
            }
        }

        /// What the wait gives when its token was cancelled: nothing, as a tick gives
        /// nothing.
        void await_canceled() noexcept {}

    private:
        waitable_timer* timer_;

        // Never both: armed while suspended in the timer, cancelled before it suspended
        // and so never to be.
        bool armed_ = false;
        bool canceled_ = false;
    };

    using awaiter = event_wait_under<Wait, async::detail::cancellation_state*>;

    awaiter operator co_await() noexcept {
        return awaiter{async::detail::cancellation_state_of(stop_), *this};
    }

    // Waiting writes down who is waiting, so a const timer cannot be awaited.
    awaiter operator co_await() const = delete;

private:
    /// The interval is up. A one-shot timer is over when it ticks; stopped here rather
    /// than relied upon, so that a timer handed in repeating behaves the same.
    void tick() {
        timer_.stop();
        signaled_ = true;

        if (waiting()) deliver();  // last statement, as in signal()
    }

    Timer timer_;
    EventToken token_;
    async::cancellation_token stop_;

    /// The latch: the time was up, or the owner said so, and no wait has taken it yet.
    bool signaled_ = false;
};

}  // namespace wxl::impl
