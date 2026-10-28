module;

#include "coroutine_checks.h"

export module wxl.core:async.cancellation;

import :async.coroutine_checks;
import :async.sta_thread;
import :compressed_optional;
import :event;
import :intrusive_ptr;
import :not_null;
import :observable;
import :refcounted;
import std;

export namespace wxl::async {

/// The operation was cancelled.
///
/// Lives here rather than with the component model that used to own it: an
/// asynchronous operation may be cancelled without any component being
/// involved, and the layer that hands out the right to cancel is the one that
/// has to name the outcome.
class operation_canceled_exception : public std::runtime_error
{
    using base = std::runtime_error;

public:
    inline operation_canceled_exception() : base("Operation was canceled") {}
    /// Constructs the exception with the explanatory string \p message.
    inline explicit operation_canceled_exception(const char* message) : base(message) {}
    /// Constructs the exception with a copy of \p message as the explanatory string.
    inline explicit operation_canceled_exception(std::string_view message)
        : base(std::string(message)) {}
};

class cancellation_token;

namespace detail {

/// What fires when a token is cancelled: an event of no arguments. Whoever waits under a
/// token -- an operation or a wait of wxl -- builds its registration into itself
/// (cancellation_registration), so that registering allocates nothing; it registers while it
/// waits and unregisters when it is over. A child state, one per source, registers as a
/// callback the event makes.
using cancellation_event = core::event<void() noexcept>;

class cancellation_state;

using cancellation_state_ptr = core::intrusive_ptr<cancellation_state>;

/// What a source and its tokens share: the answer, and its event -- and, for
/// a source made under a parent token, its own place in the parent's event. Whoever is
/// registered with the token registers and unregisters itself, through the event, and the state
/// knows none of them: a parent knows nothing of its children either.
///
/// One thread, the one the coroutines are on: an operation that is cancelled sets its own
/// flag, the one the worker reads before the body anyway, so the answer is a plain field
/// and the event an STA one.
class cancellation_state : public core::sta_refcounted
{
public:
    /// A state of its own: cancelled by its source alone.
    cancellation_state() noexcept = default;

    /// A state under the state of a parent token, or under none (null): cancelled by its own
    /// source, or by the parent's -- whichever comes first. Under a live parent it is registered
    /// with the parent's event as any subscriber is, a callback that cancels it, and holds the
    /// parent, so that the event outlives the registration and a chain of generations holds
    /// together; the parent holds nothing of it. Under a parent cancelled already it is born
    /// cancelled and registered nowhere.
    inline explicit cancellation_state(cancellation_state_ptr parent) {
        if (!parent) return;

        if (parent->canceled()) {
            canceled_ = true;
            return;
        }

        // By a plain pointer: the node lives in the parent's event, and a count there would
        // keep this state alive as long as the parent, and the parent as long as this one.
        subscription_ = parent->event().add([this]() noexcept { cancel(); });
        parent_ = std::move(parent);
    }

    /// Takes its callback out of the parent's event: here and nowhere else -- its own
    /// cancel() keeps it registered, and the parent cancelling it again is cancelling it twice.
    /// One thread makes, cancels and destroys a state, and cancelling destroys nobody, so its
    /// end never falls inside the parent's fire, and the destructor is all it takes to leave:
    /// no second count of who may still call it.
    ///
    /// A parent cancelled meanwhile has emptied its event into the one it fired and let go of
    /// the callback, so remove() finds nothing and answers false: the list compares the
    /// token's address and never reads through it, and since nobody subscribes under a
    /// cancelled parent, no later node there can have taken that address.
    inline ~cancellation_state() override {
        if (parent_) parent_->event().remove(subscription_);
    }

    inline bool canceled() const noexcept { return canceled_; }

    /// The event a subscriber adds itself to and removes itself from.
    inline cancellation_event& event() noexcept { return event_; }

    /// Sets the answer and cancels everyone registered, once: the event is emptied into a local
    /// one and fired there, so a subscriber that unsubscribes later -- or from inside its
    /// call -- finds it gone, and remove() or unlink() answers false. Cancelling resumes
    /// nobody (an operation is aborted, a wait is handed to its thread's queue), so no
    /// subscriber goes away in the middle of the fire. One that would register afterwards
    /// finds the answer and does not register at all, so a state cancelled already has nobody to
    /// cancel, and asking it again does nothing. A child state cancelled here cancels its own
    /// the same way, from inside this fire, and goes nowhere either.
    inline void cancel() noexcept {
        if (canceled_) return;

        canceled_ = true;

        cancellation_event once;
        once.swap(event_);
        once();
    }

private:
    bool canceled_ = false;
    cancellation_event event_;

    /// The parent's state and the token of the callback registered with its event, while
    /// this state is registered there; both empty for a state of its own.
    cancellation_state_ptr parent_;
    core::event_token subscription_;
};

/// The state behind a token, for whoever is registered with it; null for a token that has none.
cancellation_state* cancellation_state_of(const cancellation_token& token) noexcept;

}  // namespace detail

/// A callback registered with a token's cancellation, built into its owner -- what .NET calls a
/// CancellationTokenRegistration. `Callback`, a noexcept member of the owner, is called when the
/// token is cancelled, from inside the cancel; it resumes nobody. Registering allocates nothing:
/// the registration is the node of the token's event, added by register_with() and unlinked by
/// unregister(), and the owner keeps it, so the event's release() of it does nothing. Not
/// copied: the event holds it by address.
template <auto Callback>
class cancellation_registration final : public detail::cancellation_event::func_t
{
    using owner_t = typename core::impl::member_owner<decltype(Callback)>::type;

public:
    /// Constructs a registration, not registered yet, that calls `Callback` on \p owner.
    inline explicit cancellation_registration(owner_t& owner) noexcept : owner_(owner) {}

    cancellation_registration(const cancellation_registration&) = delete;
    cancellation_registration& operator=(const cancellation_registration&) = delete;

    /// Registers with the event of `state`, a state not cancelled yet.
    inline void register_with(detail::cancellation_state& state) noexcept {
        // Checked here and not in the class: the registration is declared inside its owner,
        // which is not complete there, and a member cannot be called on it yet.
        static_assert(std::is_nothrow_invocable_v<decltype(Callback), owner_t&>,
                      "wxl: a cancellation registration calls its owner from inside the cancel, "
                      "which has nobody to hand an exception to -- name a noexcept member");
        state.event().add(core::as_not_null<detail::cancellation_event::func_t>(this));
    }

    /// Out of the event of `state`. Once the token has been cancelled the event the
    /// registration was registered with has been fired and is gone, and unlink() finds nothing.
    inline void unregister(detail::cancellation_state& state) noexcept {
        state.event().unlink(static_cast<detail::cancellation_event::func_t&>(*this));
    }

private:
    inline void operator()() noexcept override { (owner_.*Callback)(); }

    /// The owner owns the registration.
    inline void release() noexcept override {}

    owner_t& owner_;
};

namespace detail {

/// What a wait of wxl under a token asks of its awaiter: `cancel() noexcept`, which asks
/// it to end its wait early. Cancelled, it still ends the way it ends anyway -- the event
/// ends, the thread's turn comes -- only sooner; and it is cancelled on the coroutine's own
/// thread, while the coroutine is suspended in it or just before it would be, and resumes
/// nobody from inside the call. A cancelled awaiter is then left without its await_resume():
/// whatever it would have handed over is not asked for. Cancelled before it suspends, it does
/// not suspend. A form that answers rather than throws says what a cancelled wait answers
/// through `await_canceled()`, which the wait then gives instead of the exception.
template <class Awaiter>
concept cancelable_awaiter = requires(Awaiter& awaiter) {
    { awaiter.cancel() } noexcept;
};

/// The hooks of a wait of wxl under a token, over what `Wait` gives them: `awaiter()`, the
/// awaiter of what is awaited (`cancelable_awaiter`), and `state()`, the token's state
/// or null.
///
/// Until the token is cancelled the wait is the awaiter's own, and is registered with the
/// token while the coroutine is suspended: its registration -- built in, nothing is allocated --
/// is added to the token's event when it suspends and removed when it resumes. Once the token is
/// cancelled the wait ends with operation_canceled_exception: one registered is cancelled
/// through the awaiter's cancel() and ends when the awaiter lets it; one that would begin
/// later is cancelled before it suspends, and does not. One that answers a cancellation
/// rather than throwing it -- the answering form of an event wait -- gives its answer through
/// await_canceled().
///
/// Each hook takes the place of the co_await and hands it on to an awaiter that takes
/// one, so that a strict build reports the line of the co_await rather than this file's.
/// `Wait` leaves the event in its own destructor, before its members go: one of them may
/// be what keeps the state alive.
template <class Wait>
class cancelable_wait
{
public:
    inline bool await_ready(detail::site where = detail::site::current()) {
        if (cancellation_state* const state = self().state(); state && state->canceled())
            [[unlikely]]
            self().awaiter().cancel();

        return ready(where);
    }

    template <class Promise>
    inline decltype(auto) await_suspend(
        std::coroutine_handle<Promise> awaiting,
        [[maybe_unused]] detail::site where = detail::site::current()) {
        if (cancellation_state* const state = self().state(); state && !state->canceled()) {
            registration_.register_with(*state);
            registered_ = true;
        }

        if constexpr (requires { self().awaiter().await_suspend(awaiting, where); })
            return self().awaiter().await_suspend(awaiting, where);
        else
            return self().awaiter().await_suspend(awaiting);
    }

    inline decltype(auto) await_resume(
        [[maybe_unused]] detail::site where = detail::site::current()) {
        unregister_from_token();

        if (cancellation_state* const state = self().state(); state && state->canceled())
            [[unlikely]] {
            if constexpr (requires { self().awaiter().await_canceled(); })
                return self().awaiter().await_canceled();
            else
                throw operation_canceled_exception();
        }

        if constexpr (requires { self().awaiter().await_resume(where); })
            return self().awaiter().await_resume(where);
        else
            return self().awaiter().await_resume();
    }

protected:
    inline cancelable_wait() noexcept : registration_(*this) {}

    ~cancelable_wait() = default;

    /// Takes the wait out of the token's event, if it is registered there.
    inline void unregister_from_token() noexcept {
        if (registered_) {
            registered_ = false;
            registration_.unregister(*self().state());
        }
    }

private:
    /// The token was cancelled: the event the wait was registered with is gone, and the awaiter
    /// is cancelled.
    inline void on_token_canceled() noexcept {
        registered_ = false;
        self().awaiter().cancel();
    }

    inline Wait& self() noexcept { return static_cast<Wait&>(*this); }

    inline bool ready([[maybe_unused]] detail::site where) {
        if constexpr (requires { self().awaiter().await_ready(where); })
            return self().awaiter().await_ready(where);
        else
            return self().awaiter().await_ready();
    }

    /// The wait's place in the token's event, built into it.
    cancellation_registration<&cancelable_wait::on_token_canceled> registration_;
    bool registered_ = false;
};

}  // namespace detail

/// The right to ask the coroutines that were given it to end.
///
/// A token is passed down a chain of coroutines explicitly, as an argument, and read where
/// the chain's own code reads it. Two kinds of reader:
///
/// - **The operations and waits of wxl** that can be aborted take it as their last
///   argument -- `co_await async_file::read_all(path, token)`, `co_await onClick(button,
///   token)` -- and are registered with it while they are out: asked, the operation is aborted
///   and the wait is ended, and the co_await ends with operation_canceled_exception. That
///   is the answer of what was aborted or never started; what got there first -- an
///   operation already done, an event already come -- answers with its own result. Under
///   a token cancelled already nothing is started, and the co_await ends with it at once.
/// - **The application's own coroutine** asks it itself -- is_canceled(),
///   throw_if_canceled() -- where it has something to stop, and decides what to do about
///   it. A body of its own handed to sta_loop under a token is not broken off from outside
///   either: one the loop has not started is not started; one on the worker runs to its end
///   and is waited for; an orphanable one is let go of -- the co_await ends with the
///   cancellation at once, and the body, left to finish alone, has the synchronous call to
///   the system it stands in cancelled and decides itself whether to stop between its steps,
///   by asking its orphan_state. Some work must not be broken off, and only its own code
///   knows which. There is no wrapper that puts an awaiter of somebody else's under a token.
///
/// Asking does not end anything by itself: the chain unwinds by its own exceptions,
/// through its own catch blocks, on the live thread -- and cleanup past a handler may
/// co_await again, under no token or another. A chain that ignores the answer runs on;
/// destroying it is still its owner's to do.
///
/// What a token does not do is interrupt a call already under way that has nothing to
/// interrupt it with: the grain is one operation or one wait.
///
/// One thread: tokens and their source live on the thread the coroutines run on. A
/// default-constructed token is never cancelled and costs nothing: it has no state,
/// and asking is a comparison against a null pointer.
class cancellation_token
{
public:
    cancellation_token() noexcept = default;

    /// Tests whether cancellation has been requested. A default-constructed token returns
    /// `false`.
    inline bool is_canceled() const noexcept { return state_ && state_->canceled(); }

    /// \throw operation_canceled_exception if cancellation has been requested.
    inline void throw_if_canceled() const {
        if (is_canceled()) throw operation_canceled_exception();
    }

private:
    friend class cancellation_source;
    friend detail::cancellation_state* detail::cancellation_state_of(
        const cancellation_token&) noexcept;

    inline explicit cancellation_token(detail::cancellation_state* state) noexcept
        : state_(state) {}

    detail::cancellation_state_ptr state_;
};

inline detail::cancellation_state* detail::cancellation_state_of(
    const cancellation_token& token) noexcept {
    return token.state_.get();
}

/// The other end of a token: the one that asks -- and the right to the work under it, for
/// as long as it lives.
///
/// **A source that goes cancels.** Its owner gone, nobody wants the work any more: the
/// destructor does what cancel() does, so every operation and wait registered with its
/// tokens is aborted, every later one is answered at once, and every source made under
/// them is cancelled in turn. Work that has ended has nothing registered, and the cancellation
/// is a flag written and an empty event fired. So the source lives exactly as long as the
/// right to the work: a local in a frame that unwinds takes its whole subtree down with it,
/// and a field of an object takes it down with the object. It is not copied -- a copy would
/// be a second owner whose going cancels the first one's work -- and the right is handed on
/// only by moving it: the source moved from is empty and its going does nothing, and one
/// assigned over cancels the work it held, as its destructor would, before it takes the
/// other's.
///
/// The state behind it is shared with its tokens and lives as long as the last of them, so
/// a token read after its source has gone answers that it is cancelled. Asking is the first
/// of two steps, as with event waits: cancelled, the chains end by themselves, and whatever
/// has not ended when its owner can wait no longer is destroyed by the owner, which gives up
/// its operations the usual way.
///
/// Only a source cancels, and a token only reads, so a part of some work that may be
/// cancelled apart from the rest has a source of its own, made under the token of the
/// whole: `cancellation_source child(parent);`. Its cancel() reaches its own tokens alone,
/// and the parent's reaches them too. This is how work is divided: each piece of it -- a
/// page being laid out, a download -- takes its own source under its owner's token, and
/// its operations are registered with that. A token's event is a singly linked list that leaving
/// walks, and this keeps it a handful long: a token's own operations and waits, and the
/// sources made under it.
class cancellation_source
{
public:
    inline cancellation_source() : state_(core::make_refcounted<detail::cancellation_state>()) {}

    /// A source under `parent`, cancelled by its own cancel() or by the parent's, whichever
    /// comes first. Its state is registered with the parent's event, a callback that cancels it,
    /// from now until the state goes, and holds the parent's state meanwhile; under a parent
    /// cancelled already it is born cancelled, and under a token of nobody it is a source
    /// like any other. Made on the coroutines' thread, whose event it joins; a build that
    /// checks coroutines makes sure.
    inline explicit cancellation_source(
        cancellation_token parent, [[maybe_unused]] detail::site where = detail::site::current()) {
        coro_check(sta_thread::is_current(),
                   "cancellation_source: made under a token on a thread other than its coroutines'",
                   where);

        state_ = core::make_refcounted<detail::cancellation_state>(std::move(parent.state_));
    }

    cancellation_source(const cancellation_source&) = delete;
    cancellation_source& operator=(const cancellation_source&) = delete;

    /// Takes the right to the work over, and cancels nothing: `other` is left empty.
    inline cancellation_source(cancellation_source&& other) noexcept
        : state_(std::move(other.state_)) {}

    /// Cancels the work this source held, as its destructor would, and takes over the work
    /// of `other`, which is left empty. Through a local, so that a source moved onto itself
    /// stays as it is; the work this one held is cancelled as the local goes, after this
    /// source holds its new state, so that nothing the cancelling lets go of finds it half
    /// assigned.
    inline cancellation_source& operator=(cancellation_source&& other) noexcept {
        cancellation_source taken(std::move(other));
        std::swap(state_, taken.state_);
        return *this;
    }

    /// Cancels, as cancel() does, unless the source is empty -- moved from. Where a destructor
    /// is called nothing gives the line, so the check names its own.
    inline ~cancellation_source() {
        if (state_) cancel_held(std::move(state_), detail::site::current());
    }

    /// \param where the caller's line, for a strict build's report; left to its default.
    inline cancellation_token token(detail::site where = detail::site::current()) const noexcept {
        check_not_empty(where);
        return cancellation_token(state_.get());
    }

    /// Cancels every operation and wait registered with a token of this source, and answers
    /// every later one at once. Asking again does nothing. On the coroutines' own thread, like
    /// everything about them; a build that checks coroutines makes sure.
    inline void cancel([[maybe_unused]] detail::site where = detail::site::current()) noexcept {
        check_not_empty(where);
        cancel_held(state_, where);
    }

    /// \param where the caller's line, for a strict build's report; left to its default.
    inline bool is_canceled(detail::site where = detail::site::current()) const noexcept {
        check_not_empty(where);
        return state_->canceled();
    }

private:
    /// Held by a local across the cancelling: an operation cancelled may let go of something
    /// that holds this source, and the state then goes once the fire is over.
    inline static void cancel_held(detail::cancellation_state_ptr held,
                                   [[maybe_unused]] detail::site where) noexcept {
        coro_check(sta_thread::is_current(),
                   "cancellation_source: cancelled from a thread other than its coroutines'",
                   where);

        held->cancel();
    }

    inline void check_not_empty([[maybe_unused]] detail::site where) const noexcept {
        coro_check(state_, "cancellation_source: empty -- moved from", where);
    }

    detail::cancellation_state_ptr state_;
};

namespace detail {

// The empty value of core::nullable<cancellation_source> and core::nullable<cancellation_token>:
// both are one intrusive_ptr to the state and nothing else, so the empty one is that pointer null,
// read as the object it is the whole of -- as with func_sentinel in wxl.core. The sentinel moves
// out of that null, which reads the pointer and writes it null again; is_sentinel reads it back
// the same way, without a friendship with the private state_.
//
// A source has that value already: the one it is left in when it is moved from. Its destructor
// does nothing then (`if (state_)`), so an empty optional costs nothing to destroy, and assigning
// over it cancels nothing; there is no constructor for it on purpose -- a source a caller makes is
// a right to work. A token's null state is the same bits as a default token, the one with no
// source, so such a token put in a nullable reads as empty.
template <class T>
struct cancellation_sentinel {
    static T sentinel() noexcept {
        void* empty = nullptr;
        return std::move(*reinterpret_cast<T*>(&empty));
    }

    static bool is_sentinel(T const& value) noexcept {
        return *reinterpret_cast<void* const*>(&value) == nullptr;
    }
};

}  // namespace detail

}  // export namespace wxl::async

export namespace wxl::core {

/// Provides the `wxl::core::nullable` representation of a `wxl::async::cancellation_source`: a
/// source that may not be there yet, or any more, takes one pointer, as the source itself does.
template <>
struct optional_selector<async::cancellation_source> {
    /// The type of an optional source: a `wxl::core::compressed_optional` whose empty value is
    /// the state of a moved-from source.
    using nullable =
        compressed_optional<async::cancellation_source,
                            async::detail::cancellation_sentinel<async::cancellation_source>>;
};

// The same for a token: one pointer. A default token -- no source -- has the null state, the same
// bits as the empty one, so put in a nullable it reads as empty.
template <>
struct optional_selector<async::cancellation_token> {
    using nullable =
        compressed_optional<async::cancellation_token,
                            async::detail::cancellation_sentinel<async::cancellation_token>>;
};

}  // export namespace wxl::core
