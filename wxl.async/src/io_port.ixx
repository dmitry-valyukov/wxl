module;

#include "platform.h"
#include "abi.h"

export module wxl.async:io_port;

import wxl.core;
import std;

export namespace wxl::async {

/// The completion port the loop's worker sleeps on. Two things arrive here: overlapped
/// operations the kernel has finished, and the wake-up of the queue the worker reads.
///
/// It is that queue's signal as well -- `set()` is all a channel asks of one -- so the
/// worker has one place to sleep, and the channel's arm/disarm protocol decides, as it
/// does for an event, that a wake-up is posted only to a worker that is really asleep.
class io_port : public core::noncopyable
{
public:
    /// \throw system_exception if the system has no port to give.
    io_port();
    ~io_port();

    /// The channel's wake-up: a packet with no operation behind it.
    void set() noexcept;

    /// Has the file's overlapped operations finish here.
    ///
    /// \return whether an operation that finishes inside the call that started it stays
    ///         out of the port. Where the system refuses that, every operation comes
    ///         through the port, finished at once or not, and whoever starts one has to
    ///         know which it is.
    /// \throw system_exception if the file could not be attached.
    [[nodiscard]] bool attach(HANDLE file);

    /// Takes what has arrived, a batch at a time.
    ///
    /// \param wait whether to sleep until something arrives; without it the answer is
    ///        what is there now.
    /// \return how many entries were filled.
    std::size_t take(std::span<OVERLAPPED_ENTRY> into, bool wait) noexcept;

private:
    HANDLE port_;
};

}  // export namespace wxl::async
