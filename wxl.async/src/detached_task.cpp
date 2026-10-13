module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

bool detached_task::promise_type::leave_at_end() noexcept {
    scenario_ = false;

    const scenario_owner& owner = held();
    return --owner.running_ != 0 || owner.wait_ == nullptr;
}

std::coroutine_handle<> detached_task::promise_type::hand_over(
    std::coroutine_handle<> self) noexcept {
    return std::exchange(held().wait_, nullptr)->take_over(self);
}

// Such a frame has no point after this to hand the thread over from, so a waiter it was
// the last for is resumed here, once the reference is let go of.
void detached_task::promise_type::leave_destroyed() noexcept {
    const scenario_owner& owner = held();

    std::coroutine_handle<> waiter;
    if (--owner.running_ == 0 && owner.wait_)
        waiter = std::exchange(owner.wait_, nullptr)->take_over({});

    self_ = nullptr;
    if (waiter) waiter.resume();
}

}  // namespace wxl::async
