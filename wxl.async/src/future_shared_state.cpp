module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

using namespace wxl::async;

void future_detail::throw_invalid_future() {
    throw std::logic_error("SharedStateHolder does not refer to a shared state.");
}

namespace {
std::exception_ptr g_future_disposed_exception =
    std::make_exception_ptr(std::future_error(std::future_errc::broken_promise));
}

namespace wxl::async {

const char* to_string(future_status status) {
    switch (status) {
        case future_status::ready:
            return "ready";

        case future_status::timeout:
            return "timeout";

        case future_status::deferred:
            return "deferred";

        default:
            return "INTERNAL ERROR: Unknown FutureStatus";
    }
}

std::ostream& operator<<(std::ostream& stream, future_status status) {
    stream << to_string(status);

    return stream;
}

}  // namespace wxl::async

future_detail::resolved_state<void> future_shared_state::void_resolved_state_;

future_shared_state_ptr future_shared_state::void_ready_future_(
    new future_shared_state(&void_resolved_state_));

future_shared_state::~future_shared_state() {
    state* const state = state_.load();

    if (state != &void_resolved_state_) destroy(state);
}

void future_shared_state::checked_decrement_promise_count() {
    if (decrement_promise_count() == 0) abandon();
}

void future_shared_state::abandon() {
    if (!ready()) set_exception(g_future_disposed_exception);
}

void* future_shared_state::allocator::alloc(size_t size) {
    if (void* data = buf_.alloc(size)) return data;

    if (void* data = std::malloc(size)) return data;

    throw std::bad_alloc();
}

void future_shared_state::allocator::free(void* mem) {
    if (buf_.contains(mem)) return;

    std::free(mem);
}
