export module wxl.async:turnstile;

import wxl.core;
import std;

export namespace wxl::async {

/// Counts who is inside and can be locked shut, but only when the last one is out: any
/// number of threads pass through with try_enter()/exit(), and try_close() succeeds only
/// on an empty turnstile, after which every try_enter() fails forever. A successful
/// try_close() is therefore the owner's proof that everyone who went in has come out and
/// nobody else will get in -- which is what makes it safe to tear the resource down.
///
/// Entering and leaving need not sit in one scope. `task_queue` -- which now waits in
/// .claude/wxl.components, and is still the clearest example of it -- takes a place at
/// enqueue() and gives it up when the task has run, so the count is the work still owed
/// and closing means drained rather than merely empty. The rule that mode carries:
/// an entry that never leads to an exit is a person who never came out, and the turnstile
/// can then never be closed.
class turnstile : core::noncopyable
{
    static constexpr ssize_t closed_sentinel = std::numeric_limits<ssize_t>::min();

public:
    turnstile() noexcept = default;

    /// Tries to lock the turnstile shut.
    /// \return true if it is now (or already was) closed.
    bool try_close() noexcept {
        ssize_t expected = 0;
        return closed() || inside_.compare_exchange_strong(expected, closed_sentinel);
    }

    bool closed() const noexcept { return inside_.load() < 0; }

    /// \note Do NOT call exit() if this returns false.
    /// \return true if the turnstile let you through.
    bool try_enter() noexcept {
        if (inside_.fetch_add(1) + 1 <= 0) {
            inside_.fetch_sub(1);  // keep inside_ near closed_sentinel
            return false;
        }

        return true;
    }

    /// Call only after a successful try_enter().
    void exit() noexcept { inside_.fetch_sub(1); }

private:
    std::atomic<ssize_t> inside_{0};
};

/// RAII pass through a turnstile: enters on construction, leaves on destruction. For the
/// scoped use only -- a caller whose exit belongs to another call, the way task_queue's
/// does, works the turnstile by hand.
class turnstile_guard : core::noncopyable
{
public:
    explicit turnstile_guard(turnstile& gate) noexcept
        : gate_(gate.try_enter() ? &gate : nullptr) {}

    ~turnstile_guard() {
        if (gate_) gate_->exit();
    }

    /// \return true if the turnstile let this guard through; false if it was closed.
    bool entered() const noexcept { return gate_ != nullptr; }

private:
    turnstile* gate_;
};

}  // export namespace wxl::async
