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

    error_ = canceled;
}

void async_op::keep_failure() noexcept {
    try {
        throw;
    } catch (const system_exception& failure) {
        if (failure.err_code() == ERROR_OPERATION_ABORTED)
            answer_canceled();
        else
            error_ = std::current_exception();
    } catch (...) {
        error_ = std::current_exception();
    }
}

bool orphan_stage::enter() noexcept {
    thread_ = ::GetCurrentThreadId();

    stage found = stage::waiting;

    return stage_.compare_exchange_strong(found, stage::running);
}

bool orphan_stage::leave() noexcept {
    stage found = stage::running;

    if (stage_.compare_exchange_strong(found, stage::finished)) [[likely]]
        return true;

    for (;;) {
        switch (found) {
        // Whoever told the operation to stop or gave it up may be cutting short the call
        // this thread stood in. Until that is over the thread stays here: the next call it
        // makes is somebody else's.
        case stage::cutting_short:
            stage_.wait(found);
            found = stage_.load();
            break;

        // Told, it keeps what it made -- unless it is given up meanwhile, which the
        // exchange settles with the thread giving it up.
        case stage::told:
            if (stage_.compare_exchange_strong(found, stage::finished)) return true;
            break;

        default:
            return false;
        }
    }
}

bool orphan_stage::give_up() noexcept {
    stage found = stage_.load();

    for (;;) {
        switch (found) {
        case stage::waiting:
            if (stage_.compare_exchange_strong(found, stage::given_up)) return false;
            break;

        // Told before, its call has been cut short once; the body may have gone on to
        // another since, which is cut short the same way.
        case stage::running:
        case stage::told:
            if (!stage_.compare_exchange_strong(found, stage::cutting_short)) break;

            cut_the_call_short();

            stage_.store(stage::given_up);
            stage_.notify_one();
            return false;

        case stage::finished:
            if (stage_.compare_exchange_strong(found, stage::given_up)) return true;
            break;

        default:
            return false;
        }
    }
}

void orphan_stage::tell() noexcept {
    stage found = stage_.load();

    for (;;) {
        switch (found) {
        // Not started, it does not start, as one given up does not: the worker that reaches
        // it answers the cancellation.
        case stage::waiting:
            if (stage_.compare_exchange_strong(found, stage::given_up)) return;
            break;

        case stage::running:
            if (!stage_.compare_exchange_strong(found, stage::cutting_short)) break;

            cut_the_call_short();

            stage_.store(stage::told);
            stage_.notify_one();
            return;

        // Finished first, it keeps what it made.
        default:
            return;
        }
    }
}

void orphan_stage::cut_the_call_short() noexcept {
    // The body is held inside the operation from here until the stage moves on, so what is
    // cut short can only be a call of its own.
    if (const HANDLE thread = ::OpenThread(THREAD_TERMINATE, FALSE, thread_)) {
        ::CancelSynchronousIo(thread);
        ::CloseHandle(thread);
    }
}

}  // namespace wxl::async
