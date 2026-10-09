module;

#include "coroutine_checks.h"

export module wxl.async:detached_task;

import :cancellation;
import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

/// What a coroutine nobody holds does with an exception nobody can catch.
///
/// It is a bare function pointer rather than a `core::function` on purpose:
/// this is set once by an application, if at all, and a handler that had to
/// be allocated would be allocated from a pool that may already be going
/// away by the time it is needed.
using detached_task_failure_handler = void (*)(std::exception_ptr) noexcept;

/// The handler, settable by the application. The default ends the process,
/// which is the same answer cppwinrt gives and the only honest one by
/// default: the exception has escaped a coroutine that nobody is waiting on,
/// so there is nowhere left to report it and nobody to decide what the
/// program should do instead.
inline detached_task_failure_handler& on_detached_task_failure() noexcept {
    static detached_task_failure_handler handler = [](std::exception_ptr error) noexcept {
        // Named before the process goes, because the alternative is what this
        // replaced: a bare "abort() has been called" that says nothing about
        // which coroutine failed or why. Ending is still the only honest
        // answer -- nobody is waiting on this one, so there is nobody to
        // decide anything else -- but ending silently is not.
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& what) {
            std::cerr << "wxl: a detached task failed with nobody to tell: " << what.what() << '\n';
        } catch (...) {
            std::cerr << "wxl: a detached task failed with a foreign exception and nobody to tell\n";
        }

        std::cerr.flush();
        std::terminate();
    };
    return handler;
}

class detached_task;
class scenario_owner;

namespace detached_task_detail {

class scenarios_end;

/// One reference to an object that counts its own, with either counter, in
/// one word -- and, for a scenario, the scenario's place in its owner's count.
///
/// The two counters share no base, but letting go needs only the counting
/// base, not the derived type -- the destructor is virtual in both. So the
/// word is a pointer to that base, and its two lowest bits say what it is:
/// bit 0 a `refcounted_mt`, bit 1 a `scenario_owner` this frame is still
/// counted by. Both are always clear in the pointer itself, since an object
/// with a vtable is aligned at least to a pointer.
class anchor
{
public:
    anchor() noexcept = default;

    inline explicit anchor(const core::refcounted* object) noexcept
        : word_(reinterpret_cast<std::uintptr_t>(object)) {
        if (object) intrusive_ptr_add_ref(object);
    }

    inline explicit anchor(const core::refcounted_mt* object) noexcept
        : word_(object ? reinterpret_cast<std::uintptr_t>(object) | mt_bit : 0) {
        if (object) intrusive_ptr_add_ref(object);
    }

    /// A scenario: the reference, and one more scenario of the owner running.
    inline explicit anchor(const scenario_owner* owner) noexcept;

    anchor(const anchor&) = delete;
    anchor& operator=(const anchor&) = delete;

    /// A word with no tag lets go as it always did. What a scenario does on the
    /// way out is out of line (detached_task.cpp), in this and the two members
    /// below: a frame that is no scenario keeps a test, not a copy of the path.
    inline ~anchor() {
        if (word_ == 0) return;

        if (word_ & tag_bits) {
            if (word_ & mt_bit)
                intrusive_ptr_release(
                    reinterpret_cast<const core::refcounted_mt*>(word_ & ~mt_bit));
            else
                leave_destroyed();
        } else {
            intrusive_ptr_release(reinterpret_cast<const core::refcounted*>(word_));
        }
    }

    /// At the final point: whether the frame goes by itself, as every detached
    /// frame does -- or stays for a moment, being the last running scenario of an
    /// owner somebody waits for.
    inline bool ends_alone() noexcept { return (word_ & scenario_bit) == 0 || leave_at_end(); }

    /// The thread goes to the waiter, and the frame, stopped at its final point,
    /// with it: the waiter destroys the frame before its co_await returns.
    std::coroutine_handle<> hand_over(std::coroutine_handle<> self) noexcept;

    /// Whether this is a running scenario of `owner`.
    inline bool scenario_of(const scenario_owner& owner) const noexcept;

private:
    static constexpr std::uintptr_t mt_bit = 1;
    static constexpr std::uintptr_t scenario_bit = 2;
    static constexpr std::uintptr_t tag_bits = mt_bit | scenario_bit;

    inline const scenario_owner& held() const noexcept;

    /// The scenario leaves at its final point; the frame keeps the reference alone.
    bool leave_at_end() noexcept;

    /// A scenario whose frame is destroyed before its final point leaves here.
    void leave_destroyed() noexcept;

    std::uintptr_t word_ = 0;
};

/// The final point of a detached frame: no suspension at all, unless the frame
/// is the last scenario of an owner somebody waits for -- then it stops there,
/// and the thread goes to the waiter. The answer is all the awaiter keeps; the
/// way to the waiter is asked of the promise.
struct final_handover {
    bool alone;

    inline bool await_ready() const noexcept { return alone; }

    template <class Promise>
    inline std::coroutine_handle<> await_suspend(
        std::coroutine_handle<Promise> self) const noexcept {
        return self.promise().hand_over(self);
    }

    inline void await_resume() const noexcept {}
};

}  // namespace detached_task_detail

/// An object that waits for its own detached coroutines -- its scenarios.
///
/// A `detached_task` whose first argument is a scenario_owner, by reference or
/// by pointer -- for a member coroutine, the object itself -- is one of its
/// scenarios: counted from the call until its frame is gone, however the body
/// ends, and holding the owner meanwhile, as any counted first argument is held.
/// The type of that parameter decides, when the coroutine is compiled: an owner
/// passed as `core::refcounted&` is held and not counted, and a coroutine with
/// any other first argument does nothing here.
///
/// `co_await owner.scenarios_ended()` resumes the awaiting coroutine once no
/// scenario of the owner runs, and does not suspend if none does. A scenario
/// awaiting it does not wait for itself: while it stands in the wait it is not
/// counted. It is known by its promise, so a `task` awaiting the end is not a
/// scenario, even one a scenario awaits -- whose wait then cannot end, since it
/// waits for the scenario it is part of. One coroutine waits at a time.
///
/// The ends of the scenarios alone settle the wait, and the same ends settle it
/// the same way:
///
/// - The waiter is resumed at the final point of the scenario that leaves none
///   running, by symmetric transfer, before control goes back to whoever resumed
///   that scenario. That frame is destroyed before the waiter's co_await returns:
///   its locals went with its body, its promise -- the reference to the owner --
///   and its parameter copies go then.
/// - A scenario started while the wait stands, by a scenario or from outside, is
///   counted, and the wait lasts until it ends too.
/// - A scenario started once the waiter is resumed -- by the waiter, by whoever
///   resumed the last one, by the destruction of its frame -- is not waited for;
///   `scenarios()` shows it, and the waiter may await again.
/// - A scenario frame destroyed from outside, through its handle, never reaches
///   its final point: it leaves in its promise's destructor, and if it was the
///   last one, the waiter is resumed there, inside that destroy(), after the
///   frame has let go of the owner and before its parameter copies go. wxl
///   destroys no detached frame; a detached_task ends through its body.
/// - A waiter whose frame is destroyed while it waits takes itself off.
///
/// Nothing here ends a scenario or limits the wait. A scenario that is to end
/// sooner is asked to through its token (cancellation_source); whether to ask,
/// and whether and when to wait, is the application's to say.
///
/// One thread, the one the scenarios run on: the counts are plain fields. A build
/// that checks coroutines stops at a second waiter, at a wait from another thread,
/// and at an owner destroyed while a scenario runs -- which an owner counted on
/// the heap never is, its scenarios holding it.
class scenario_owner : public core::sta_refcounted
{
public:
    /// How many scenarios of this owner run: not counting one that stands in
    /// scenarios_ended().
    inline std::size_t scenarios() const noexcept { return running_; }

    /// \return what a coroutine awaits, where it is made -- `co_await
    ///         scenarios_ended();` -- to be resumed once no scenario of this owner
    ///         runs; it does not suspend if none does.
    [[nodiscard]] inline detached_task_detail::scenarios_end scenarios_ended() noexcept;

protected:
    scenario_owner() noexcept = default;

    /// A waiter stands only while a scenario runs, so one check covers both. Where
    /// a destructor is called nothing gives the line: the check names its own.
    inline ~scenario_owner() override {
        coro_check(running_ == 0, "scenario_owner: destroyed while a scenario of it runs",
                   coro_detail::site::current());
    }

private:
    friend class detached_task_detail::anchor;
    friend class detached_task_detail::scenarios_end;

    /// Mutable like the reference count beside it: a scenario of a const member
    /// is counted all the same.
    mutable std::size_t running_ = 0;
    mutable detached_task_detail::scenarios_end* wait_ = nullptr;
};

/// A coroutine that owns itself: nothing is returned to hold, and the frame
/// is released the moment the body ends.
///
/// Detached in the sense of `std::thread::detach()`: it runs on independently
/// of whoever started it, frees its own resources when it is done, and cannot
/// be joined or stopped from outside. It is what almost all GUI code writes
/// -- a loop watching a button has no result and no reader, and making the
/// caller keep it alive would turn a hundred such loops into a hundred
/// entries in a container somebody has to maintain. Where there *is*
/// something to hold and read, the type is `task`.
///
/// **The price is a contract: it has to end.** Nobody holds it, so nobody can
/// stop it; the only way out is through the body, which means every
/// suspension in it must be on something that can end. wxl's event waits can:
/// they are registered while suspended, and going down tells all of them at
/// once (see wxl.ui's impl/event_waits.h). An awaitable with no way to
/// finish would strand this frame for the life of the process, silently.
///
/// Cancellation is therefore not a failure here but the ordinary end, and is
/// swallowed. Anything else goes to on_detached_task_failure(), because by
/// then there is no caller left to give it to -- rethrowing would carry it
/// out of a resume, which for an event wait means out through a COM delegate.
///
/// **A first argument that counts its own references is held by the frame.**
/// For a member function that argument is the object, so a member coroutine
/// cannot outlive its `this`: when the first argument derives from
/// `core::refcounted` or `core::refcounted_mt`, by reference or by pointer,
/// the promise takes a reference to it and gives it back as the frame goes --
/// which may be what destroys the object. An object nobody holds by count yet,
/// one still in its constructor or one on the stack, goes from one to two and
/// back, and stays its owner's. Only here: a `task` is held by somebody, and
/// an object keeping its own task in a field would then keep itself. It
/// follows that the object's destructor cannot be what ends such a coroutine.
///
/// **A first argument that is a `scenario_owner` makes it a scenario of that
/// owner**: held the same way, and counted until its frame is gone, so that the
/// owner can wait for all of them to end.
class detached_task
{
public:
    struct promise_type {
        /// A coroutine whose first argument does not count its references:
        /// nothing is held.
        promise_type() noexcept = default;

        /// A coroutine whose first argument -- for a member function, the
        /// object -- counts its references holds one of them until the frame
        /// goes. The counting base is chosen by the ordinary conversion from
        /// the derived type, by reference or by pointer; a null pointer holds
        /// nothing. The promise is destroyed before the parameter copies are,
        /// so a parameter whose destructor reaches the object may find it gone.
        template <class... Args>
        inline promise_type(const core::refcounted& self, Args&...) noexcept
            : anchor_(std::addressof(self)) {}

        template <class... Args>
        inline promise_type(const core::refcounted_mt& self, Args&...) noexcept
            : anchor_(std::addressof(self)) {}

        template <class... Args>
        inline promise_type(const core::refcounted* self, Args&...) noexcept : anchor_(self) {}

        template <class... Args>
        inline promise_type(const core::refcounted_mt* self, Args&...) noexcept : anchor_(self) {}

        /// A coroutine whose first argument is a scenario_owner is one of its
        /// scenarios: held as above, and counted. Chosen over the counting base
        /// by the same conversion, since the owner derives from it.
        template <class... Args>
        inline promise_type(const scenario_owner& owner, Args&...) noexcept
            : anchor_(std::addressof(owner)) {}

        template <class... Args>
        inline promise_type(const scenario_owner* owner, Args&...) noexcept : anchor_(owner) {}

        /// The frame, from the pool -- the same reasoning as `task`: a small
        /// object made and unmade on the one thread, over and over.
        inline static void* operator new(std::size_t size) {
            return core::sta_memory_pool::alloc(size);
        }

        inline static void operator delete(void* mem, std::size_t size) noexcept {
            core::sta_memory_pool::free(mem, size);
        }

        inline detached_task get_return_object() const noexcept { return {}; }

        /// Starts where it is called, like `task`: whatever the body does
        /// before its first co_await has happened by the time the call
        /// returns, so a subscription made there is already live.
        inline std::suspend_never initial_suspend() const noexcept { return {}; }

        /// And releases itself at the end. This is the whole difference from
        /// `task`, whose frame stays for its owner to read. The one frame that
        /// stops at its final point is the last running scenario of an owner
        /// somebody waits for: the thread goes to the waiter, which destroys it.
        inline detached_task_detail::final_handover final_suspend() noexcept {
            return {anchor_.ends_alone()};
        }

        /// The last scenario of an owner, stopped at its final point, hands the
        /// thread to the coroutine waiting for the owner's scenarios.
        inline std::coroutine_handle<> hand_over(std::coroutine_handle<> self) noexcept {
            return anchor_.hand_over(self);
        }

        inline void return_void() const noexcept {}

        inline void unhandled_exception() const noexcept {
            const std::exception_ptr error = std::current_exception();

            try {
                std::rethrow_exception(error);
            } catch (const operation_canceled_exception&) {
                // The ordinary end, not a failure: something this coroutine
                // was waiting for is never going to happen, and letting the
                // body unwind is how it was told.
            } catch (...) {
                on_detached_task_failure()(error);
            }
        }

        /// Whether this coroutine is a running scenario of `owner`: the wait for
        /// the owner's scenarios asks it of the coroutine that awaits it.
        inline bool scenario_of(const scenario_owner& owner) const noexcept {
            return anchor_.scenario_of(owner);
        }

    private:
        detached_task_detail::anchor anchor_;
    };
};

namespace detached_task_detail {

/// What `co_await owner.scenarios_ended()` stands in: the owner's one waiter.
///
/// Made where it is awaited and never moved: the owner points at it while the
/// coroutine waits, and the scenario that ends last hands it its own frame to
/// destroy.
class [[nodiscard("waiting for the scenarios does nothing until it is co_awaited")]] scenarios_end
{
public:
    inline explicit scenarios_end(scenario_owner& owner) noexcept : owner_(owner) {}

    scenarios_end(const scenarios_end&) = delete;
    scenarios_end& operator=(const scenarios_end&) = delete;

    /// A waiting frame destroyed takes itself off, and a scenario counts again
    /// for its promise to leave.
    inline ~scenarios_end() {
        if (waiter_) {
            owner_.wait_ = nullptr;
            if (counted_) ++owner_.running_;
        }
    }

    inline bool await_ready(
        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) const noexcept {
        coro_check(core::sta_memory_pool::is_safe(),
                   "scenario_owner: awaited from a thread other than its scenarios'", where);
        return owner_.running_ == 0;
    }

    /// A scenario of this owner stops being counted while it waits, and does not
    /// suspend when it is the only one; any other coroutine waits for all.
    template <class Promise>
    inline bool await_suspend(
        std::coroutine_handle<Promise> waiter,
        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) noexcept {
        coro_check(owner_.wait_ == nullptr,
                   "scenario_owner: awaited by a second coroutine while the first one still waits",
                   where);

        if constexpr (std::is_same_v<Promise, detached_task::promise_type>) {
            if (waiter.promise().scenario_of(owner_)) {
                if (owner_.running_ == 1) return false;

                --owner_.running_;
                counted_ = true;
            }
        }

        waiter_ = waiter;
        owner_.wait_ = this;
        return true;
    }

    /// The last scenario's frame, stopped at its final point, goes before the
    /// waiter does anything else -- and with it, perhaps, the owner: nothing of
    /// the owner is touched past this.
    inline void await_resume() const noexcept {
        if (ended_) ended_.destroy();
    }

private:
    friend class anchor;

    /// The last scenario has ended: the waiter counts again if it is a scenario,
    /// and is handed the ended frame, if it stopped, to destroy.
    inline std::coroutine_handle<> take_over(std::coroutine_handle<> ended) noexcept {
        ended_ = ended;
        if (counted_) ++owner_.running_;
        return std::exchange(waiter_, {});
    }

    scenario_owner& owner_;

    /// Set while the coroutine waits, and only then.
    std::coroutine_handle<> waiter_;
    std::coroutine_handle<> ended_;
    bool counted_ = false;
};

inline anchor::anchor(const scenario_owner* owner) noexcept
    : word_(owner ? reinterpret_cast<std::uintptr_t>(static_cast<const core::refcounted*>(owner)) |
                        scenario_bit
                  : 0) {
    if (owner) {
        intrusive_ptr_add_ref(static_cast<const core::refcounted*>(owner));
        ++owner->running_;
    }
}

inline const scenario_owner& anchor::held() const noexcept {
    return static_cast<const scenario_owner&>(
        *reinterpret_cast<const core::refcounted*>(word_ & ~tag_bits));
}

inline bool anchor::scenario_of(const scenario_owner& owner) const noexcept {
    return word_ ==
           (reinterpret_cast<std::uintptr_t>(static_cast<const core::refcounted*>(&owner)) |
            scenario_bit);
}

}  // namespace detached_task_detail

inline detached_task_detail::scenarios_end scenario_owner::scenarios_ended() noexcept {
    return detached_task_detail::scenarios_end(*this);
}

}  // export namespace wxl::async
