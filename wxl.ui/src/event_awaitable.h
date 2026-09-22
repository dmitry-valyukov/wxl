#pragma once

// Waiting for an event rather than being called back by it.
//
// A handler is the right shape for an answer that is over in one line, and
// the wrong one as soon as the answer has a middle: a drag is pressed, then
// moved, then released, and written as three handlers the state between them
// has to live in a flag somewhere outside all three. Awaited instead, that
// state is the coroutine's own program counter.
//
// What is awaited is a proxy for the event, not a member of the element:
//
//     auto keys = wxl::on_event<EventKey::PreviewKeyDown>(keypad);
//     while (true) {
//         KeyRoutedEventArgs& args = co_await keys;
//         ...
//     }
//
// Made separately rather than asked of the element for three reasons. The
// generator emits nothing for it -- EventAdder is the vocabulary already
// written for the builder syntax, so every event in the profile, and every
// one added to it later, is awaitable the moment it exists. The subscription
// becomes an object with a place, so how long it lasts is written down rather
// than implied. And one name covers both shapes: kept in a variable it is a
// subscription a loop waits on, written as a temporary it is a single wait --
//
//     co_await wxl::on_event<EventKey::Loaded>(page);
//
// -- because the temporary lives to the end of the full expression, which is
// past the suspension inside it.
//
// This is emphatically not async::awaitable, which owns one pending operation,
// is consumed by the co_await that takes its result, and is resumed by a
// worker thread through the loop. This one holds a subscription across any
// number of waits and is resumed by the handler itself, inline, before that
// handler has returned -- which is not a detail of how it is built but the
// point of it: args.handled(true) means something only while the framework is
// still inside the call.
//
// **A wait can end without the event happening**: the element has left the
// tree -- its window closed, a page was navigated away from -- and every
// wait on it ends there, while the message loop is still turning; or the
// application is going down, and impl/event_waits.h says so to everyone
// still suspended. An element can come back (Loaded), so a coroutine that
// is to live with the element is started from Loaded and ends with
// Unloaded, over and over. There are two ways to hear the end, and the
// difference is written at the point of waiting:
//
//     KeyRoutedEventArgs& args = co_await keys;        // throws
//     auto const got = co_await keys.next();           // answers
//
// The throwing form is the default because it is what cancellation already
// does everywhere else in wxl -- an outcome travelling the same way as a value
// and an error. The answering form is for a body that must let nothing
// escape, and it loses nothing by not throwing: what it hands back carries the
// exception when there was one, so it can be read, rethrown or ignored.

#include "core.h"

#include "events.h"
#include "impl/event_waits.h"
#include "impl/member.h"

namespace wxl {

class FrameworkElement;

namespace impl {

// A source naming an element of the tree -- one that can be unloaded. A
// wait on anything else (a type's own event, a Window) ends only with the
// application.
template <typename Source>
concept unloadable_source = requires(Source const& source) {
    requires std::derived_from<std::remove_cvref_t<decltype(source.object)>, FrameworkElement>;
};

}  // namespace impl

// The subscription, and the thing a coroutine awaits.
//
// It is neither copied nor moved, and cannot be: the handler it gives the
// element captures it by address. Made by on_event() below, which returns it
// as a prvalue -- so a variable can still be initialised from the call.
template <typename Source>
class event_awaitable : public impl::waiting_event
{
public:
    // Whatever the source says it hands over: read off the signature of the
    // add it subscribes through, so the pair (class, event) picks the type
    // rather than the event alone.
    using args_t = typename Source::args_t;

    // As a handler is given them: a non-copyable view by reference for
    // event args proper, the wrapper by const reference for the events whose
    // "args" are an ordinary object.
    using args_ref_t = EventArgsRef<args_t>;

    // What the answering form hands back. A reference cannot be the value of
    // an expected, so it travels wrapped; the error side is a nullable
    // exception_ptr, whose empty state is a null pointer and therefore free --
    // empty means "cancelled, and that is all there is to say", a value means
    // "this went wrong, and here it is".
    using args_value_t = std::reference_wrapper<std::remove_reference_t<args_ref_t>>;
    using next_result_t = std::expected<args_value_t, core::nullable<std::exception_ptr>>;

    explicit event_awaitable(Source source) : source_{std::move(source)} {
        // The element is held by value, which is a reference count: the
        // subscription outlives every temporary in the description tree, and
        // the unsubscription below has to have something to unsubscribe from.
        token_ = source_.add(EventHandler<args_t>{[this](Object const&, args_ref_t args) {
                if (!waiting()) {
                    // Nobody is waiting, so this event is not ours: it is left
                    // unhandled and travels on. Deliberately not queued --
                    // args that carry an answer back to the framework
                    // (Handled, a cancellable request refused) are worthless
                    // once the handler has returned, so a queue would hand the
                    // coroutine something it can no longer answer.
                    return;
                }

                // Valid for exactly as long as the frame below runs inside
                // this call, which is what the args type says about itself by
                // being non-copyable. It is left pointing at args afterwards
                // and that is harmless: the only reader is await_resume, and
                // every delivery writes it again first.
                args_ = &args;

                // Resumes, and touches nothing of ours afterwards -- see
                // waiting_event::deliver.
                deliver();
        }});

        // The element leaving the tree ends the wait, and does so while the
        // message loop still turns, so the coroutine may answer with a
        // co_await of its own. IsLoaded is asked rather than assumed: Unloaded
        // says the element left the tree, not that it is still out of it,
        // and one moved between parents may be back by the time this runs.
        // Nothing of ours is touched after end(): it may have run the
        // coroutine to its end, and this object with it.
        if constexpr (impl::unloadable_source<Source>) {
            unloaded_ = impl::EventAdder<EventKey::Unloaded>::add(source_.object, [this] {
                if (!source_.object.isLoaded()) end();
            });
        }
    }

    // Unsubscribing can be refused, and a destructor may not pass that on.
    // The window closes while a coroutine is still waiting; the framework
    // goes down around the element, and taking a handler off one it has
    // already let go of comes back as an HRESULT. There is nothing to do
    // about it and nobody to tell -- the subscription is going away either
    // way -- and letting it out would end the process, since this runs while
    // the cancellation that stopped the coroutine is still in flight.
    ~event_awaitable() {
        try {
            if constexpr (impl::unloadable_source<Source>) {
                impl::EventAdder<EventKey::Unloaded>::remove(source_.object, unloaded_);
            }

            source_.remove(token_);
        } catch (...) {
        }
    }

    event_awaitable(event_awaitable const&) = delete;
    event_awaitable& operator=(event_awaitable const&) = delete;

    // One wait. It lives in the frame of the coroutine that wrote the
    // co_await, and taking itself off the proxy is what its destructor is
    // for: a frame destroyed while suspended unhooks on the way out, leaving
    // no handle behind to be resumed into freed memory.
    //
    // `Throwing` picks how the wait reports that it ended instead. It is the
    // only difference between the two forms, and it is chosen at the point of
    // waiting rather than by the kind of coroutine, so that one written line
    // means one thing wherever it is read.
    template <bool Throwing>
    class awaiter_t
    {
    public:
        using result_t = std::conditional_t<Throwing, args_ref_t, next_result_t>;

        explicit awaiter_t(event_awaitable& event) noexcept : event_{&event} {}

        ~awaiter_t() {
            if (armed_) event_->disarm();
        }

        awaiter_t(awaiter_t const&) = delete;
        awaiter_t& operator=(awaiter_t const&) = delete;

        // Nothing suspends once the application has begun going down: the
        // wait ends where it stands rather than joining a list that is being
        // emptied. Which is also what keeps a coroutine that catches
        // cancellation and waits again from being ended over and over.
        bool await_ready() const noexcept { return impl::events_closing(); }

        void await_suspend(std::coroutine_handle<> waiter) noexcept {
            event_->arm(waiter);
            armed_ = true;
        }

        result_t await_resume() {
            armed_ = false;

            if constexpr (Throwing) {
                if (over()) event_->throw_ended();

                return static_cast<args_ref_t>(*event_->args_);
            } else {
                if (over()) return result_t{std::unexpect, event_->error()};

                return result_t{args_value_t{*event_->args_}};
            }
        }

    private:
        // Ended while suspended, or never suspended because it was already
        // too late.
        bool over() const noexcept { return event_->ended() || impl::events_closing(); }

        event_awaitable* event_;
        bool armed_ = false;
    };

    using awaiter = awaiter_t<true>;

    awaiter operator co_await() noexcept { return awaiter{*this}; }

    // Waiting writes down who is waiting, so a const proxy cannot be awaited
    // -- said here rather than left to overload resolution, which would
    // otherwise report the absence of await_resume and name nothing that a
    // reader could act on.
    awaiter operator co_await() const = delete;

    /// The same wait, reported rather than thrown:
    ///
    ///     while (auto const got = co_await keys.next()) {
    ///         got->get().handled(true);
    ///     }
    ///
    /// For a body that must let nothing escape. What comes back on the error
    /// side is the exception if there was one, so nothing is lost by not
    /// throwing -- an empty one is a plain cancellation.
    awaiter_t<false> next() noexcept { return awaiter_t<false>{*this}; }

private:
    Source source_;
    EventToken token_;

    // The Unloaded subscription of an element source; unused for the rest.
    EventToken unloaded_;

    // What the waiter is about to be given: set only inside the call that
    // hands the args over.
    std::remove_reference_t<args_ref_t>* args_ = nullptr;
};

namespace impl {

// Where a wait gets its event from. Three of them, and they differ only in
// how the pair of calls is reached -- which is why the wait itself knows
// nothing about any of it.

// Named by its key, on an object: `on_event<EventKey::Click>(button)`.
template <EventKey key, typename Obj>
struct keyed_event {
    using args_t = typename EventAdder<key>::template args_t<Obj>;

    Obj object;

    EventToken add(EventHandler<args_t> const& handler) const {
        return EventAdder<key>::add(object, handler);
    }

    void remove(EventToken token) const { EventAdder<key>::remove(object, token); }
};

// Named by its own members, on an object:
// `on_event(button, &Button::add_onClick, &Button::remove_onClick)`.
template <typename Obj, typename Add, typename Remove>
struct member_event {
    using args_t = event_args_of_t<Add>;

    Obj object;
    Add adder;
    Remove remover;

    EventToken add(EventHandler<args_t> const& handler) const {
        return (object.*adder)(handler);
    }

    void remove(EventToken token) const { (object.*remover)(token); }
};

// Named by its own members, on the type -- an event with no object to
// subscribe on:
// `on_event(&CompositionTarget::add_onRendering, &CompositionTarget::remove_onRendering)`.
template <typename Add, typename Remove>
struct type_event {
    using args_t = event_args_of_t<Add>;

    Add adder;
    Remove remover;

    EventToken add(EventHandler<args_t> const& handler) const { return adder(handler); }

    void remove(EventToken token) const { remover(token); }
};

}  // namespace impl

/// A wait on an event of this object, named as the builder syntax names it:
/// `on_event<EventKey::Click>(button)` beside `onClick = handler`.
template <EventKey key, typename Obj>
event_awaitable<impl::keyed_event<key, Obj>> on_event(Obj const& source) {
    return event_awaitable<impl::keyed_event<key, Obj>>{{source}};
}

/// A wait on an event this object has but the generated vocabulary does not
/// name, given by its own two members.
template <typename Obj, typename Add, typename Remove>
event_awaitable<impl::member_event<Obj, Add, Remove>> on_event(Obj const& source, Add add,
                                                              Remove remove) {
    return event_awaitable<impl::member_event<Obj, Add, Remove>>{{source, add, remove}};
}

/// And a wait on an event of the type itself, which has no object to be
/// asked of: only the two members, and they are static.
template <typename Add, typename Remove>
event_awaitable<impl::type_event<Add, Remove>> on_event(Add add, Remove remove) {
    return event_awaitable<impl::type_event<Add, Remove>>{{add, remove}};
}

/// The keyed wait through its tag: `onClick(button)` is `on_event<EventKey::Click>(button)`.
/// A schema tag names the class too, and that is checked here like every other
/// use of one.
template <EventKey key, typename Owner>
template <typename Obj>
    requires std::derived_from<Obj, Object>
auto Event<key, Owner>::operator()(Obj const& source) const {
    impl::check_owner<Owner, Obj>();

    return on_event<key>(source);
}

}  // namespace wxl
