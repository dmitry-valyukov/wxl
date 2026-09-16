module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

int thread::current_processor_number() noexcept {
    return static_cast<int>(::GetCurrentProcessorNumber());
}

void thread::sleep(size_t milliseconds) { ::Sleep(static_cast<DWORD>(milliseconds)); }

bool thread::yield() noexcept { return ::SwitchToThread() != 0; }

namespace {

[[noreturn]] void throw_last_error(const char* what) {
    throw std::system_error(
        std::error_code(static_cast<int>(::GetLastError()), std::system_category()), what);
}

}  // namespace

void thread::set_affinity(const cpu_indexes& indexes) {
    DWORD_PTR mask = 0;

    for (size_t index : indexes) {
        if (index >= sizeof(DWORD_PTR) * 8) throw std::invalid_argument("CPU index out of range");

        mask |= (DWORD_PTR(1) << index);
    }

    if (::SetThreadAffinityMask(::GetCurrentThread(), mask) == 0)
        throw_last_error("SetThreadAffinityMask failed");
}

void thread::set_affinity(size_t cpu_index) { set_affinity(cpu_indexes{cpu_index}); }

void thread::priority(int value) {
    if (::SetThreadPriority(::GetCurrentThread(), value) == 0)
        throw_last_error("SetThreadPriority failed");
}

int thread::priority() {
    const int result = ::GetThreadPriority(::GetCurrentThread());

    if (result == THREAD_PRIORITY_ERROR_RETURN) throw_last_error("GetThreadPriority failed");

    return result;
}

}  // namespace wxl::async
