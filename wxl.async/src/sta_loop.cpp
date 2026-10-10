module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

void async_op::abandon(async_op* op) noexcept {
    op->fate_ = fate::abandoned;

    // Left to finish alone, wherever it is; one that has not started will not.
    if (op->orphanable_) {
        op->cancel();
        return;
    }

    bool owed = false;

    // One already in the return channel is the worker's no more, and there is nothing
    // to cancel.
    if (!sta_loop::is_back(op)) {
        op->cancel();

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

    if (take(taken)) delete taken;
}

void sta_loop::pay_owed_callback() noexcept {
    // run_pending()'s way out: the trigger set again, and checked against what came
    // back while it was clear, which nobody has told the dispatcher about.
    from_worker_.arm();
    std::atomic_thread_fence(std::memory_order_seq_cst);

    auto at = from_worker_reader_.look_ahead();

    if (from_worker_reader_.peek(at) && from_worker_.disarm()) from_worker_.wakeup().set();
}

void sta_loop::take_back_outstanding() noexcept {
    sta_signal& signal = from_worker_.wakeup();

    // Nobody is called back from here on: what the dispatcher was owed is taken below.
    if (signal.driven()) from_worker_.disarm();

    signal.hold();

    while (outstanding_ != 0) {
        async_op* op = nullptr;

        if (take(op)) {
            op->settle();
            continue;
        }

        from_worker_.arm();
        std::atomic_thread_fence(std::memory_order_seq_cst);

        auto at = from_worker_reader_.look_ahead();

        if (!from_worker_reader_.peek(at)) signal.wait_held();

        from_worker_.disarm();
    }

    signal.release();
}

void sta_loop::worker::run() {
    // Built here rather than beside the channels: the reader belongs to the thread
    // that reads, and this is it.
    to_worker_t::reader reader(to_worker_);
    io_port& port = to_worker_.wakeup();

    OVERLAPPED_ENTRY arrived[64];

    for (;;) {
        bool worked = false;

        for (async_op* op = nullptr; reader.read(op); worked = true) execute(op);

        // The channel's waiting protocol around the port's sleep: armed, fenced, looked
        // at once more, and only then asleep. A closed channel is never slept on: the
        // wake-up that came with the close may already have been taken by a look
        // into the port after a run of work.
        if (!worked) {
            to_worker_.arm();
            std::atomic_thread_fence(std::memory_order_seq_cst);

            if (async_op* op = nullptr; reader.read(op)) {
                to_worker_.disarm();
                execute(op);
                worked = true;
            } else if (to_worker_.closed()) {
                to_worker_.disarm();
                break;
            }
        }

        // After a run of work the port is only looked into, so that what the kernel
        // has finished does not wait for the queue to run dry.
        const std::size_t count = port.take(arrived, !worked);

        if (!worked) to_worker_.disarm();

        for (const OVERLAPPED_ENTRY& entry : std::span(arrived, count)) {
            // The queue's wake-up is a packet with no operation behind it.
            if (!entry.lpOverlapped) continue;

            io_op* const op = io_op::from(entry.lpOverlapped);

            if (op->completed()) from_worker_.send(op);
        }

        if (!worked && to_worker_.closed()) break;
    }

    // A latched close is the promise that nothing more can be sent, so what is left
    // in the queue is everything that will ever be there -- and it was accepted, so
    // it gets done.
    for (async_op* op = nullptr; reader.read(op);) execute(op);
}

}  // namespace wxl::async
