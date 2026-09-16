module;

#include "abi.h"

export module wxl.async:spsc_queue_reference_implementation;

import wxl.core;
import std;

export namespace wxl::async {

/// `spsc_queue` with the block handover done the obvious way, kept as the thing to measure
/// the real one against. Same surface, same elements, same warmest-block-first choice: the
/// reader hands a drained block back and the writer works on the one freed last, parking
/// whatever came with it in a pool of its own.
///
/// The difference is the handover, and it is the whole reason both exist. Here the blocks
/// come back on an atomic top: the reader links its block in with a compare-exchange -- one
/// pusher and one taker still need it, since the writer can take the top from under the
/// reader between reading it and linking to it -- and the writer empties the top with one
/// unconditional exchange. Two locked instructions per block, against none at all in
/// `spsc_queue`, which reaches the same thing through a slot with one writer per word.
///
/// That is worth a benchmark rather than an argument: per element the difference is
/// 1/block_size of two locked instructions, which is everything at sixteen elements to a
/// block and nothing at two hundred and fifty-six. Both are kept so the comparison can be
/// re-run whenever either side changes -- and if this one ever wins outright, it is the
/// simpler of the two and should be the one that stays.
template <class element_t, size_t block_size>
class spsc_queue_reference_implementation : public core::noncopyable
{
    static_assert(block_size > 0, "a block has to hold at least one element");

    static constexpr size_t item_alignment =
        std::max(alignof(element_t), std::hardware_destructive_interference_size);

    /// One chunk of the queue: storage for `block_size` elements, how far into it the
    /// writer has got, and the link -- which here is a real list link, threading the block
    /// through the live chain first and through the free runs afterwards.
    struct alignas(std::hardware_destructive_interference_size) block {
        // A free block has no element count to keep, so the head of a parked run spends
        // that word on the link to the run parked before it. Switching back is
        // std::construct_at, never a store through index_ -- calling a member function on
        // the member that is not the active one would be undefined.
        union
        {
            std::atomic<size_t> index_{};
            block* next_run_;
        };

        // Threads the block through the live chain first, the free runs afterwards, and the
        // handover in between. Written by one side at a time and read by the other through
        // the release store that hands the block over, so no atomic of its own.
        block* next_{};

        element_t* item(size_t index) noexcept {
            return reinterpret_cast<element_t*>(storage_ + index * sizeof(element_t));
        }

        // The writer keeps storing into this array while the reader reads out of it, so the
        // alignment starts it on a line of its own rather than on the one carrying index_
        // and next_, which the reader is reading at the same time.
        alignas(item_alignment) std::byte storage_[block_size * sizeof(element_t)];
    };

public:
    using element_type = element_t;

    static constexpr size_t elements_per_block = block_size;

    spsc_queue_reference_implementation() : write_cursor_(new block), first_(write_cursor_) {}

    ~spsc_queue_reference_implementation();

    /// Builds one element in place, then publishes it. The producing thread's call.
    template <class... t_args>
        requires std::constructible_from<element_t, t_args...>
    void emplace(t_args&&... args) {
        block* const current = write_cursor_;
        std::construct_at(current->item(write_index_), std::forward<t_args>(args)...);

        if (++write_index_ == block_size) [[unlikely]] {
            block* const next = take_block();
            current->next_ = next;
            // The one release store publishes the elements and the link together, so a
            // reader that finds a full block always finds its successor as well.
            current->index_.store(block_size, std::memory_order_release);
            write_cursor_ = next;
            write_index_ = 0;
        } else {
            current->index_.store(write_index_, std::memory_order_release);
        }
    }

    void write(const element_t& element) { emplace(element); }

    void write(element_t&& element) { emplace(std::move(element)); }

    /// The consuming thread's half: where it has read up to, and how far the writer had
    /// published when it last looked.

    class reader;

private:
    /// The reader is asked first and unconditionally, even with blocks already parked: the
    /// head of what it hands back is the one it freed last, and no parked block can be
    /// warmer than that. Whatever came with it is parked whole.
    ///
    /// Nothing here walks a run, and the only lines touched besides the writer's own belong
    /// to the block being taken, which is about to be written to anyway. That is the rule
    /// the parking is built around: a block out of circulation is left completely alone
    /// until it is wanted again.
    block* take_block() {
        block* taken;

        // One unconditional exchange, with no look before it. `drain_stack` reads the
        // top first and only swaps when it is not null, which is right where a consumer
        // mostly finds the stack empty: a read needs the line in Shared, a swap has to own
        // it. Here the writer asks once per block and in the steady state the reader has
        // always left one, so that look would cost a second coherence transaction on a line
        // this is about to own anyway.
        if (block* const returned = returned_.exchange(nullptr, std::memory_order_acquire))
            [[likely]] {
            taken = returned;

            if (block* const rest = taken->next_) park(rest);
        } else if (free_run_) {
            taken = free_run_;
            free_run_ = taken->next_;
        } else if (free_runs_) {
            taken = free_runs_;
            free_runs_ = taken->next_run_;
            free_run_ = taken->next_;
        } else {
            ++allocated_blocks_;
            return new block;
        }

        taken->next_ = nullptr;
        // The block may have been the head of a parked run, in which case the word held a
        // link rather than a count and the count has to be built there afresh.
        std::construct_at(&taken->index_, size_t{0});
        return taken;
    }

    /// The reader's side of the handover: link the drained block in as the new top. One
    /// pusher and one taker still need the compare-exchange -- the writer can take the top
    /// from under the reader between reading it and linking to it -- and the release is what
    /// carries the element destructors to whoever picks the block up.
    void hand_back(block* drained) noexcept {
        block* top = returned_.load(std::memory_order_relaxed);

        do {
            drained->next_ = top;
        } while (!returned_.compare_exchange_weak(top, drained, std::memory_order_release,
                                                  std::memory_order_relaxed));
    }

    /// The newest run is the warmest, so it becomes the one being consumed, and whatever
    /// was left of the previous one goes onto the stack of runs -- linked through the head's
    /// own spare word, so the pool needs no storage of its own at all.
    void park(block* run) {
        if (free_run_) {
            block* const older = free_run_;
            older->next_run_ = free_runs_;
            free_runs_ = older;
        }

        free_run_ = run;
    }

    block* write_cursor_;  // the block being filled, and how far into it
    size_t write_index_{};
    block* free_run_{};           // the run being consumed, newest block first
    block* free_runs_{};          // runs parked behind it, linked through their heads' next_run_
    size_t allocated_blocks_{1};  // every block ever created, counted down on teardown
    block* first_;                // where a reader starts, and the whole chain if none ever does
    bool has_reader_{};

    // The whole handover, and the only line the two threads share: the blocks the reader
    // has drained, newest first. The reader has to build the chain with a CAS -- the writer
    // can take the top from under it between reading and linking -- while the writer takes
    // the lot in one exchange.
    alignas(std::hardware_destructive_interference_size) std::atomic<block*> returned_{};
};

template <class element_t, size_t block_size>
spsc_queue_reference_implementation<element_t, block_size>::~spsc_queue_reference_implementation() {
    // The free runs and the returned stack hold every block that is not on the live chain,
    // which the reader's own destructor has already handed back. A run head's spare word is
    // the link to the run parked before it, so it is read before that head is freed.
    for (block* run = free_run_; run || free_runs_;) {
        if (!run) {
            run = free_runs_;
            free_runs_ = run->next_run_;
        }

        block* const next = run->next_;
        delete run;
        --allocated_blocks_;
        run = next;
    }

    block* chain = returned_.exchange(nullptr, std::memory_order_relaxed);

    // A queue that never had a reader still holds unread elements, and with nothing ever
    // handed back the live chain is still whole from the block it started on.
    if (!has_reader_) {
        for (block* live = first_; live;) {
            block* const next = live->next_;
            std::destroy_n(live->item(0), live->index_.load(std::memory_order_relaxed));
            live->next_ = chain;
            chain = live;
            live = next;
        }
    }

    while (chain) {
        block* const next = chain->next_;
        delete chain;
        chain = next;
        --allocated_blocks_;
    }

    assert(allocated_blocks_ == 0 &&
           "a reader outlived the spsc_queue_reference_implementation it was on");
}

template <class element_t, size_t block_size>
class spsc_queue_reference_implementation<element_t, block_size>::reader : public core::noncopyable
{
public:
    explicit reader(spsc_queue_reference_implementation& queue) noexcept
        : read_cursor_(queue.first_), queue_(&queue) {
        assert(!queue.has_reader_ &&
               "an spsc_queue_reference_implementation takes a single reader");
        queue.has_reader_ = true;
    }

    /// Destroys whatever was left unread and hands those blocks back the ordinary way, so
    /// the queue finds every block it ever made in its two free lists. With the writing
    /// thread already done, the chain from the cursor to the tail is the reader's alone.
    ~reader() {
        for (block* current = read_cursor_; current;) {
            const size_t write_index = current->index_.load(std::memory_order_acquire);
            std::destroy_n(current->item(read_index_), write_index - read_index_);
            read_index_ = 0;

            block* const next = current->next_;
            queue_->hand_back(current);
            current = next;
        }
    }

    /// Moves the next element into `out`.
    ///
    /// @return `false` if the queue was empty as of this call -- which is an answer
    ///         about that moment only, since the writer may be publishing as it
    ///         returns. A caller that has to wait waits on its own event, not by
    ///         asking again.
    [[nodiscard]] bool read(element_t& out) {
        if (read_index_ == write_index_ && !advance()) return false;

        element_t* const slot = read_cursor_->item(read_index_++);
        out = std::move(*slot);
        std::destroy_at(slot);
        return true;
    }

private:
    /// Re-reads what the writer has published, stepping to the next block once this one is
    /// drained. The push is what frees the old block: its release carries the element
    /// destructors, so the writer that takes the block finds storage nobody is using.
    /// @return `false` when there is nothing to read.
    bool advance() {
        write_index_ = read_cursor_->index_.load(std::memory_order_acquire);

        if (read_index_ != write_index_) return true;
        if (write_index_ != block_size) return false;  // the writer is still filling it

        block* const next = read_cursor_->next_;  // linked before the index reached block_size
        assume(next != nullptr);

        queue_->hand_back(read_cursor_);
        read_cursor_ = next;
        read_index_ = 0;

        write_index_ = next->index_.load(std::memory_order_acquire);
        return write_index_ != 0;
    }

    block* read_cursor_;
    size_t read_index_{};
    size_t write_index_{};  // the writer's position in read_cursor_, as of the last look
    spsc_queue_reference_implementation* queue_;
};

}  // namespace wxl::async
