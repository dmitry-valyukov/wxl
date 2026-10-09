module;

#include "abi.h"

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

    /// An event is not destroyed from inside one of its own callbacks: the fire under way
    /// would come back to a list that is gone.
    ~basic_event() {
        assert(!firing_ && "wxl: an event is destroyed from inside its own fire()");
        clear();
    }

    /// Calls every callback, in the order they were added.
    ///
    /// The arguments are passed on as they are and never forwarded: all the callbacks get
    /// the same argument, so a value moved into the first of them would reach the second
    /// one empty.
    ///
    /// The list may change while it is being walked, and the walk keeps to two rules. A
    /// callback removed before its turn -- by itself, by another, by clear() -- is not
    /// called, however far the walk has got; one added is not called by a fire already
    /// under way, only by the next. The callback that removes itself finishes its call
    /// first: the walk destroys it once it returns. A callback may fire the event again;
    /// that inner fire calls what the list holds when it starts.
    ///
    /// Noexcept, because a callback is noexcept: with no result to report and no way to
    /// tell one subscriber's failure to the next, an event that let an exception out would
    /// leave the rest of the list uncalled and hand the raiser to whoever happened to be
    /// firing.
    void fire(Args... args) noexcept {
        if (callbacks_.empty()) return;

        firing frame{.next = callbacks_.front(), .outer = firing_};
        firing_ = &frame;
        while (frame.next && frame.next != frame.end) {
            func_t* const fn = frame.next;
            frame.current = fn;
            frame.next = fn->next_;
            (*fn)(args...);
            if (frame.doomed) {
                frame.doomed = false;
                delete fn;
            }
        }
        firing_ = frame.outer;
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
    cookie_t add(not_null<func_t> fn) {
        for (firing* f = firing_; f; f = f->outer) {
            if (!f->end) f->end = fn.get();
        }
        return callbacks_.push_back(fn);
    }

    /// Removes and destroys the callback named by `cookie` -- at once, or, when it is the
    /// one being called, as soon as it returns. Either way it is called no more.
    /// \return false if this event has no such callback -- it was never added here, or it
    ///         is gone already.
    bool remove(cookie_t cookie) {
        // Compared by address only: a node a fire is about to reach is in the list, so
        // a match is one, and only a match is read through.
        if (firing_) step_past(static_cast<func_t const*>(cookie.get()));
        func_t* const fn = callbacks_.remove(cookie);
        if (fn) release(fn);
        return fn != nullptr;
    }

    /// Removes and destroys every callback; one being called goes when it returns.
    void clear() {
        // Emptied first and walked afterwards, so that a callback whose destructor reaches
        // back into the event finds it empty rather than halfway through being freed.
        intrusive_slist<func_t> orphans;
        orphans.swap(callbacks_);
        for (firing* f = firing_; f; f = f->outer) {
            f->next = nullptr;
            f->end = nullptr;
        }

        auto it = orphans.begin();
        while (it != orphans.end()) {
            func_t * fn = &*it;
            ++it;
            release(fn);
        }
    }

    /// Exchanges the callbacks of two events; a cookie follows its callback into the other
    /// one. This is how a notification that happens once is fired: empty the event into a
    /// local one and fire that, so that a callback unsubscribing from inside the call finds
    /// nothing left to unsubscribe from. Neither event may be firing.
    void swap(basic_event & other) noexcept {
        assert(!firing_ && !other.firing_ && "wxl: an event is swapped while it fires");
        callbacks_.swap(other.callbacks_);
    }

    explicit operator bool() const { return !callbacks_.empty(); }

protected:
    intrusive_slist<func_t> callbacks_;

private:
    // A fire under way, kept on its own stack frame; the event knows the innermost, and
    // each knows the one it interrupted. This is what lets the list change while it is
    // walked without a mark on every node: whatever takes a callback out moves every
    // walk past it, and only the callback being called has to wait for its walk.
    struct firing {
        func_t* current = nullptr;
        // What the walk calls next; null when nothing is left.
        func_t* next = nullptr;
        // The first callback added while this walk was under way, where it stops: whatever
        // is added goes to the tail, so everything from here on is new to it.
        func_t* end = nullptr;
        firing* outer = nullptr;
        // `current` left the list during its call, and this walk -- the outermost one in
        // that call -- destroys it once the call returns.
        bool doomed = false;
    };

    // `fn` is about to leave the list: a walk that would call it next calls what follows
    // instead, and a walk that would stop at it stops where it would have gone on to.
    void step_past(func_t const* fn) noexcept {
        for (firing* f = firing_; f; f = f->outer) {
            if (f->next == fn) f->next = fn->next_;
            if (f->end == fn) f->end = fn->next_;
        }
    }

    // Destroys a callback that has left the list, unless a walk is inside its call: then
    // the outermost such walk destroys it on return, so the code of the call never runs
    // on freed memory.
    void release(func_t* fn) noexcept {
        firing* caller = nullptr;
        for (firing* f = firing_; f; f = f->outer) {
            if (f->current == fn) caller = f;
        }
        if (caller) {
            caller->doomed = true;
        } else {
            delete fn;
        }
    }

    firing* firing_ = nullptr;
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
