module;

#include "platform.h"
#include "abi.h"

export module wxl.async:mpsc_channel;

import :mpsc_queue;
import wxl.core;
import std;

export namespace wxl::async {

/// Hands elements over from any number of writer threads to one <b>single</b> reader,
/// which may sleep while there is nothing to take. Ownership travels with the element:
/// send() empties the caller's smart pointer, receive() fills another one.
///
/// Sleeping is the whole reason to reach for a channel rather than the mpsc_queue it is
/// built on: the queue answers "nothing there" and leaves the reader to decide what to do
/// about it. The writers pay for the wakeup only when the reader is actually asleep --
/// see ../doc/arm-disarm.md for the exchange that lets them find that out
/// without a lock.
///
/// Waiting follows the condition-variable contract: receive() may return empty-handed
/// before its timeout elapses, so the caller loops on a predicate of its own rather than
/// on the channel's answer. That is also how a reader is told to stop -- signal(true)
/// wakes it once, and the reader re-checks whatever flag its owner set.
template <typename t_element, typename t_wait_object = core::hevent>
class mpsc_channel : public core::noncopyable
{
public:
    using element_type = t_element;
    using wait_object = t_wait_object;

    mpsc_channel()
        : wait_object_(false)  // auto-reset: each signal() wakes exactly one receive().
    {}

    ~mpsc_channel() { clear(); }

    /// The primitive the reader sleeps on, for an owner that has to wait on more than
    /// this channel at once: WaitForMultipleObjects and its like take handles, not
    /// channels. Non-const on purpose -- waiting is a mutating use of the primitive.
    ///
    /// Whoever waits on it directly takes a wakeup the reader was meant to get, so a
    /// caller doing this owns the whole waiting protocol from that point on: it is the
    /// one that has to call arm() before and disarm() after.
    wait_object* get_wait_object() { return &wait_object_; }

    /// The same primitive as a raw handle, for the group-wait call that wants one.
    HANDLE wait_handle() { return wait_object_.handle(); }

    /// Whether the reader would find anything to take.
    ///
    /// Reader-side only: this looks at the reader's own end of the queue, a plain
    /// non-atomic member that no writer may touch. Asking from a writer thread is a data
    /// race, and the answer would be stale by the time it arrived anyway.
    bool empty() const { return queue_.empty(); }

    /// Shows the next element without taking it. Reader-side only, like empty().
    [[nodiscard]] element_type* peek() noexcept { return queue_.peek(); }

    /// Takes the next element as a raw pointer, never waiting; the caller owns it from
    /// then on. try_receive() is the same thing with the ownership spelled out, and is
    /// what a caller should reach for unless it has its own way to hold the element.
    [[nodiscard]] element_type* pop_one() noexcept { return queue_.pop_one(); }

    /// Hands the element over to the reader and wakes it if it is waiting.
    ///
    /// The push is the strong one on purpose: signal() reads the wait trigger right after
    /// it, and the two have to be ordered or a writer can decide the reader is not waiting
    /// while the reader decides the channel is empty.
    template <typename t_smart_ptr>
    void send(t_smart_ptr& element_ptr) {
        queue_.push_one_ordered(element_ptr.get());
        element_ptr.release();
        signal();
    }

    /// Takes one element if there is one, never waiting.
    ///
    /// \param out_element Receives the element; left alone when none is taken.
    template <typename t_smart_ptr>
    bool try_receive(t_smart_ptr& out_element) {
        if (element_type* el = queue_.pop_one()) {
            out_element.reset(el);
            return true;
        }

        return false;
    }

    /// Takes one element, waiting for one to arrive.
    ///
    /// \param out_element Receives the element; left alone when none is taken.
    /// \return \c true if an element was taken. A \c false does not mean the channel is
    ///         empty: a signal left over from a previous call, or a signal() with no data
    ///         behind it, comes back here as an ordinary spurious wakeup, which is why the
    ///         caller loops on a predicate of its own rather than on this answer.
    template <typename t_smart_ptr>
    bool receive(t_smart_ptr& out_element) {
        return receive_waiting(out_element, [this] { wait_object_.wait(); });
    }

    /// Takes one element, waiting up to \p timeout for one to arrive.
    ///
    /// \param out_element Receives the element; left alone when none is taken.
    /// \param timeout How long to wait; a zero duration tries without blocking, which is
    ///        what try_receive() says more plainly.
    /// \return \c true if an element was taken. A \c false does not prove the timeout
    ///         elapsed -- see receive() for why.
    template <typename t_smart_ptr>
    bool receive_for(t_smart_ptr& out_element, core::duration timeout) {
        if (!timeout) return try_receive(out_element);  // asked not to wait at all

        return receive_waiting(out_element, [this, timeout] { wait_object_.wait_for(timeout); });
    }

    /// Drops whatever is still in the channel, destroying each element with `delete`.
    /// That is the only ownership policy the channel knows: an element that came from a
    /// pool has to be taken out and returned there before the channel goes.
    void clear() {
        std::unique_ptr<element_type> tmp;

        while (try_receive(tmp)) tmp.reset();
    }

    /// Wakes the reader, but only if it is actually waiting; `force` signals regardless.
    ///
    /// Must be called after the element is in the channel, never before: a writer that
    /// reads the trigger first and pushes afterwards can find the reader not waiting yet
    /// and leave it asleep on the element it pushes a moment later. The push must also be
    /// the seq_cst one -- reading the trigger here is the load half of that exchange, and
    /// send() supplies the store half.
    ///
    /// \see ../doc/arm-disarm.md
    void signal(bool force = false) {
        const bool reader_is_waiting = disarm();

        if (reader_is_waiting || force) wait_object_.set();
    }

    /// Arms the wake-up: lets the writers know that the reader is waiting for new data.
    ///
    /// \see ../doc/arm-disarm.md
    void arm() {
        // Nobody can have set the trigger before us: a channel has one reader, and this
        // is a step of its own receive().
        [[maybe_unused]] const bool trigger_was_clear = wait_trigger_.set();

        assert(trigger_was_clear && "mpsc_channel: more than one thread is receiving");
    }

    /// Disarms the wake-up.
    /// \return \c false if someone has disarmed it first -- that is, took the right to
    ///         wake the reader.
    ///
    /// \see ../doc/arm-disarm.md
    bool disarm() { return wait_trigger_.reset(); }

private:
    /// The body both waiting receives share; they differ only in the wait handed in, which
    /// is why there is no timeout here to ask questions about.
    template <typename t_smart_ptr, typename t_wait>
    bool receive_waiting(t_smart_ptr& out_element, t_wait wait) {
        if (try_receive(out_element)) return true;

        arm();

        // The reader's half of the store/load exchange with signal(): a writer pushing
        // right now must either see our trigger or be seen by the retry below. The queue
        // is read relaxed, so this side needs a fence to tie that read to the trigger --
        // the writer needs none, its seq_cst push and the trigger carry the order between
        // them. On the cold path anyway: we only get here having found the channel empty.
        std::atomic_thread_fence(std::memory_order_seq_cst);

        if (try_receive(out_element)) {
            disarm();
            return true;
        }

        wait();

        disarm();

        return try_receive(out_element);
    }

    mpsc_queue<element_type> queue_;

    /// The wait object comes from a template parameter, so neither its size nor its own
    /// alignment is known here; the alignas keeps it off the line queue_ ends on.
    alignas(std::hardware_destructive_interference_size) t_wait_object wait_object_;

    /// Set while the reader is waiting, so that a writer can skip the kernel when it is
    /// not. Aligned for the same reason, and away from wait_object_ besides.
    alignas(std::hardware_destructive_interference_size) core::atomic_trigger wait_trigger_;
};

}  // export namespace wxl::async
