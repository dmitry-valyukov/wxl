#pragma once

// Every wait on an event that is in flight right now, in one list -- and the
// two-phase ending that list exists for.
//
// A coroutine waiting on an event is held by nothing: it is not a managed_task
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
// The node lives in the base below rather than in the proxy that derives from
// it, so one list holds waits on every event of every element without knowing
// any of their types -- and without a vtable, since ending a wait touches
// only what the base itself holds.

#include "../core.h"

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
    void arm(std::coroutine_handle<> waiter) noexcept {
        assert(!waiter_ && "wxl: two coroutines waiting on one event proxy");

        waiter_ = waiter;
        ended_ = false;
        error_ = {};
        waiting_events().push_back(core::as_not_null(this));
    }

    void disarm() noexcept {
        waiter_ = {};
        unlink();
    }

    bool waiting() const noexcept { return static_cast<bool>(waiter_); }

    bool ended() const noexcept { return ended_; }

    core::nullable<std::exception_ptr> const& error() const noexcept { return error_; }

    /// Resumes whoever is waiting because the event happened.
    void deliver() noexcept {
        unlink();
        std::exchange(waiter_, {}).resume();  // last statement, as above
    }

    ~waiting_event() {
        // A waiter still here means a coroutine is suspended on a
        // subscription that is going away, and there is nothing sound to
        // resume it with. It cannot happen where a proxy belongs -- in the
        // frame of the coroutine awaiting it, whose awaiter is younger and so
        // disarms first.
        assert(!waiter_ && "wxl: an event proxy outlived the coroutine waiting on it");
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

/// Phase one of going down: no wait suspends from here on, and every wait
/// suspended right now is told so. What each coroutine does about it -- roll
/// back, close, save -- runs from here, which is why this happens while the
/// message loop is still turning.
inline void close_event_waits() noexcept {
    events_closing() = true;

    auto& all = waiting_events();
    while (waiting_event* const waiting = all.front()) {
        waiting->end();
    }
}

/// Phase two: whether everything that was told has finished. The loop keeps
/// turning until this is true or the caller runs out of patience.
inline bool event_waits_drained() noexcept { return waiting_events().empty(); }

}  // namespace wxl::impl
