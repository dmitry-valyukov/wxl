#pragma once

// The wait behind `co_await on_event(event, stop)` on a core::event -- an event of the
// application's own, with no sender and no element to leave the tree with
// (event_awaitable.h).
//
// The wait subscribes when it suspends and leaves when it resumes, so an invocation
// that comes while the coroutine is not suspended here is not its: as with an event of
// an element, arguments are not queued. The invocation resumes the coroutine inside the
// call, and the subscription is taken out by that call before it resumes: whatever the
// coroutine then does to the event, the wait no longer reaches for it. The arguments
// come as values -- one the event passes by value is moved, one by reference to const
// copied -- so nothing the coroutine keeps outlives the call; only a reference to
// non-const stays one, for the coroutine to answer through inside the call. The rest is
// every wait's (event_waits.h): a cancelled token resumes nobody, the queue's next turn
// does, or an invocation that comes first; going down ends the wait.
//
// The callback is a node of its own from the STA pool, not one built into the wait:
// the coroutine resumed inside the call may run on to the end of its co_await or of
// its body and take the wait with it, and the event still lets go of the callback once
// the call returns. The callback points back at the wait until the wait leaves. One the
// event lets go of while the wait stands -- cleared, or ended -- ends the wait as one
// whose event will not come, on the queue's next turn rather than inside the clear() or
// the destructor that did it.

#include <cassert>
#include <coroutine>
#include <tuple>
#include <type_traits>
#include <utility>

#include "event_waits.h"

namespace wxl::impl {

/// Whether the wait can hand an argument over: a reference to const it hands over as a
/// copy, which a type that cannot be copied does not survive, and a polymorphic one would
/// survive cut down to its static type, without a word.
template <typename Arg>
concept invocation_answerable =
    !(std::is_lvalue_reference_v<Arg> && std::is_const_v<std::remove_reference_t<Arg>>) ||
    (std::is_copy_constructible_v<std::remove_cvref_t<Arg>> && !std::is_polymorphic_v<std::remove_cvref_t<Arg>>);

/// How the wait hands over one argument: a reference to non-const as it is, the channel
/// the coroutine answers through; anything else as a value of its own.
template <typename Arg>
struct invocation_answer {
    static_assert(invocation_answerable<Arg>,
                  "wxl: on_event answers an argument passed as T const& with a copy of T, and this T "
                  "cannot be copied or would be sliced -- declare the event with T& to get the "
                  "raiser's object itself, or with T to pass a value");

    using type =
        std::conditional_t<std::is_lvalue_reference_v<Arg> && !std::is_const_v<std::remove_reference_t<Arg>>, Arg,
                           std::remove_cvref_t<Arg>>;
};

template <typename Arg>
using invocation_answer_t = typename invocation_answer<Arg>::type;

/// What a wait on an event of these arguments gives: none -- nothing, one -- that one,
/// several -- a tuple of them.
template <typename... Args>
struct invocation_result {
    using type = std::tuple<invocation_answer_t<Args>...>;
};

template <>
struct invocation_result<> {
    using type = void;
};

template <typename Arg>
struct invocation_result<Arg> {
    using type = invocation_answer_t<Arg>;
};

/// One wait on the next invocation of `Event`, a core::event of `Args`. The event
/// outlives the wait, unless it lets go of the callback first -- cleared or ended under
/// the wait, it ends it.
template <typename Event, typename... Args>
class core_event_wait : waiting_event
{
public:
    using result_t = typename invocation_result<Args...>::type;

    explicit core_event_wait(Event& event) noexcept : event_(&event) {}

    ~core_event_wait() {
        if (waiting()) disarm();
        leave();
    }

    core_event_wait(core_event_wait const&) = delete;
    core_event_wait& operator=(core_event_wait const&) = delete;

    /// Cancelled before it began, or the application going down: the wait does not
    /// suspend, and subscribes to nothing.
    bool await_ready() const noexcept { return canceled_ || events_closing(); }

    // Declared twice, as every hook that checks how it is used is: a strict build's
    // takes the place of the co_await, any other build's has the signature it always had.

    void await_suspend(std::coroutine_handle<> waiter) requires(!async::detail::strict) {
        subscribe();
        arm(waiter);
    }

    void await_suspend(std::coroutine_handle<> waiter,
                       async::detail::site where = async::detail::site::current())
        requires(async::detail::strict)
    {
        subscribe();
        arm(waiter, where);
    }

    /// The arguments of the invocation that resumed the wait. Ended instead -- going
    /// down, or the event let go of the callback -- it throws as every wait of wxl does.
    /// A cancelled token never gets here: the wait under it answers that itself
    /// (async::detail::wait_under).
    result_t await_resume() {
        leave();

        if (ended() || events_closing()) throw_ended();

        if constexpr (sizeof...(Args) == 1) {
            return answer<std::tuple_element_t<0, std::tuple<Args...>>>(std::get<0>(*args_));
        } else if constexpr (sizeof...(Args) > 1) {
            return std::apply([](std::remove_reference_t<Args>&... args) { return result_t{answer<Args>(args)...}; },
                              *args_);
        }
    }

    /// Cancelled by the token (async::detail::cancellable_awaiter): a wait that is
    /// suspended is resumed by the queue's next turn, or by an invocation if one comes
    /// first -- never by cancel() itself; one that has not begun never suspends.
    void cancel() noexcept {
        if (waiting()) {
            waiting_event::cancel();
        } else {
            canceled_ = true;
        }
    }

private:
    /// The subscription: the event calls it and lets go of it, the wait is reached
    /// through it while the wait stands.
    class callback final : public Event::func_t
    {
    public:
        explicit callback(core_event_wait& wait) noexcept : wait_(&wait) {}

        void operator()(Args... args) noexcept override {
            if (wait_) wait_->take(args...);
        }

        void release() noexcept override {
            if (wait_) wait_->dropped();
            delete this;
        }

        core_event_wait* wait_;
    };

    /// One argument as invocation_answer_t says: one passed by value is the callback's own
    /// copy, and is moved out of it.
    template <typename Arg>
    static invocation_answer_t<Arg> answer(std::remove_reference_t<Arg>& arg) {
        if constexpr (std::is_reference_v<Arg>) {
            return arg;
        } else {
            return std::move(arg);
        }
    }

    void subscribe() {
        assert(!callback_ && "wxl: a wait on an event subscribed twice");

        auto* const made = new callback{*this};
        event_->add(core::as_not_null<typename Event::func_t>(made));
        callback_ = made;
    }

    /// remove() answers false when swap() has handed the callback to another event: that
    /// one lets go of it in its time, and the callback, no longer pointing at the wait,
    /// does nothing meanwhile.
    void leave() noexcept {
        if (callback* const subscribed = std::exchange(callback_, nullptr)) {
            subscribed->wait_ = nullptr;
            event_->remove(core::cookie_t{static_cast<typename Event::func_t*>(subscribed)});
        }
    }

    /// An invocation that finds the wait resumed already, its co_await not over yet,
    /// is not its. The one that is takes the subscription out first: the coroutine it
    /// resumes may end the event, and the wait, gone or not, then has nothing to leave.
    void take(Args&... args) noexcept {
        if (!waiting()) return;

        leave();

        std::tuple<Args&...> taken{args...};
        args_ = &taken;

        // Last statement: the coroutine may run to its end inside this call, and this
        // object with it.
        deliver();
    }

    void dropped() noexcept {
        callback_ = nullptr;
        if (waiting()) waiting_event::cancel();
    }

    Event* event_;
    callback* callback_ = nullptr;

    // The arguments of the invocation under way: set only inside the call that hands
    // them over, read only by the await_resume it resumes into.
    std::tuple<Args&...>* args_ = nullptr;

    // Cancelled before it suspended, and so never to.
    bool canceled_ = false;
};

}  // namespace wxl::impl
