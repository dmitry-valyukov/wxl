module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

using namespace wxl::async;

one_shot_event::~one_shot_event() {
    if (handle event_obj = event_obj_.load(std::memory_order_relaxed)) destroy_event(event_obj);
}

one_shot_event::handle one_shot_event::create_event() {
    const handle h_event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (!h_event) [[unlikely]]
        core::abort("Failed to create one_shot_event's underlying Event object");

    return h_event;
}

void one_shot_event::destroy_event(handle event_obj) { ::CloseHandle(event_obj); }

void one_shot_event::set_event(handle event_obj) const noexcept { ::SetEvent(event_obj); }

void one_shot_event::wait_event() const {
    ::WaitForSingleObject(event_obj_.load(std::memory_order_acquire), INFINITE);
}

bool one_shot_event::wait_event_for(core::duration timeout) const {
    return ::WaitForSingleObject(event_obj_.load(std::memory_order_acquire),
                                 core::to_os_timeout_ms(timeout)) == WAIT_OBJECT_0;
}

bool one_shot_event::initialize_event_handle() const {
    handle event_obj = create_event();
    handle expected = nullptr;

    if (!event_obj_.compare_exchange_strong(expected, event_obj, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
        // somebody has created eventObj_ concurrently
        destroy_event(event_obj);
        return signaled();
    }

    if (!signaled()) return false;

    // somebody has signaled while we created event
    set_event(event_obj);
    return true;
}
