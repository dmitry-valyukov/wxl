module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

bool orphan_stage::enter() noexcept {
    thread_ = ::GetCurrentThreadId();

    stage found = stage::waiting;

    return stage_.compare_exchange_strong(found, stage::running);
}

bool orphan_stage::leave() noexcept {
    stage found = stage::running;

    if (stage_.compare_exchange_strong(found, stage::finished)) [[likely]]
        return true;

    // Whoever gave the operation up may be cutting short the call this thread stood
    // in. Until that is over the thread stays here: the next call it makes is somebody
    // else's.
    while (found == stage::cutting_short) {
        stage_.wait(found);
        found = stage_.load();
    }

    return false;
}

bool orphan_stage::give_up() noexcept {
    stage found = stage_.load();

    for (;;) {
        switch (found) {
        case stage::waiting:
            if (stage_.compare_exchange_strong(found, stage::given_up)) return false;
            break;

        case stage::running:
            if (!stage_.compare_exchange_strong(found, stage::cutting_short)) break;

            // The body is held inside the operation from here until the stage moves
            // on, so what is cut short can only be a call of its own.
            if (const HANDLE thread = ::OpenThread(THREAD_TERMINATE, FALSE, thread_)) {
                ::CancelSynchronousIo(thread);
                ::CloseHandle(thread);
            }

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

}  // namespace wxl::async
