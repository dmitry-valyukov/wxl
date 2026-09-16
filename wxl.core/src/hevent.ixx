module;

#include "platform.h"

export module wxl.core:hevent;

import :noncopyable;
import :time;

export namespace wxl::core {

/// RAII wrapper over a Windows event object (CreateEvent/SetEvent/ResetEvent/WaitForSingleObject).
class hevent : public noncopyable
{
    HANDLE handle_;

public:
    explicit hevent(bool manual_reset, bool initial_state = false);
    ~hevent();

    /// Sets the event to the signaled state, releasing waiters (all of them, for a
    /// manual-reset event; exactly one, for an auto-reset event).
    void set() noexcept;

    /// Sets the event to the non-signaled state. Only meaningful for a manual-reset event.
    void reset() noexcept;

    /// Blocks until the event is signaled.
    void wait() const;

    /// Blocks until the event is signaled or \p timeout elapses.
    /// \return \c false only on timeout.
    bool wait_for(duration timeout) const;

    HANDLE handle() const noexcept { return handle_; }
};

}  // export namespace wxl::core
