module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

io_op::io_op(kind what, HANDLE file, void* data, std::size_t size, std::uint64_t at,
             bool skips_port) noexcept
    : file_(file),
      data_(static_cast<std::byte*>(data)),
      left_(size),
      kind_(what),
      skips_port_(skips_port) {
    overlapped_.Offset = static_cast<DWORD>(at);
    overlapped_.OffsetHigh = static_cast<DWORD>(at >> 32);
}

bool io_op::start() noexcept {
    for (;;) {
        const DWORD asked = asking();

        DWORD transferred = 0;

        const BOOL finished = kind_ == kind::read
                                  ? ::ReadFile(file_, data_, asked, &transferred, &overlapped_)
                                  : ::WriteFile(file_, data_, asked, &transferred, &overlapped_);

        if (!finished) {
            const DWORD error = ::GetLastError();

            if (error == ERROR_IO_PENDING) return false;

            // A call that failed queues nothing.
            finish(error);
            return true;
        }

        // Without the skip a packet is on its way all the same, and the operation is
        // the port's until it arrives.
        if (!skips_port_) [[unlikely]]
            return false;

        if (!advance(transferred)) return true;

        // Between two calls of a chain, on the worker: an operation given up meanwhile is
        // not carried on with.
        if (canceled()) [[unlikely]] {
            finish(ERROR_OPERATION_ABORTED);
            return true;
        }

#ifndef NDEBUG
        debug.chained.fetch_add(1, std::memory_order_relaxed);
#endif
    }
}

bool io_op::completed() noexcept {
    DWORD transferred = 0;

    if (!::GetOverlappedResult(file_, &overlapped_, &transferred, FALSE)) {
        finish(::GetLastError());
        return true;
    }

    if (!advance(transferred)) return true;

    if (canceled()) [[unlikely]] {
        finish(ERROR_OPERATION_ABORTED);
        return true;
    }

#ifndef NDEBUG
    debug.chained.fetch_add(1, std::memory_order_relaxed);
#endif

    return start_on_worker();
}

io_op* io_op::from(OVERLAPPED* overlapped) noexcept {
    return core::object_from_field(&io_op::overlapped_, overlapped);
}

bool io_op::execute() {
#ifndef NDEBUG
    debug.started_on_worker.fetch_add(1, std::memory_order_relaxed);
#endif

    return start_on_worker();
}

bool io_op::start_on_worker() noexcept {
    if (start()) return true;

    if (canceled()) [[unlikely]]
        ::CancelIoEx(file_, &overlapped_);

    return false;
}

void io_op::on_cancel() noexcept { ::CancelIoEx(file_, &overlapped_); }

bool io_op::advance(DWORD transferred) noexcept {
    const bool whole = transferred == asking();

    done_ += transferred;
    left_ -= transferred;
    data_ += transferred;

    const std::uint64_t at = (std::uint64_t{overlapped_.OffsetHigh} << 32 | overlapped_.Offset) +
                             transferred;

    overlapped_.Offset = static_cast<DWORD>(at);
    overlapped_.OffsetHigh = static_cast<DWORD>(at >> 32);

    if (whole && left_ != 0) [[unlikely]]
        return true;

    finish(ERROR_SUCCESS);
    return false;
}

void io_op::finish(DWORD error) noexcept {
    if (kind_ == kind::read) {
        // The end of the file is an answer, not a failure: what was read before it.
        if (error == ERROR_SUCCESS || error == ERROR_HANDLE_EOF) [[likely]] {
            set_value(std::size_t{done_});
            return;
        }

        set_error(std::make_exception_ptr(system_exception("ReadFile", static_cast<int>(error))));
        return;
    }

    if (error == ERROR_SUCCESS && left_ == 0) [[likely]] {
        set_value(std::size_t{done_});
        return;
    }

    // A write can succeed and take less than it was given, which is what a full disk
    // looks like from here.
    set_error(std::make_exception_ptr(system_exception(
        "WriteFile", static_cast<int>(error == ERROR_SUCCESS ? ERROR_DISK_FULL : error))));
}

// At the edge of the pool's 128-byte class: one more field, and every operation costs a
// 256-byte block and a second cache line.
static_assert(sizeof(io_op) <= 128);

}  // namespace wxl::async
