module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

void async_op::abandon(async_op* op) noexcept {
    op->abandoned_ = true;

    bool owed = false;

    // One already in the return channel is the worker's no more, and there is nothing
    // to cancel.
    if (!sta_loop::is_back(op)) {
        op->canceled_.store(true, std::memory_order_relaxed);
        op->on_cancel();

        if (op->orphanable_) return;

        owed = sta_loop::wait_until_back(op);
    }

    // Taken before the debt is paid, so that a channel holding nothing else is not
    // called back about.
    sta_loop::take_if_next(op);

    if (owed) sta_loop::pay_owed_callback();
}

bool sta_loop::is_back(async_op* op) noexcept {
    auto at = from_worker_reader_.look_ahead();

    while (async_op** const next = from_worker_reader_.peek(at))
        if (*next == op) return true;

    return false;
}

bool sta_loop::wait_until_back(async_op* op) noexcept {
    sta_signal& signal = from_worker_.wakeup();

    // In the driven shape an armed trigger is a callback the next handover owes the
    // dispatcher. Taken here, it makes this thread the one looking, and the debt this
    // call's to pay once it has stopped looking.
    const bool owed = signal.driven() && from_worker_.disarm();

    signal.hold();

    // The channel's own waiting protocol, with a look where receive() reads: armed,
    // fenced, looked once more, and only then asleep -- so a handover made between the
    // last look and the arming is found by the second look rather than lost.
    do {
        from_worker_.arm();
        std::atomic_thread_fence(std::memory_order_seq_cst);

        if (is_back(op)) {
            from_worker_.disarm();
            break;
        }

        signal.wait_held();
        from_worker_.disarm();
    } while (!is_back(op));

    signal.release();

    return owed;
}

void sta_loop::take_if_next(async_op* op) noexcept {
    auto at = from_worker_reader_.look_ahead();

    if (async_op** const next = from_worker_reader_.peek(at); !next || *next != op) return;

    async_op* taken = nullptr;

    if (from_worker_reader_.read(taken)) delete taken;
}

void sta_loop::pay_owed_callback() noexcept {
    // run_pending()'s way out: the trigger set again, and checked against what came
    // back while it was clear, which nobody has told the dispatcher about.
    from_worker_.arm();
    std::atomic_thread_fence(std::memory_order_seq_cst);

    auto at = from_worker_reader_.look_ahead();

    if (from_worker_reader_.peek(at) && from_worker_.disarm()) from_worker_.wakeup().set();
}

}  // namespace wxl::async
