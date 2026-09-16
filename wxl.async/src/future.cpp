module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

// let's destroy intrusive_ptr in the single place
future_detail::future_base::~future_base() = default;

namespace future_detail {
namespace {

/// Shared state of a when_all_* combination: ready once the last operation counted into it is.
/// The count starts at one, which is what keeps it from reaching zero while the combinator is
/// still adding futures; countdown::get_future() takes that one off at the end.
struct countdown_state : future_shared_state {
    countdown_state() noexcept : counter_(1) {}

    void increment() { ++counter_; }

    void decrement() {
        if (--counter_ == 0) set_value();
    }

    std::atomic<size_t> counter_;
};

countdown_state* as_countdown(const future_shared_state_ptr& state) {
    return static_cast<countdown_state*>(state.get());
}

/// Counts a completion down whichever way it went. noexcept deliberately: a combinator has
/// nowhere to report a failure of its own, so running out of memory mid-completion terminates
/// rather than unwinding through a lock-free resolution.
struct count_down_func {
    void operator()() const noexcept { countdown_->decrement(); }

    core::intrusive_ptr<countdown_state> countdown_;
};

/// Fails the whole combination with the first exception any of its futures reports.
struct fail_all_func {
    void operator()(const std::exception_ptr& e) const noexcept { countdown_->set_exception(e); }

    core::intrusive_ptr<countdown_state> countdown_;
};

/// Resolves a when_any combination from whichever future finishes first, with its outcome.
struct first_of_func {
    void operator()() const noexcept { result_->set_value(); }

    void operator()(const std::exception_ptr& e) const noexcept { result_->set_exception(e); }

    future_shared_state_ptr result_;
};

}  // namespace

countdown::countdown() : state_(new countdown_state()) {}

void countdown::add_ready(const future_shared_state* f) {
    as_countdown(state_)->increment();
    f->when_ready0(count_down_func{as_countdown(state_)});
}

void countdown::add_succeeded_or_failed(const future_shared_state* f) {
    as_countdown(state_)->increment();

    // when_succeeded_void ignores the value the future carries, which is exactly what a
    // combinator wants -- and what lets one non-template subscription serve every future<T>.
    f->when_succeeded_void(count_down_func{as_countdown(state_)});
    f->when_failed(fail_all_func{as_countdown(state_)});
}

future<void> countdown::get_future() {
    as_countdown(state_)->decrement();
    return future<void>(state_.get());
}

first_of::first_of() : state_(new future_shared_state()) {}

void first_of::add(const future_shared_state* f) {
    f->when_succeeded_void(first_of_func{state_});
    f->when_failed(first_of_func{state_});
}

future<void> first_of::get_future() const { return future<void>(state_.get()); }

}  // namespace future_detail

}  // namespace wxl::async
