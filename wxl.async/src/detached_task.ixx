export module wxl.async:detached_task;

import :cancellation;
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
/// program should do instead. It ends through std::terminate from inside the
/// catch of the rethrown exception, so the terminate handler wxl sets names
/// the exception, once -- a winrt::hresult_error by its code and message too.
inline detached_task_failure_handler& on_detached_task_failure() noexcept {
    static detached_task_failure_handler handler = &core::impl::terminate_with_exception;
    return handler;
}

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
/// `core::refcounted` -- the STA counter, `sta_refcounted` included -- by
/// reference or by pointer, the promise takes a reference to it and gives it
/// back as the frame goes -- which may be what destroys the object. An object
/// nobody holds by count yet, one still in its constructor or one on the
/// stack, goes from one to two and back, and stays its owner's. Only here: a
/// `task` is held by somebody, and an object keeping its own task in a field
/// would then keep itself. It follows that the object's destructor cannot be
/// what ends such a coroutine. An object counted for several threads
/// (`core::refcounted_mt`) is not held: the frame lives in the STA pool, so a
/// detached coroutine is a thing of that thread, and so is what owns one.
class detached_task
{
public:
    struct promise_type {
        /// A coroutine whose first argument does not count its references:
        /// nothing is held.
        promise_type() noexcept = default;

        /// A coroutine whose first argument -- for a member function, the
        /// object -- counts its references with the STA counter holds one of
        /// them until the frame goes. The counting base is reached by the
        /// ordinary conversion from the derived type, by reference or by
        /// pointer; a null pointer holds nothing. The reference is taken, not
        /// adopted: the object is alive and somebody's already, and its birth
        /// reference stays with its owner. The promise is destroyed before the
        /// parameter copies are, so a parameter whose destructor reaches the
        /// object may find it gone.
        template <class... Args>
        inline promise_type(const core::refcounted& self, Args&...) noexcept
            : self_(std::addressof(self)) {}

        template <class... Args>
        inline promise_type(const core::refcounted* self, Args&...) noexcept : self_(self) {}

        /// The frame, from the pool -- the same reasoning as `task`: a small
        /// object made and unmade on the one thread, over and over. The pool
        /// asserts the thread in debug builds, so a frame made anywhere else is
        /// caught here. Counted: a frame is alive from here to operator delete.
        inline static void* operator new(std::size_t size) {
            void* const frame = core::sta_memory_pool::alloc(size);
            ++live_;
            return frame;
        }

        inline static void operator delete(void* mem, std::size_t size) noexcept {
            --live_;
            core::sta_memory_pool::free(mem, size);
        }

        inline detached_task get_return_object() const noexcept { return {}; }

        /// Starts where it is called, like `task`: whatever the body does
        /// before its first co_await has happened by the time the call
        /// returns, so a subscription made there is already live.
        inline std::suspend_never initial_suspend() const noexcept { return {}; }

        /// And releases itself at the end. This is the whole difference from
        /// `task`, whose frame stays for its owner to read.
        inline std::suspend_never final_suspend() const noexcept { return {}; }

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

    private:
        core::intrusive_ptr<const core::refcounted> self_;
    };

    /// How many frames of this kind exist -- made and not yet unmade, suspended or
    /// running. Nobody holds such a frame, so nobody but the frame itself can end it,
    /// and a process has no business ending while one is alive: this is the count an
    /// exit waits on. A plain number, since the frames live on the one STA thread.
    static std::size_t live_frames() noexcept { return live_; }

private:
    static inline std::size_t live_ = 0;
};

}  // export namespace wxl::async
