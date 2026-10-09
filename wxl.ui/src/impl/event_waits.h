#pragma once

// Every wait on an event that is in flight right now, in one list -- and the
// two-phase ending that list exists for.
//
// A coroutine waiting on an event is held by nothing: it is not a task
// somebody keeps, it is a frame suspended inside a subscription. That is what
// makes it pleasant to write and what makes ending it a problem, because
// there is no handle to let go of. This is the answer: a wait puts itself in
// the list while it is suspended and takes itself out when it resumes, so the
// list is exactly the set of coroutines that are waiting for something to
// happen -- and on the way down, exactly the set that has to be told it will
// not.
//
// Ending is not destroying, and the difference matters. A coroutine may have
// a transaction to roll back or a socket to close politely, and that is
// asynchronous work: it cannot happen in a destructor, only in the body,
// which means the body has to be given back control rather than taken apart.
// So a wait ends by resuming its coroutine and telling it the news, and the
// application's exit is the same two phases wxl already uses for a thread:
// close the turnstile, then let what is inside finish.
//
// A wait can also be asked to end, through a cancellation token it was put
// under (`co_await onClick(button, stop)`). Asking resumes nobody -- whoever
// cancels the token is not re-entered by the coroutines it asked -- so a wait
// that is told moves to a second list, of waits told and not yet resumed, and
// the thread's queue resumes them on its next turn: the road the event itself
// comes by. An event that comes first resumes its told wait itself, as it
// would have anyway; either way the coroutine ends as cancelled.
//
// The node lives in the base below rather than in the proxy that derives from
// it, so one list holds waits on every event of every element without knowing
// any of their types -- and without a vtable, since ending a wait touches
// only what the base itself holds.

#include "../core.h"

#include "coroutine_checks.h"

import wxl.async;

namespace wxl::impl {

class waiting_event;

/// Every wait suspended right now. A function-local static rather than an
/// object at namespace scope: the nodes live inside coroutine frames, so the
/// list allocates nothing and only has to outlive them.
inline core::intrusive_list<waiting_event>& waiting_events() noexcept {
    static core::intrusive_list<waiting_event> all;
    return all;
}

/// The waits that were told and are still suspended, until the queue's turn
/// resumes them.
inline core::intrusive_list<waiting_event>& told_waits() noexcept {
    static core::intrusive_list<waiting_event> told;
    return told;
}

/// Whether a turn that resumes the told waits is in the queue already: one
/// turn serves every wait told before it comes.
inline bool& told_waits_posted() noexcept {
    static bool posted = false;
    return posted;
}

/// Asks the thread's queue for a turn that calls resume_told_waits(), at the
/// priority an event comes at. False: the queue takes nothing more -- the
/// application is going down, and close_event_waits() ends the told waits with
/// the rest. Defined beside the queue, in UiThread.cpp.
bool post_told_waits() noexcept;

/// Set once the application has begun going down. After that a wait does not
/// suspend at all -- it ends where it stands, which is what keeps a coroutine
/// that catches cancellation and waits again from being ended forever.
inline bool& events_closing() noexcept {
    static bool closing = false;
    return closing;
}

/// The part of an event proxy that does not depend on the event: who is
/// waiting, and how the wait ended if it was not by the event happening.
class waiting_event : public core::intrusive_list_node<waiting_event>
{
public:
    /// Resumes the coroutine and tells it that what it waited for will not
    /// come. \p error carries a failure to report as it stands; empty means
    /// there is nothing to say beyond "cancelled".
    void end(core::nullable<std::exception_ptr> error = {}) noexcept {
        if (!waiter_) {
            return;
        }

        ended_ = true;
        error_ = error;

        // Last statement, always: the coroutine may run to its end inside
        // this call, and this object -- a local of its frame -- is destroyed
        // before it gets there.
        unlink();
        std::exchange(waiter_, {}).resume();
    }

protected:
    /// \param where the co_await, which a build that checks coroutines names
    ///        when the wait breaks the rule.
    void arm(std::coroutine_handle<> waiter,
             [[maybe_unused]] async::coro_detail::site where = async::coro_detail::site::current()) noexcept {
        coro_check(!waiter_, "wxl: two coroutines waiting on one event proxy", where);

        waiter_ = waiter;
        ended_ = false;
        error_ = {};
        waiting_events().push_back(core::as_not_null(this));
    }

    void disarm() noexcept {
        waiter_ = {};
        unlink();
    }

    /// Whether a coroutine is suspended here -- told or not.
    bool waiting() const noexcept { return static_cast<bool>(waiter_); }

    bool ended() const noexcept { return ended_; }

    core::nullable<std::exception_ptr> const& error() const noexcept { return error_; }

    /// Resumes whoever is waiting because the event happened.
    void deliver() noexcept {
        unlink();
        std::exchange(waiter_, {}).resume();  // last statement, as above
    }

    /// Hands the suspended wait to the queue's next turn, which resumes it as
    /// plainly cancelled; whoever told it goes on undisturbed. Until then it is
    /// still the event's: an event that comes first resumes it, and a wait
    /// under a cancelled token ends as cancelled whatever resumed it -- its
    /// args unread, the event unanswered and travelling on. A turn the queue
    /// will not give is made up for on the way down, with the rest.
    void tell() noexcept {
        unlink();
        told_waits().push_back(core::as_not_null(this));

        if (!told_waits_posted()) told_waits_posted() = post_told_waits();
    }

    ~waiting_event() {
        // A waiter still here means a coroutine is suspended on a
        // subscription that is going away, and there is nothing sound to
        // resume it with. It cannot happen where a proxy belongs -- in the
        // frame of the coroutine awaiting it, whose awaiter is younger and so
        // disarms first.
        coro_check(!waiter_, "wxl: an event proxy outlived the coroutine waiting on it",
                   async::coro_detail::site::current());
    }

    /// What the throwing form of a wait does when the wait ended instead.
    [[noreturn]] void throw_ended() const {
        if (error_) {
            std::rethrow_exception(*error_);
        }

        throw async::operation_canceled_exception{
            "wxl: the event this coroutine waited for will not come"};
    }

private:
    void unlink() noexcept { core::intrusive_list<waiting_event>::remove(core::as_not_null(this)); }

    std::coroutine_handle<> waiter_;
    bool ended_ = false;
    core::nullable<std::exception_ptr> error_;
};

/// The queue's turn: every wait told since the last one is resumed, each taken
/// off the list first, and ends as cancelled. A coroutine resumed here may tell
/// more; they are resumed in the same turn.
inline void resume_told_waits() noexcept {
    told_waits_posted() = false;

    auto& told = told_waits();
    while (waiting_event* const waiting = told.front()) {
        waiting->end();
    }
}

/// Phase one of going down: no wait suspends from here on, and every wait
/// suspended right now is told so -- the told ones that the queue has not
/// resumed yet among them, since it will not. What each coroutine does about
/// it -- roll back, close, save -- runs from here, which is why this happens
/// while the message loop is still turning.
inline void close_event_waits() noexcept {
    events_closing() = true;

    for (auto* const list : {&told_waits(), &waiting_events()}) {
        while (waiting_event* const waiting = list->front()) {
            waiting->end();
        }
    }
}

/// Phase two: whether everything that was told has finished. The loop keeps
/// turning until this is true or the caller runs out of patience.
inline bool event_waits_drained() noexcept { return waiting_events().empty() && told_waits().empty(); }

/// A wait of this thread under a cancellation token: the awaiter made in place,
/// what async::cancellation_detail::wait_under does with it. `Stop` is how the
/// token is had -- held (`async::cancellation_token`), for a wait made by a call
/// that was handed the token, or borrowed as its state, for the wait of a proxy
/// that holds the token and outlives the co_await.
///
/// Neither copied nor moved, like the awaiter: it is co_awaited where it is made.
template <typename Awaiter, typename Stop>
class [[nodiscard("a wait under a token does nothing until it is co_awaited")]] event_wait_under
    : public async::cancellation_detail::wait_under<event_wait_under<Awaiter, Stop>, true>
{
    using base = async::cancellation_detail::wait_under<event_wait_under, true>;

    friend base;

public:
    template <typename... Args>
    explicit event_wait_under(Stop stop, Args&&... args) noexcept
        : stop_(std::move(stop)), awaiter_(std::forward<Args>(args)...) {}

    event_wait_under(event_wait_under const&) = delete;
    event_wait_under& operator=(event_wait_under const&) = delete;

    /// A frame destroyed while it stands here takes the wait off the token's
    /// list before the token it may hold goes.
    ~event_wait_under() { this->leave(); }

private:
    Awaiter& awaiter() noexcept { return awaiter_; }

    async::cancellation_detail::cancellation_state* state() const noexcept {
        if constexpr (std::is_same_v<Stop, async::cancellation_token>)
            return async::cancellation_detail::state_of(stop_);
        else
            return stop_;
    }

    Stop stop_;
    Awaiter awaiter_;
};

}  // namespace wxl::impl
