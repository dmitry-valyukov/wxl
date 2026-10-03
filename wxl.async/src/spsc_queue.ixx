module;

#include "abi.h"

export module wxl.async:spsc_queue;

import wxl.core;
import std;

export namespace wxl::async {

/// Unbounded queue for exactly one writing and one reading thread, made of blocks of
/// `block_size` elements. Neither side ever waits, nothing here is a read-modify-write, and
/// what a burst makes the queue grow stops being touched once the burst is over.
///
/// **The elements: one release store each.** The writer builds an element in the slot the
/// reader will take it from and publishes its own position in the block; the reader picks
/// that position up with one acquire load per block and then walks the elements with
/// ordinary reads. The block's link to its successor rides that same store, so it needs no
/// atomic of its own -- and neither do the elements, which are plain memory travelling on
/// exactly the same edge.
///
/// **The blocks: the warmest one, handed over with one writer per word.** A block the
/// reader has drained is the warmest there is, so that is the one the writer takes next.
/// The reader chains what it drains, newest first, and hands the chain across through a
/// single slot guarded by two counters -- it bumps its own after filling the slot, the
/// writer bumps its own after emptying it, and neither ever writes the other's word. So the
/// whole protocol is release stores and acquire loads, plain moves on x86-64. A reader whose
/// chain has not been collected yet simply keeps adding to it; nobody waits for anybody.
///
/// The writer works on the head of the chain it is given and parks the rest in a pool of its
/// own, which it does not look at again while the reader keeps handing blocks back. That is
/// what makes the set of blocks in circulation follow the load down as well as up: a queue
/// that grew to a hundred blocks under a backlog settles back onto the two that stay warm,
/// and the rest are never visited, never evicting anything from under anyone.
///
/// The queue **is** the writer: `write()` and `emplace()` belong to the producing thread and
/// the cursor they advance is a field of the queue itself, so a producer that keeps the
/// queue by value on its stack reaches its own state without a hop. The consumer's state is
/// the same kind of object one level down -- a `reader` built from the queue, by value on the
/// consuming thread's stack. There is one of each, and the second `reader` is an assert.
///
/// Teardown happens with the traffic stopped: the reader is destroyed once the writing
/// thread is done with the queue, and the queue after the reader.
///
/// There is no signalling here on purpose. A consumer with nothing to read has to be woken
/// by something the caller owns -- an event next to the queue, the way `mpsc_channel` does
/// it -- never by re-reading the queue on a timer.
template <class element_t, size_t block_size>
class spsc_queue : public core::noncopyable
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

    spsc_queue() : write_cursor_(new block), first_(write_cursor_) {}

    ~spsc_queue();

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
    /// The chain the reader has handed over, if it has handed one over since the last look:
    /// its head is the block freed last and no parked block can be warmer, so that is the
    /// one taken and the rest are parked whole.
    ///
    /// The look costs an acquire load of a counter the reader writes -- one shared line
    /// read once per block, and no read-modify-write anywhere in it. Emptying the slot is
    /// published back the same way, with a release store of the writer's own counter, which
    /// is what lets the reader know it may fill the slot again.
    block* take_block() {
        block* taken;

        if (handed_.load(std::memory_order_acquire) != taken_batches_) [[likely]] {
            taken = handover_;
            taken_.store(++taken_batches_, std::memory_order_release);

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
    size_t taken_batches_{};      // chains taken from the slot, the writer's own count
    size_t allocated_blocks_{1};  // every block ever created, counted down on teardown
    block* first_;                // where a reader starts, and the whole chain if none ever does
    block* orphaned_{};           // what the reader was holding when it went
    bool has_reader_{};

    // The reader's half of the handover: the chain it has left, newest block first, and the
    // count it bumps once the chain is in place. One writer each, so a release store and an
    // acquire load carry it -- no read-modify-write on either side.
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> handed_{};
    block* handover_{};

    // The writer's half: the count of chains it has taken, which is how the reader knows
    // the slot is free again. Its own line, since the reader reads it.
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> taken_{};
};

template <class element_t, size_t block_size>
spsc_queue<element_t, block_size>::~spsc_queue() {
    // Everything that is not on the live chain: what the reader was still holding when it
    // went, a chain left in the slot that the writer never came back for, and the run the
    // writer was working through.
    const bool slot_filled =
        handed_.load(std::memory_order_relaxed) != taken_.load(std::memory_order_relaxed);

    for (block* chain : {orphaned_, slot_filled ? handover_ : nullptr, free_run_}) {
        while (chain) {
            block* const next = chain->next_;
            delete chain;
            --allocated_blocks_;
            chain = next;
        }
    }

    // The parked runs, whose heads carry the link to the run parked before them in the word
    // a free block has no count to keep in.
    while (free_runs_) {
        block* run = free_runs_;
        free_runs_ = run->next_run_;

        while (run) {
            block* const next = run->next_;
            delete run;
            --allocated_blocks_;
            run = next;
        }
    }

    // A queue that never had a reader still holds unread elements, and with nothing ever
    // handed back the live chain is still whole from the block it started on.
    if (!has_reader_) {
        for (block* live = first_; live;) {
            block* const next = live->next_;
            std::destroy_n(live->item(0), live->index_.load(std::memory_order_relaxed));
            delete live;
            --allocated_blocks_;
            live = next;
        }
    }

    assert(allocated_blocks_ == 0 && "a reader outlived the queue it was attached to");
}

template <class element_t, size_t block_size>
class spsc_queue<element_t, block_size>::reader : public core::noncopyable
{
public:
    explicit reader(spsc_queue& queue) noexcept : read_cursor_(queue.first_), queue_(&queue) {
        assert(!queue.has_reader_ && "this queue takes a single reader");
        queue.has_reader_ = true;
    }

    /// Destroys whatever was left unread and leaves the blocks where the queue will find
    /// them. With the writing thread already done there is nobody to hand them to, so they
    /// go into the queue's own pointer rather than through the slot.
    ~reader() {
        for (block* current = read_cursor_; current;) {
            const size_t write_index = current->index_.load(std::memory_order_acquire);
            std::destroy_n(current->item(read_index_), write_index - read_index_);
            read_index_ = 0;

            block* const next = current->next_;
            current->next_ = freed_chain_;
            freed_chain_ = current;
            current = next;
        }

        queue_->orphaned_ = freed_chain_;
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

    /// A place among the elements published and not read yet, for a reader that has to
    /// look further than the next one without taking anything.
    ///
    /// Valid while the reading thread reads nothing past it: the blocks it stands in are
    /// the reader's until reading moves beyond them and hands them back to the writer.
    class lookahead
    {
    public:
        lookahead() noexcept = default;

        bool operator==(const lookahead&) const noexcept = default;

    private:
        friend class reader;

        lookahead(block* at, size_t index) noexcept : block_(at), index_(index) {}

        block* block_{};
        size_t index_{};
    };

    /// Where the next read() would start.
    lookahead look_ahead() const noexcept { return {read_cursor_, read_index_}; }

    /// Steps `at` over the next element the writer has published.
    ///
    /// The same acquire load of a block's count that read() makes, and the link to the next
    /// block is followed only once the count says the writer has filled this one -- the
    /// release store that published the count published the link with it.
    ///
    /// @return the element stepped over, left in place; `nullptr` at the end of what has
    ///         been published, with `at` staying there to carry on from later.
    element_t* peek(lookahead& at) const noexcept {
        size_t published = at.block_->index_.load(std::memory_order_acquire);

        if (at.index_ == published) {
            if (published != block_size) return nullptr;

            at.block_ = at.block_->next_;
            at.index_ = 0;

            published = at.block_->index_.load(std::memory_order_acquire);

            if (published == 0) return nullptr;
        }

        return at.block_->item(at.index_++);
    }

private:
    /// Re-reads what the writer has published, stepping to the next block once this one is
    /// drained. Handing the old block back is what frees it: the release store that
    /// publishes the chain carries the element destructors with it, so the writer finds
    /// storage nobody is using. @return `false` when there is nothing to read.
    bool advance() {
        write_index_ = read_cursor_->index_.load(std::memory_order_acquire);

        if (read_index_ != write_index_) return true;
        if (write_index_ != block_size) return false;  // the writer is still filling it

        block* const next = read_cursor_->next_;  // linked before the index reached block_size
        assume(next != nullptr);

        hand_back(read_cursor_);
        read_cursor_ = next;
        read_index_ = 0;

        write_index_ = next->index_.load(std::memory_order_acquire);
        return write_index_ != 0;
    }

    /// Chains the drained block onto what has not been handed over yet, newest first, and
    /// hands the whole chain across as soon as the writer has taken the previous one. The
    /// slot has one writer and each counter has one writer, so there is no
    /// read-modify-write in any of it; and a writer that has not come back for the last
    /// chain simply gets a longer one when it does.
    void hand_back(block* drained) noexcept {
        drained->next_ = freed_chain_;
        freed_chain_ = drained;

        if (queue_->taken_.load(std::memory_order_acquire) == handed_batches_) {
            queue_->handover_ = freed_chain_;
            freed_chain_ = nullptr;
            queue_->handed_.store(++handed_batches_, std::memory_order_release);
        }
    }

    block* read_cursor_;
    size_t read_index_{};
    size_t write_index_{};  // the writer's position in read_cursor_, as of the last look
    block* freed_chain_{};  // drained and not handed over yet, newest first
    size_t handed_batches_{};
    spsc_queue* queue_;
};

}  // namespace wxl::async
