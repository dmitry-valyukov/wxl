export module wxl.async:task;

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
using task_failure_handler = void (*)(std::exception_ptr) noexcept;

/// The handler, settable by the application. The default ends the process,
/// which is the same answer cppwinrt gives and the only honest one by
/// default: the exception has escaped a coroutine that nobody is waiting on,
/// so there is nowhere left to report it and nobody to decide what the
/// program should do instead.
inline task_failure_handler& on_task_failure() noexcept {
    static task_failure_handler handler = [](std::exception_ptr error) noexcept {
        // Named before the process goes, because the alternative is what this
        // replaced: a bare "abort() has been called" that says nothing about
        // which coroutine failed or why. Ending is still the only honest
        // answer -- nobody is waiting on this one, so there is nobody to
        // decide anything else -- but ending silently is not.
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& what) {
            std::cerr << "wxl: a task failed with nobody to tell: " << what.what() << '\n';
        } catch (...) {
            std::cerr << "wxl: a task failed with a foreign exception and nobody to tell\n";
        }

        std::cerr.flush();
        std::terminate();
    };
    return handler;
}

/// A coroutine that owns itself: nothing is returned to hold, and the frame
/// is released the moment the body ends.
///
/// This is the ordinary one, and it has the ordinary name for the same reason
/// every other framework gives it that name -- it is what almost all code
/// writes. A loop watching a button has no result and no reader, and making
/// the caller keep it alive would turn a hundred such loops into a hundred
/// entries in a container somebody has to maintain. Where there *is* something
/// to hold and read, the type is `managed_task`, and it says so.
///
/// **The price is a contract: it has to end.** Nobody holds it, so nobody can
/// stop it; the only way out is through the body, which means every
/// suspension in it must be on something that can end. wxl's event waits can:
/// they are registered while suspended, and going down tells all of them at
/// once (see wxl.ui's impl/event_waits.h). An awaitable with no way to
/// finish would strand this frame for the life of the process, silently.
///
/// Cancellation is therefore not a failure here but the ordinary end, and is
/// swallowed. Anything else goes to on_task_failure(), because by then there
/// is no caller left to give it to -- rethrowing would carry it out of a
/// resume, which for an event wait means out through a COM delegate.
class task
{
public:
    struct promise_type {
        /// The frame, from the pool -- the same reasoning as `managed_task`: a
        /// small object made and unmade on the one thread, over and over.
        static void* operator new(std::size_t size) { return core::sta_memory_pool::alloc(size); }

        static void operator delete(void* mem, std::size_t size) noexcept {
            core::sta_memory_pool::free(mem, size);
        }

        task get_return_object() const noexcept { return {}; }

        /// Starts where it is called, like `managed_task`: whatever the body
        /// does before its first co_await has happened by the time the call
        /// returns, so a subscription made there is already live.
        std::suspend_never initial_suspend() const noexcept { return {}; }

        /// And releases itself at the end. This is the whole difference from
        /// `managed_task`, whose frame stays for its owner to read.
        std::suspend_never final_suspend() const noexcept { return {}; }

        void return_void() const noexcept {}

        void unhandled_exception() const noexcept {
            const std::exception_ptr error = std::current_exception();

            try {
                std::rethrow_exception(error);
            } catch (const operation_canceled_exception&) {
                // The ordinary end, not a failure: something this coroutine
                // was waiting for is never going to happen, and letting the
                // body unwind is how it was told.
            } catch (...) {
                on_task_failure()(error);
            }
        }
    };
};

}  // export namespace wxl::async
