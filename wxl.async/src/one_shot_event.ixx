module;

#include "platform.h"

export module wxl.async:one_shot_event;

import wxl.core;
import std;

export namespace wxl::async {

/// A notification that happens once and stays happened: signal() fires it, wait() blocks
/// until it has fired, and everyone arriving afterwards passes straight through. There is
/// no way back -- a second signal() does nothing, and nothing resets it.
///
/// The underlying OS waitable object is created only when a thread is about to block on a
/// notification that has not fired yet. A caller that never has to wait therefore never
/// pays for a kernel handle, which is the whole reason this type exists beside
/// core::hevent.
class one_shot_event : public core::noncopyable
{
public:
    explicit one_shot_event(bool initial_state = false) noexcept
        : state_(initial_state), event_obj_(nullptr) {}

    ~one_shot_event();

    /// Returns \c true if the event has been already signaled.
    ///
    /// Reads with acquire, not relaxed: a caller that sees \c true concludes the
    /// notification happened and goes on to read whatever the signalling thread published
    /// before it, so this load has to synchronize with signal()'s release. The same load
    /// is wait()'s fast path, which is where it matters most.
    bool signaled() const { return state_.value(std::memory_order_acquire); }

    /// Fires the notification. Safe to call from several threads at once; the first one
    /// through does the work and the rest return having done nothing.
    void signal() {
        if (state_.set()) {  // we are the first.
            set_event();
        }
    }

    /// Blocks until the notification has fired.
    ///
    /// \note Aborts the process if the underlying OS waitable object cannot be created.
    void wait() const {
        if (already_fired()) return;

        wait_event();
    }

    /// Blocks until the notification has fired or \p timeout elapses.
    ///
    /// \return \c false only on timeout.
    ///
    /// \note Aborts the process if the underlying OS waitable object cannot be created.
    bool wait_for(core::duration timeout) const {
        if (already_fired()) return true;

        return wait_event_for(timeout);
    }

private:
    /// The step both waits share: the fast path, and the lazy creation of the OS object the
    /// slow one is about to block on.
    ///
    /// \return \c true if the notification has fired, so there is nothing left to wait for.
    bool already_fired() const {
        if (signaled()) return true;

        return !event_obj_.load(std::memory_order_acquire) && initialize_event_handle();
    }

    /// Creates the OS object, unless a concurrent thread got there first.
    /// \return \c true if the event had already been signaled by then, so there is
    ///         nothing left to wait for.
    bool initialize_event_handle() const;

    core::atomic_trigger state_;

    using handle = HANDLE;

    mutable std::atomic<handle>
        event_obj_;  ///< the underlying OS waitable object, lazily created.

    static handle create_event();
    static void destroy_event(handle event_obj);

    /// Waits on the underlying OS waitable object. Two calls, not one taking "forever" as a
    /// value: the endless wait has no deadline to convert and no answer to give.
    ///@{
    void wait_event() const;
    bool wait_event_for(core::duration timeout) const;
    ///@}

    void set_event() const noexcept {
        if (handle event = event_obj_.load(std::memory_order_acquire)) set_event(event);
    }

    void set_event(handle event) const noexcept;
};

}  // export namespace wxl::async
