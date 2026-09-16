module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

hevent::hevent(bool manual_reset, bool initial_state)
    : handle_(::CreateEventW(nullptr, manual_reset ? TRUE : FALSE, initial_state ? TRUE : FALSE,
                             nullptr)) {
    if (handle_ == nullptr) [[unlikely]]
        abort("Failed to create event");
}

hevent::~hevent() { ::CloseHandle(handle_); }

void hevent::set() noexcept { ::SetEvent(handle_); }

void hevent::reset() noexcept { ::ResetEvent(handle_); }

void hevent::wait() const { ::WaitForSingleObject(handle_, INFINITE); }

bool hevent::wait_for(duration timeout) const {
    return ::WaitForSingleObject(handle_, to_os_timeout_ms(timeout)) == WAIT_OBJECT_0;
}

}  // namespace wxl::core
