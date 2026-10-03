module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

io_port::io_port() : port_(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)) {
    if (!port_) throw system_exception("CreateIoCompletionPort");
}

io_port::~io_port() { ::CloseHandle(port_); }

void io_port::set() noexcept {
    [[maybe_unused]] const BOOL posted = ::PostQueuedCompletionStatus(port_, 0, 0, nullptr);

    assert(posted && "io_port: the wake-up could not be posted");
}

bool io_port::attach(HANDLE file) {
    if (!::CreateIoCompletionPort(file, port_, 0, 0)) throw system_exception("CreateIoCompletionPort");

    return ::SetFileCompletionNotificationModes(
               file, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE) != 0;
}

std::size_t io_port::take(std::span<OVERLAPPED_ENTRY> into, bool wait) noexcept {
    ULONG taken = 0;

    if (!::GetQueuedCompletionStatusEx(port_, into.data(), static_cast<ULONG>(into.size()), &taken,
                                       wait ? INFINITE : 0, FALSE))
        return 0;

    return taken;
}

}  // namespace wxl::async
