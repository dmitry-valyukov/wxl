module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

void async_op::answer_canceled() noexcept {
    // Made the first time it is needed, by whichever thread needs it first, and copied
    // from then on: a copy of the pointer is all an answer costs.
    static const std::exception_ptr canceled =
        std::make_exception_ptr(operation_canceled_exception());

    ::new (error_slot()) std::exception_ptr(canceled);
}

void async_op::answer_current_exception() noexcept {
    try {
        throw;
    } catch (const last_error& failure) {
        if (failure.code().value() == ERROR_OPERATION_ABORTED)
            answer_canceled();
        else
            set_current_exception();
    } catch (...) {
        set_current_exception();
    }
}

bool orphan_state::try_begin() noexcept {
    thread_ = ::GetCurrentThreadId();

    stage found = stage::waiting;

    return stage_.compare_exchange_strong(found, stage::running);
}

bool orphan_state::end() noexcept {
    stage found = stage::running;

    if (stage_.compare_exchange_strong(found, stage::finished)) [[likely]]
        return true;

    for (;;) {
        switch (found) {
            // Whoever cancelled the operation or gave it up may be aborting the call
            // this thread stood in. Until that is over the thread stays here: the next call it
            // makes is somebody else's.
            case stage::cancelling_io:
                stage_.wait(found);
                found = stage_.load();
                break;

            // Cancelled, it keeps what it made -- unless it is given up meanwhile, which the
            // exchange settles with the thread giving it up.
            case stage::canceled:
                if (stage_.compare_exchange_strong(found, stage::finished)) return true;
                break;

            default:
                return false;
        }
    }
}

bool orphan_state::abandon() noexcept {
    stage found = stage_.load();

    for (;;) {
        switch (found) {
            case stage::waiting:
                if (stage_.compare_exchange_strong(found, stage::abandoned)) return false;
                break;

            // Cancelled before, its call has been aborted once; the body may have gone on to
            // another since, which is aborted the same way.
            case stage::running:
            case stage::canceled:
                if (!stage_.compare_exchange_strong(found, stage::cancelling_io)) break;

                cancel_synchronous_io();

                stage_.store(stage::abandoned);
                stage_.notify_one();
                return false;

            case stage::finished:
                if (stage_.compare_exchange_strong(found, stage::abandoned)) return true;
                break;

            default:
                return false;
        }
    }
}

void orphan_state::cancel() noexcept {
    stage found = stage_.load();

    for (;;) {
        switch (found) {
            // Not started, it does not start, as one given up does not: the worker that reaches
            // it answers the cancellation.
            case stage::waiting:
                if (stage_.compare_exchange_strong(found, stage::abandoned)) return;
                break;

            case stage::running:
                if (!stage_.compare_exchange_strong(found, stage::cancelling_io)) break;

                cancel_synchronous_io();

                stage_.store(stage::canceled);
                stage_.notify_one();
                return;

            // Finished first, it keeps what it made.
            default:
                return;
        }
    }
}

void orphan_state::cancel_synchronous_io() noexcept {
    // The body is held inside the operation from here until the stage moves on, so what is
    // aborted can only be a call of its own.
    if (const HANDLE thread = ::OpenThread(THREAD_TERMINATE, FALSE, thread_)) {
        ::CancelSynchronousIo(thread);
        ::CloseHandle(thread);
    }
}

}  // namespace wxl::async
