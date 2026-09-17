module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

namespace {
std::atomic<module_cleanup*> s_cleanup_stacks[3] = {};

std::atomic<module_cleanup*>& stack_by_priority(module_cleanup::priority p) {
    using enum module_cleanup::priority;

    if (p == cleanup_first) return s_cleanup_stacks[0];

    if (p == cleanup_normal) return s_cleanup_stacks[1];

    return s_cleanup_stacks[2];
}
}  // namespace

std::atomic<bool> module_cleanup::s_process_is_terminating{false};

void module_cleanup::init(cleanup_func* f, cleanup_func1* f1, void* arg, priority p) {
    assert(f || f1);
    cleanup_func_ = f;
    cleanup_func1_ = f1;
    arg_ = arg;

    if (s_process_is_terminating.load(std::memory_order_acquire)) {
        call_cleanup();
        return;
    }

    std::atomic<module_cleanup*>& cleanup_stack = stack_by_priority(p);
    module_cleanup* prev = cleanup_stack.load(std::memory_order_relaxed);

    while (true) {
        prev_.store(prev, std::memory_order_relaxed);

        if (cleanup_stack.compare_exchange_weak(prev, this, std::memory_order_release,
                                                std::memory_order_relaxed))
            return;

        if (s_process_is_terminating.load(std::memory_order_acquire)) {
            call_cleanup();
            return;
        }
    }
}

void module_cleanup::call_cleanup() const {
    try {
        if (cleanup_func_)
            (*cleanup_func_)();
        else
            cleanup_func1_(arg_);
    } catch (...) {
    }
}

void module_cleanup::execute_at_exit() {
    bool expected = false;

    if (!s_process_is_terminating.compare_exchange_strong(expected, true,
                                                          std::memory_order_acq_rel))
        return;

    for (auto& stack : s_cleanup_stacks) {
        module_cleanup* walk = stack.load(std::memory_order_acquire);

        while (walk) {
            walk->call_cleanup();
            walk = walk->prev_.load(std::memory_order_relaxed);
        }
    }
}

}  // namespace wxl::core
