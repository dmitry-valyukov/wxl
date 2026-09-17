export module wxl.core:event;

import :function;
import :intrusive_slist;
import :not_null;
import std;

namespace wxl::core {

/**
 * The machinery of an `event`, keyed on the signature already taken apart into result and
 * arguments. It is not named directly: the `event<...>` a caller writes derives from it. A
 * signature can be written with or without `noexcept` -- `event<void(int)>` and
 * `event<void(int) noexcept>` are two spellings that land on the same `basic_event` -- and
 * this is where the one implementation of both lives. `Base` is the node the callback is
 * linked into the list by, and with it where the node is allocated: the STA pool for an
 * `event`, the ordinary heap for an `event_mt`.
 */
template <template <class> class Base, class R, class... Args>
class basic_event
{
    static_assert(std::is_void_v<R>,
                  "an event calls every one of its callbacks, so there is no single result "
                  "for fire() to return -- declare the signature with a void result");

public:
    /// The callback object of this event: what add() makes out of what it is given, and
    /// what the other add() takes ready-made. Spelled `noexcept`, and the spelling is the
    /// contract: a callback fired out of a list has nobody to throw to, so the event only
    /// holds functions whose operator() promises not to. This holds however the signature
    /// of the event itself was spelled -- the `noexcept` on the callback is not optional.
    using func_t = impl::func_body<R(Args...) noexcept, Base>;

    ~basic_event() { clear(); }

    /// Calls every callback, in the order they were added.
    ///
    /// The arguments are passed on as they are and never forwarded: all the callbacks get
    /// the same argument, so a value moved into the first of them would reach the second
    /// one empty.
    ///
    /// Noexcept, because a callback is noexcept: with no result to report and no way to
    /// tell one subscriber's failure to the next, an event that let an exception out would
    /// leave the rest of the list uncalled and hand the raiser to whoever happened to be
    /// firing.
    void fire(Args... args) noexcept {
        for (auto & fn : callbacks_) fn(args...);
    }

    /// Adds a callback to the end of the list.
    /// \return the cookie naming it, for remove().
    template <invocable<R(Args...) noexcept> F>
    cookie_t add(F&& fn) {
        return add(func_t::create(std::forward<F>(fn)));
    }

    /// Adds a callback that was made elsewhere -- by a subscribe() of someone who wraps
    /// this event and wants the callback built at its own caller's side, so that what the
    /// caller wrote goes into the node directly instead of through a std::function on the
    /// way. The event takes it over, as it does any other.
    cookie_t add(not_null<func_t> fn) { return callbacks_.push_back(fn); }

    /// Removes and destroys the callback named by `cookie`.
    /// \return false if this event has no such callback -- it was never added here, or it
    ///         is gone already.
    bool remove(cookie_t cookie) {
        func_t* fn = callbacks_.remove(cookie);
        delete fn;
        return fn != nullptr;
    }

    /// Removes and destroys every callback.
    void clear() {
        // Emptied first and walked afterwards, so that a callback whose destructor reaches
        // back into the event finds it empty rather than halfway through being freed.
        intrusive_slist<func_t> orphans;
        orphans.swap(callbacks_);

        auto it = orphans.begin();
        while (it != orphans.end()) {
            func_t * fn = &*it;
            ++it;
            delete fn;
        }
    }

    /// Exchanges the callbacks of two events; a cookie follows its callback into the other
    /// one. This is how a notification that happens once is fired: empty the event into a
    /// local one and fire that, so that a callback unsubscribing from inside the call finds
    /// nothing left to unsubscribe from, instead of the list being walked underneath it.
    void swap(basic_event & other) noexcept { callbacks_.swap(other.callbacks_); }

    explicit operator bool() const { return !callbacks_.empty(); }

protected:
    intrusive_slist<func_t> callbacks_;
};

}  // namespace wxl::core

export namespace wxl::core {

/**
 * A list of callbacks of one signature, called in the order they were added.
 *
 * The signature is written out in full -- `event<void()>`, `event<void(int)>`,
 * `event<void(widget&, std::string)>` -- so the event imposes nothing on the shape of
 * what it calls: a sender argument is there when the signature asks for one, and not
 * otherwise. The result has to be `void`. Every callback is called, so there is no single
 * result for `fire` to hand back, and picking one of them would be a rule that cannot be
 * read off the declaration.
 *
 * The signature may carry `noexcept` -- `event<void(int) noexcept>` -- and this changes
 * nothing but the declaration: every callback is noexcept in either case, because a callback
 * fired out of a list has nobody to throw to. The `noexcept` spelling is offered so that a
 * type built on an event can make that contract visible where it matters, as
 * `observable`'s change event does; the two spellings name the same event.
 *
 * Each callback lives in a node of its own, linked into an `intrusive_slist`, and `add`
 * returns the `cookie_t` naming that node. The cookie is the only way back to that one
 * callback -- `remove` is what it is for -- and it stays valid until that callback goes,
 * by `remove`, by `clear`, or with the event itself. The callbacks belong to the event: it
 * destroys them in all three cases.
 *
 * An `event` belongs to the STA thread: its nodes come from `sta_memory_pool`, so they are
 * added, fired and removed on that thread and inside the pool's life, the way a `function`'s
 * body is. A list that another thread swaps out and fires -- a component's stop callbacks --
 * is an `event_mt`, whose nodes stay on the ordinary heap. The two are otherwise the same.
 */
template <class Signature>
class event;

template <class R, class... Args>
class event<R(Args...)> : public basic_event<sta_intrusive_slist_node, R, Args...> {};

template <class R, class... Args>
class event<R(Args...) noexcept> : public basic_event<sta_intrusive_slist_node, R, Args...> {};

template <class Signature>
class event_mt;

template <class R, class... Args>
class event_mt<R(Args...)> : public basic_event<intrusive_slist_node, R, Args...> {};

template <class R, class... Args>
class event_mt<R(Args...) noexcept> : public basic_event<intrusive_slist_node, R, Args...> {};

}  // export namespace wxl::core
