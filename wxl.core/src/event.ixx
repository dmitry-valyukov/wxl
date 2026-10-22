module;

#include "abi.h"

export module wxl.core:event;

import :function;
import :intrusive_slist;
import :noncopyable;
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
 *
 * Neither copied nor moved: a cookie names a node of this list, and an invocation under
 * way names this object. Callbacks change hands through swap() alone.
 */
template <template <class> class Base, class R, class... Args>
class basic_event : noncopyable
{
    static_assert(std::is_void_v<R>,
                  "an event calls every one of its callbacks, so there is no single result "
                  "for a call to return -- declare the signature with a void result");

public:
    /// The callback object of this event: what add() makes out of what it is given, and
    /// what the other add() takes ready-made. Spelled `noexcept`, and the spelling is the
    /// contract: a callback fired out of a list has nobody to throw to, so the event only
    /// holds functions whose operator() promises not to. This holds however the signature
    /// of the event itself was spelled -- the `noexcept` on the callback is not optional.
    using func_t = impl::func_body<R(Args...) noexcept, Base>;

    /// An event may end from inside one of its own callbacks -- a click handler taking down
    /// the element that owns the event is the ordinary case -- and the invocations under
    /// way learn it: the callback being called finishes, nothing after it is called, every
    /// callback is let go of, and the walks leave the event alone on their way out. A
    /// callback's destructor may still subscribe to the event that is ending; what it adds
    /// is let go of as well.
    ~basic_event() {
        do clear();
        while (!callbacks_.empty());

        for (invocation* i = invocation_; i; i = i->outer) i->owner_ = nullptr;
    }

    /// Calls every callback, in the order they were added.
    ///
    /// The arguments are passed on as they are and never forwarded: all the callbacks get
    /// the same argument, so a value moved into the first of them would reach the second
    /// one empty.
    ///
    /// The list may change while it is being walked, and the walk keeps to two rules. A
    /// callback removed before its turn -- by itself, by another, by clear() -- is not
    /// called, however far the walk has got; one added is not called by an invocation
    /// already under way, only by one that starts afterwards, nested or not. The callback
    /// that removes itself finishes its
    /// call first: the walk lets go of it once it returns. A callback may invoke the event
    /// again; that inner invocation calls what the list holds when it starts.
    ///
    /// Noexcept, because a callback is noexcept: with no result to report and no way to
    /// tell one subscriber's failure to the next, an event that let an exception out would
    /// leave the rest of the list uncalled and hand the raiser to whoever happened to be
    /// invoking.
    void operator()(Args... args) noexcept {
        if (callbacks_.empty()) return;

        invocation frame{*this};
        while (frame.next && frame.next != frame.end) {
            func_t* const fn = frame.next;
            frame.current = fn;
            frame.next = fn->next_;
            (*fn)(args...);
            // Named only while its call runs: the release() below may reach back into the
            // event, and a removal made there is not deferred to a walk that has passed it.
            frame.current = nullptr;
            if (std::exchange(frame.current_removed, false)) release_now(fn);
        }
    }

    /// Adds a callback to the end of the list.
    /// \return the cookie naming it, for remove().
    template <invocable<R(Args...) noexcept> F>
    cookie_t add(F&& fn) {
        return add(func_t::create(std::forward<F>(fn)));
    }

    /// Adds a callback that was made elsewhere: by a subscribe() of someone who wraps
    /// this event and wants the callback built at its own caller's side, so that what the
    /// caller wrote goes into the node directly instead of through a std::function on the
    /// way -- or a node built into the subscriber itself, which is then not allocated at
    /// all. What happens to it once it is out of the list is its own release(): one made
    /// by func_t::create() is destroyed, one built into its subscriber is left to it.
    ///
    /// The node must not stand in a list already, this event's or another's, and one that
    /// took itself out from inside its own call stands again only once that call has
    /// returned and the event has let go of it. The cookie names the node by the address of
    /// its `func_t` subobject, and that is what remove() or unlink() has to be handed back.
    cookie_t add(not_null<func_t> fn) noexcept {
        for (invocation* i = invocation_; i; i = i->outer) {
            // Removed from inside its call, which is still running: the walk lets go of it,
            // and clears its link, only when the call returns.
            assert(!(i->current == fn.get() && i->current_removed) &&
                   "wxl: a callback removed from inside its call stands again before it returned");
            if (!i->end) i->end = fn.get();
        }
        return callbacks_.push_back(fn);
    }

    /// Removes the callback named by `cookie` and lets go of it -- at once, or, when it is
    /// the one being called, as soon as it returns. Either way it is called no more.
    /// \return false if this event has no such callback -- it was never added here, it
    ///         went with the callbacks swap() handed to another, or it is gone already.
    ///         Gone from the list, that is, not necessarily let go of: during clear() a
    ///         callback still to be released answers false as well. And a cookie kept past
    ///         its callback is compared by address only: once a new callback of this event
    ///         takes the same address -- the STA pool hands a freed node out first -- the
    ///         cookie names that one. Whoever keeps a cookie longer than its callback rules
    ///         that out, as a cancellation does by taking no subscriber once cancelled.
    bool remove(cookie_t cookie) noexcept {
        func_t* const fn = callbacks_.remove(cookie);
        if (!fn) return false;
        step_past(fn);
        release(fn);
        return true;
    }

    /// Takes out a node that its subscriber owns and is about to destroy -- a member deriving
    /// from `func_t` -- and does nothing else: the node leaves the list, no walk under way
    /// calls it or stops at it, its link is cleared, and its release() is not called. Such a
    /// subscriber does not go away from inside a call that reaches its node, so when it
    /// unlinks no walk of this event stands on the node, and there is nothing to defer; a
    /// build that checks stops if a walk is inside the node's call. remove() stays for the
    /// nodes the event owns: it is what frees them, and a callback removed from inside its
    /// own call has to be freed only once that call returns.
    /// \return false if this event has no such node, as remove() answers; a node that left
    ///         by swap() or clear() is still held there until its release().
    bool unlink(cookie_t cookie) noexcept {
        assert(!in_call(cookie.get()) && "wxl: a node is unlinked while a walk is inside its call");
        func_t* const fn = callbacks_.remove(cookie);
        if (!fn) return false;
        step_past(fn);
        fn->next_ = nullptr;
        return true;
    }

    /// Removes every callback and lets go of each, in the order they were added; one being
    /// called goes when it returns.
    void clear() noexcept {
        // Emptied first and walked afterwards, so that a callback whose destructor reaches
        // back into the event finds it empty rather than halfway through being freed. A
        // remove() from there of one not let go of yet answers false: it is out of the
        // list, and is released a moment later all the same.
        intrusive_slist<func_t> orphans;
        orphans.swap(callbacks_);
        for (invocation* i = invocation_; i; i = i->outer) {
            i->next = nullptr;
            i->end = nullptr;
        }

        for (func_t* fn = orphans.front(); fn;) {
            func_t* const next = fn->next_;
            release(fn);
            fn = next;
        }
    }

    /// Exchanges the callbacks of two events; a cookie follows its callback into the other
    /// one. This is how a notification that happens once is fired: empty the event into a
    /// local one and call that, so that a callback unsubscribing from inside the call finds
    /// nothing left to unsubscribe from. Neither event may be inside an invocation.
    void swap(basic_event& other) noexcept {
        assert(!invocation_ && !other.invocation_ && "wxl: an event is swapped while it is invoked");
        callbacks_.swap(other.callbacks_);
    }

    /// Whether anything is subscribed.
    explicit operator bool() const { return !callbacks_.empty(); }

private:
    // An invocation under way, kept on the stack frame of the operator() that made it; the
    // event knows the innermost, and each knows the one it interrupted. This is what lets
    // the list change while it is walked without a mark on every node: whatever takes a
    // callback out moves every walk past it, and only the callback being called has to
    // wait for its walk.
    struct invocation {
        explicit invocation(basic_event& owner) noexcept
            : next(owner.callbacks_.front()), outer(owner.invocation_), owner_(&owner) {
            owner.invocation_ = this;
        }

        // The owner is null when the event was destroyed from inside this invocation: it
        // has nothing to give the outer one back to, and does not reach for it.
        ~invocation() {
            if (owner_) owner_->invocation_ = outer;
        }

        // The callback being called; null between calls, so that release() meets it only
        // while its call runs.
        func_t* current = nullptr;
        // What the walk calls next; null when nothing is left.
        func_t* next;
        // The first callback added while this walk was under way, where it stops: whatever
        // is added goes to the tail, so everything from here on is new to it.
        func_t* end = nullptr;
        // The invocation this one interrupted, which goes on when this one returns.
        invocation* const outer;
        // `current` left the list during its call, and this walk -- the outermost one in
        // that call -- lets go of it once the call returns.
        bool current_removed = false;

        // The event walked, until it is destroyed from inside the walk.
        basic_event* owner_;
    };

    // `fn` has just left the list, its link still what it was: a walk that would call it
    // next calls what follows instead, and a walk that would stop at it stops where it
    // would have gone on to.
    void step_past(func_t const* fn) noexcept {
        for (invocation* i = invocation_; i; i = i->outer) {
            if (i->next == fn) i->next = fn->next_;
            if (i->end == fn) i->end = fn->next_;
        }
    }

    // Whether a walk is inside the call of the node at `node`.
    bool in_call(void const* node) const noexcept {
        for (invocation const* i = invocation_; i; i = i->outer)
            if (i->current == node) return true;
        return false;
    }

    // Lets go of a callback that has left the list, unless a walk is inside its call: then
    // the outermost such walk lets go of it on return, so the code of the call never runs
    // on freed memory.
    void release(func_t* fn) noexcept {
        invocation* caller = nullptr;
        for (invocation* i = invocation_; i; i = i->outer) {
            // No break: the chain runs inwards out, and a callback that invoked the event and
            // was reached again by that inner walk is on two frames -- the outer one returns
            // from it last.
            if (i->current == fn) caller = i;
        }
        if (caller) {
            caller->current_removed = true;
        } else {
            release_now(fn);
        }
    }

    // The link is cleared first: the list leaves it as it was when it unlinks, and a node
    // that outlives the list -- built into its subscriber -- may be added again.
    static void release_now(func_t* fn) noexcept {
        fn->next_ = nullptr;
        fn->release();
    }

    intrusive_slist<func_t> callbacks_;
    invocation* invocation_ = nullptr;
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
 * result for a call to hand back, and picking one of them would be a rule that cannot be
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
 * by `remove`, by `clear`, or with the event itself. In all three cases the event lets go
 * of the node through its `release()`: one the event made out of what `add` was given --
 * or one made by `func_t::create()` and handed over -- belongs to the event and is
 * destroyed; a node built into its subscriber (a member deriving from `func_t`, whose
 * `release()` does nothing) belongs to the subscriber. Such a subscriber allocates
 * nothing to subscribe, and in return keeps three rules. It stays alive until its
 * `release()` comes or `unlink` takes the node out, not only while `unlink` would still
 * find it: `clear` and the event's end let go of what they emptied one by one, and an
 * `unlink` or `remove` answered false in the meantime is not a `release()`. It does not go
 * away from inside a call that reaches it. And a node that took itself out from inside its
 * own call stands again only once that call has returned. Going away, it leaves by `unlink`
 * rather than `remove`: nothing is let go of, and the node unlinked is its own at once.
 *
 * An `event` belongs to the STA thread: its nodes come from `sta_memory_pool`, so they are
 * added, fired and removed on that thread and inside the pool's life, the way a `function`'s
 * body is. A list that another thread swaps out and fires -- a component's stop callbacks --
 * is an `event_mt`, whose nodes stay on the ordinary heap. The two are otherwise the same,
 * and neither synchronises anything: with `event_mt`, adding, removing, swapping and the
 * call itself all go under one lock of the holder's, the way `component` keeps them.
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
