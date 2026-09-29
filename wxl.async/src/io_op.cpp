module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

io_op::io_op(kind what, HANDLE file, void* data, std::size_t size, std::uint64_t at,
             bool skips_port) noexcept
    : file_(file),
      data_(data),
      size_(static_cast<DWORD>(size)),
      kind_(what),
      skips_port_(skips_port) {
    assert(size <= max_size);

    overlapped_.Offset = static_cast<DWORD>(at);
    overlapped_.OffsetHigh = static_cast<DWORD>(at >> 32);
}

bool io_op::start() noexcept {
    DWORD transferred = 0;

    const BOOL finished = kind_ == kind::read
                              ? ::ReadFile(file_, data_, size_, &transferred, &overlapped_)
                              : ::WriteFile(file_, data_, size_, &transferred, &overlapped_);

    if (finished) {
        // Without the skip a packet is on its way all the same, and the operation is
        // the port's until it arrives.
        if (!skips_port_) [[unlikely]]
            return false;

        finish(ERROR_SUCCESS, transferred);
        return true;
    }

    const DWORD error = ::GetLastError();

    if (error == ERROR_IO_PENDING) return false;

    // A call that failed queues nothing.
    finish(error, 0);
    return true;
}

void io_op::completed() noexcept {
    DWORD transferred = 0;

    if (::GetOverlappedResult(file_, &overlapped_, &transferred, FALSE))
        finish(ERROR_SUCCESS, transferred);
    else
        finish(::GetLastError(), transferred);
}

io_op* io_op::from(OVERLAPPED* overlapped) noexcept {
    return core::object_from_field(&io_op::overlapped_, overlapped);
}

bool io_op::execute() {
    if (start()) return true;

    if (canceled()) [[unlikely]]
        ::CancelIoEx(file_, &overlapped_);

    return false;
}

void io_op::on_cancel() noexcept { ::CancelIoEx(file_, &overlapped_); }

void io_op::finish(DWORD error, DWORD transferred) noexcept {
    if (kind_ == kind::read) {
        // The end of the file is an answer, not a failure: nothing was read.
        if (error == ERROR_SUCCESS || error == ERROR_HANDLE_EOF) [[likely]] {
            set_value(std::size_t{transferred});
            return;
        }

        set_error(std::make_exception_ptr(system_exception("ReadFile", static_cast<int>(error))));
        return;
    }

    if (error == ERROR_SUCCESS && transferred == size_) [[likely]] {
        set_value(std::size_t{transferred});
        return;
    }

    // A write can succeed and take less than it was given, which is what a full disk
    // looks like from here.
    set_error(std::make_exception_ptr(system_exception(
        "WriteFile", static_cast<int>(error == ERROR_SUCCESS ? ERROR_DISK_FULL : error))));
}

}  // namespace wxl::async
