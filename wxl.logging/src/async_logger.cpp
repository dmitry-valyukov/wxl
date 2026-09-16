module;

#include "abi.h"

module wxl.logging;

import wxl.async;
import wxl.core;
import std;

namespace wxl::logging {
namespace {

/// wxl::core::duration counts 100ns ticks and so does the system clock on
/// Windows, but neither promises the other anything, so the conversion is
/// spelled out once here instead of assumed at four call sites.
constexpr log_clock::duration as_clock_duration(const core::duration span) noexcept {
    using hundred_ns = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

    return std::chrono::duration_cast<log_clock::duration>(
        hundred_ns(static_cast<std::int64_t>(span.ticks())));
}

constexpr core::duration as_wxl_duration(const log_clock::duration span) noexcept {
    using hundred_ns = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

    const auto ticks = std::chrono::duration_cast<hundred_ns>(span).count();

    return core::duration::from_ticks(ticks > 0 ? static_cast<std::uint64_t>(ticks) : 0);
}

}  // namespace

// --- async_output ---

async_output::async_output(const core::duration window, async::thread_group* const group)
    : threaded_component("wxl.logging", group), window_(window) {}

async_output::~async_output() {
    dispose();
}

void async_output::on_stopping() {
    threaded_component::on_stopping();

    // The thread is asleep on the queue with no deadline whenever it holds
    // nothing, so the stop has to knock.
    channel_.signal(true);
}

void async_output::enqueue(entry_ptr& entry) {
    channel_.send(entry);
}

void async_output::flush_all() {
    ask_for_flush(flush_kind::all);
}

void async_output::flush_window() {
    ask_for_flush(flush_kind::window);
}

void async_output::ask_for_flush(const flush_kind kind) {
    {
        const core::lock_guard<core::mutex> lock(flush_mutex_);

        // The thread has left its loop, or never entered it: there is nobody to
        // answer, and the stop writes out everything anyway.
        if (!accepting_requests_) return;

        request_.kind = std::max(request_.kind, kind);
        ++request_.waiters;
    }

    channel_.signal(true);

    answered_.acquire();
}

async_output::flush_request async_output::take_request() {
    const core::lock_guard<core::mutex> lock(flush_mutex_);

    // Taken before the round does its work, not after: a request that arrives
    // while the writing is under way waits for the next round, which is the
    // only way "everything enqueued before the call" can mean anything.
    return std::exchange(request_, flush_request{});
}

std::size_t async_output::close_requests() {
    const core::lock_guard<core::mutex> lock(flush_mutex_);

    accepting_requests_ = false;

    return std::exchange(request_, flush_request{}).waiters;
}

void async_output::answer(const std::size_t waiters) {
    for (std::size_t answered = 0; answered < waiters; ++answered) answered_.release();
}

void async_output::run() {
    while (!stop_requested()) {
        const flush_request asked = take_request();

        take_everything_queued();

        if (asked.kind == flush_kind::all)
            write_everything();
        else
            write_due(log_clock::now());

        answer(asked.waiters);

        if (stop_requested()) break;

        wait_for_more();
    }

    // Everything still held or queued belongs in the log: a log that dropped
    // its last lines would do it exactly when they mattered most. Requests are
    // closed first, so that a caller arriving now is turned away rather than
    // left waiting for a thread that is about to be gone.
    const std::size_t last_waiters = close_requests();

    take_everything_queued();
    write_everything();

    answer(last_waiters);
}

void async_output::take_everything_queued() {
    entry_ptr taken;

    while (channel_.try_receive(taken)) hold(std::move(taken));
}

void async_output::hold(entry_ptr entry) {
    const log_time made_at = entry->time;

    // upper_bound and not lower_bound: entries made at the same instant keep the
    // order they arrived in, which is the best a clock this coarse can do.
    const auto at = std::ranges::upper_bound(held_, made_at, {},
                                             [](const entry_ptr& held) { return held->time; });

    held_.insert(at, std::move(entry));
}

void async_output::write_due(const log_time now) {
    if (held_.empty()) return;

    const log_clock::duration window = as_clock_duration(window_);

    // One lock for the batch rather than one per line: the outputs are shared
    // with whatever else writes to them, and this is the thread that has the
    // most to say.
    const core::lock_guard<core::mutex> lock(outputs_mutex());

    while (!held_.empty() && held_.front()->time + window <= now) {
        write_unlocked(*held_.front());
        held_.pop_front();
    }
}

void async_output::write_everything() {
    if (held_.empty()) return;

    const core::lock_guard<core::mutex> lock(outputs_mutex());

    while (!held_.empty()) {
        write_unlocked(*held_.front());
        held_.pop_front();
    }
}

core::duration async_output::time_until_due(const log_time now) const {
    ensure(!held_.empty());

    const log_time due = held_.front()->time + as_clock_duration(window_);

    return due > now ? as_wxl_duration(due - now) : core::duration::zero();
}

void async_output::wait_for_more() {
    entry_ptr taken;

    if (held_.empty()) {
        // Nothing to be woken for but an arrival, so wait for one and nothing
        // else. flush and stop both come through signal().
        channel_.receive(taken);
    } else {
        const core::duration until_due = time_until_due(log_clock::now());

        // Due already: go round rather than wait for zero. write_due() will
        // have it out of the way before this is asked again.
        if (!until_due) return;

        channel_.receive_for(taken, until_due);
    }

    if (taken.get()) hold(std::move(taken));
}

// --- async_logger ---

async_logger::async_logger(async_sink& sink, const severity level, const std::size_t warm_entries,
                           const std::size_t pool_size)
    : base_logger(level), sink_(sink), pool_(pool_size) {
    // Held all at once and released together, so that the pool really does end
    // up with that many: taken one at a time it would hand back the same entry.
    std::vector<entry_ptr> warming;

    warming.reserve(warm_entries);

    for (std::size_t made = 0; made < warm_entries; ++made)
        warming.push_back(entry_ptr(pool_.get()));
}

log_entry& async_logger::begin_entry() {
    current_.reset(pool_.get());
    current_->reset();

    return *current_;
}

void async_logger::commit_entry(log_entry& entry) {
    ensure(&entry == static_cast<log_entry*>(current_.get()));

    sink_.enqueue(current_);
}

}  // namespace wxl::logging
