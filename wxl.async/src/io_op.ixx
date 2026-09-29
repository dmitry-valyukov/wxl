module;

#include "platform.h"
#include "abi.h"

export module wxl.async:io_op;

import :async_op;
import wxl.core;
import std;

export namespace wxl::async {

/// One overlapped read or write on a file attached to the loop's port.
///
/// It can be started on either thread. The STA thread starts a read itself: one the
/// system finishes inside the call never leaves that thread, and the coroutine awaiting
/// it does not suspend. What is known to hold the calling thread -- a write, a large
/// buffer, a compressed or encrypted file -- travels to the worker instead and is started
/// there. Either way an operation the kernel has taken comes back through the port, on
/// the worker's thread, and from there through the return channel like any other.
class io_op : public async_op_t<std::size_t>
{
public:
    enum class kind : std::uint8_t { read, write };

    /// The most one operation carries: what a DWORD holds, rounded down.
    static constexpr std::size_t max_size = 0x4000'0000;

    /// \param at where in the file; an overlapped handle keeps no position of its own.
    /// \param skips_port what io_port::attach() answered for this file.
    io_op(kind what, HANDLE file, void* data, std::size_t size, std::uint64_t at,
          bool skips_port) noexcept;

    /// Starts the operation on the calling thread.
    /// \return `true` if it is over, with its result or its failure in place; `false`
    ///         if the kernel has it, and it will arrive at the port.
    bool start() noexcept;

    /// The worker's call for an operation that has arrived at the port: reads what the
    /// kernel left in it.
    void completed() noexcept;

    /// The operation a port entry belongs to.
    static io_op* from(OVERLAPPED* overlapped) noexcept;

protected:
    /// Started on the worker, the operation may have been given up while it stood in
    /// the queue, by a thread that found nothing to cancel yet. So the flag is read
    /// again once the kernel has the operation: whichever of the two came second sees
    /// the other.
    bool execute() override;

    void on_cancel() noexcept override;

private:
    void finish(DWORD error, DWORD transferred) noexcept;

    OVERLAPPED overlapped_{};
    HANDLE file_;
    void* data_;
    DWORD size_;
    kind kind_;
    bool skips_port_;
};

}  // export namespace wxl::async
