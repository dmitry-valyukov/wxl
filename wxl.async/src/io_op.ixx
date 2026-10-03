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
///
/// The buffer may be of any size. A call to the system takes at most `call_size`, so a
/// larger buffer goes in a chain of calls, the next issued where the previous one
/// completed, and the coroutine sees one operation and one answer.
class io_op : public async_op_t<std::size_t>
{
public:
    enum class kind : std::uint8_t { read, write };

    /// The most one call to the system is given. Bigger buys nothing -- the storage
    /// stack splits a request anyway, and this much is already all copying -- while the
    /// kernel keeps the whole of a request's buffer pinned for its duration.
    static constexpr std::size_t call_size = 64 * 1024 * 1024;

    /// \param at where in the file; an overlapped handle keeps no position of its own.
    /// \param skips_port what io_port::attach() answered for this file.
    io_op(kind what, HANDLE file, void* data, std::size_t size, std::uint64_t at,
          bool skips_port) noexcept;

    /// Starts the operation on the calling thread.
    /// \return `true` if it is over, with its result or its failure in place; `false`
    ///         if the kernel has it, and it will arrive at the port.
    [[nodiscard]] bool start() noexcept;

    /// The worker's call for an operation that has arrived at the port: reads what the
    /// kernel left in it, and issues the next call of the chain if there is one.
    /// \return `true` if the operation is over and goes back; `false` if the kernel has
    ///         it again.
    [[nodiscard]] bool completed() noexcept;

    /// The operation a port entry belongs to.
    static io_op* from(OVERLAPPED* overlapped) noexcept;

#ifndef NDEBUG
    /// Debug tallies for the tests, which cannot otherwise tell where an operation ran
    /// or whether it took more than one call. Not in a Release build.
    struct tally
    {
        /// Calls issued beyond the first one of an operation.
        std::atomic<std::size_t> chained{0};

        /// Operations started on the worker rather than where they were made.
        std::atomic<std::size_t> started_on_worker{0};
    };

    static inline tally debug;
#endif

protected:
    /// Started on the worker, the operation may have been given up while it stood in
    /// the queue, by a thread that found nothing to cancel yet. So the flag is read
    /// again once the kernel has the operation: whichever of the two came second sees
    /// the other.
    bool execute() override;

    void on_cancel() noexcept override;

private:
    /// start() on the worker, with the second look at the flag.
    bool start_on_worker() noexcept;

    /// How much the next call asks for -- and, until advance() has moved on, how much the
    /// one in flight was asked for.
    inline DWORD asking() const noexcept { return static_cast<DWORD>(std::min(left_, call_size)); }

    /// Accounts for a completed call.
    /// \return `true` if another call is to be issued: the buffer is not full yet and
    ///         the call brought all it was asked for; less is the end of the file.
    bool advance(DWORD transferred) noexcept;

    /// Ends the operation: with what has been transferred, or with the error.
    void finish(DWORD error) noexcept;

    OVERLAPPED overlapped_{};
    HANDLE file_;

    /// Where the next call goes, and how much is left to ask for.
    std::byte* data_;
    std::size_t left_;

    /// Transferred by the calls that have completed.
    std::size_t done_ = 0;

    kind kind_;
    bool skips_port_;
};

}  // export namespace wxl::async
