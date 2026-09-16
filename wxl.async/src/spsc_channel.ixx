module;

#include "abi.h"

export module wxl.async:spsc_channel;

import :spsc_queue;
import wxl.core;
import std;

export namespace wxl::async {

/// Hands elements from one writing thread to one reading thread, which may
/// sleep while there is nothing to take, and which can be closed.
///
/// It is `spsc_queue` with the two things that queue deliberately leaves out.
///
/// **Sleeping.** The queue answers "nothing there" and walks away, leaving the
/// reader to decide what to do about it, and it wakes nobody on purpose -- a
/// consumer with nothing to read has to be woken by something its owner owns.
/// Here that something is a member, and the writer touches it only when the
/// reader is actually asleep: the arm/disarm protocol, the same
/// one `mpsc_channel` uses, written up in
/// wxl.async/doc/arm-disarm.md.
///
/// The primitive is a template parameter because owners differ in what they
/// sleep on -- an event, a semaphore, something with a message queue behind it
/// -- and "wake up" means a different call for each. All that is asked of it is
/// `set()` and `wait()`, and its constructor arguments travel through this
/// class's own.
///
/// **Closing is the writer's last word.** There is one writer, so nobody can be
/// in the middle of a send when it closes: its own program order is the whole
/// guarantee, and close() is a flag stored after the last element. A reader
/// that finds the flag set drains what is left and leaves knowing it dropped
/// nothing -- everything sent before the close is published before it. A
/// turnstile around the send, the way a many-writer queue guards its close,
/// would defend against a race a single writer cannot have.
///
/// Ownership: the channel belongs to the writing side (the queue *is* the
/// writer), and the reading side builds a `reader` of its own -- one, and only
/// one.
template <class element_t, std::size_t block_size, class signal_t = core::hevent>
class spsc_channel : public spsc_queue<element_t, block_size>
{
    using base = spsc_queue<element_t, block_size>;

public:
    using signal_type = signal_t;

    /// The arguments go to the wake-up primitive: a message queue wants the
    /// thread to post to, a semaphore wants nothing at all.
    template <class... signal_args>
    explicit spsc_channel(signal_args&&... args) : signal_(std::forward<signal_args>(args)...) {}

    /// Puts an element in and wakes the reader if it is asleep. The writing
    /// thread's call.
    ///
    /// Sending after close() is an error of the program, not a condition to
    /// report: the writer that closed the channel is the one sending, and it
    /// knew.
    template <class... args_t>
    void send(args_t&&... args) {
        ensure(!closed_.load(std::memory_order_relaxed) && "spsc_channel: send after close");

        base::emplace(std::forward<args_t>(args)...);
        signal();
    }

    /// Wakes the reader, but only if it is really waiting; `force` wakes it
    /// regardless.
    ///
    /// Called strictly after the element is in the queue: a writer that reads
    /// the trigger first and pushes afterwards can find the reader not yet
    /// asleep and leave it sleeping on the element it pushes a moment later.
    ///
    /// The ordering rests on disarm() being a seq_cst exchange: it is
    /// acquire and release at once, so the release store with which emplace()
    /// publishes the element cannot move past it. The writer needs no fence of
    /// its own; the reader does, see reader::receive().
    //
    // TODO: Если выбирать самый строгий, но при этом сочный вариант для Хабра,
    // мне нравится связка arm_trigger() / disarm_trigger(). Взвели курок - выстрелили!
    void signal(bool force = false) {
        if (disarm() || force) signal_.set();
    }

    /// Closes the channel. The writing thread's call, after its last send:
    /// everything sent before it stays in the queue for the reader to finish,
    /// and the release store is what carries those elements across with it.
    ///
    /// It wakes nobody: a reader asleep at this moment is woken by
    /// signal(true), which the owner calls next, and comes back through
    /// receive() empty-handed to find the flag.
    void close() noexcept { closed_.store(true, std::memory_order_release); }

    /// \return `true` if the channel is closed -- that is, no further element
    ///         can appear. Cheap to ask: the word changes once, so the reader's
    ///         copy of the line stays valid until it does.
    bool closed() const noexcept { return closed_.load(std::memory_order_acquire); }

    /// The wake-up primitive itself, for the owner to set up.
    ///
    /// The one part of a channel that differs between owners -- an event, a
    /// semaphore, something with a message queue behind it -- and the one that
    /// may need saying more than its constructor could. Nobody but the owner has
    /// any business here: the reader waits through receive(), the writer signals
    /// through send().
    signal_t& wakeup() noexcept { return signal_; }

    /// Arms the wake-up: lets the writer know that the reader is going to sleep.
    void arm() {
        [[maybe_unused]] const bool trigger_was_clear = wait_trigger_.set();

        assert(trigger_was_clear && "spsc_channel: more than one thread is receiving");
    }

    /// Disarms the wake-up.
    /// \return `false` if somebody disarmed it first -- that is, took the right
    ///         to wake the reader.
    bool disarm() { return wait_trigger_.reset(); }

    /// The reading half: where it has read up to, and the ability to sleep
    /// while there is nothing there.
    class reader : public base::reader
    {
    public:
        explicit reader(spsc_channel& channel) : base::reader(channel), channel_(&channel) {}

        /// Takes an element, waiting for one to arrive.
        ///
        /// \return `true` if an element was taken. A `false` does not mean the
        ///         queue is empty: a signal left over from a previous call, or
        ///         a signal(force) with no data behind it, arrives here as an
        ///         ordinary spurious wakeup, which is why the caller loops on a
        ///         predicate of its own rather than on this answer.
        [[nodiscard]] bool receive(element_t& out) {
            if (this->read(out)) return true;

            channel_->arm();

            // The reader's half of the exchange with the writer's signal(): a
            // writer publishing right now must either see our trigger or be
            // seen by the check below. The queue is read with ordinary loads,
            // so only a fence ties them to the trigger. On the cold path
            // anyway: we only get here having found the queue empty.
            std::atomic_thread_fence(std::memory_order_seq_cst);

            if (this->read(out)) {
                channel_->disarm();
                return true;
            }

            channel_->signal_.wait();
            channel_->disarm();

            return this->read(out);
        }

    private:
        spsc_channel* const channel_;
    };

private:
    /// A line of its own: the queue is written by the writer, while this one is
    /// touched by both sides.
    alignas(std::hardware_destructive_interference_size) signal_t signal_;

    /// Set while the reader is waiting -- the whole point of the exercise.
    alignas(std::hardware_destructive_interference_size) core::atomic_trigger wait_trigger_;

    /// The writer's last word. Its own line, so that the reader's rare look at
    /// it never shares a line with anything the writer is changing.
    alignas(std::hardware_destructive_interference_size) std::atomic<bool> closed_{false};
};

}  // export namespace wxl::async
