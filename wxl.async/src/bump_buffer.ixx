export module wxl.async:bump_buffer;

import wxl.core;
import std;

export namespace wxl::async {

/// A fixed-size byte buffer with an atomic cursor: alloc() claims a word-aligned range by
/// a single fetch_add, with no retry loop, and the cursor never moves back below a block
/// it has handed out. Blocks therefore never overlap, and none of them is ever reclaimed
/// on its own -- the whole buffer goes when the object does.
///
/// Running out is an answer, not a failure: alloc() returns nullptr, and the caller falls
/// back to a general-purpose allocator (future_shared_state::allocator shows the pattern,
/// and uses contains() to tell blocks of one kind from the other when freeing). Any thread
/// may call either method at any time; there is no usage discipline to keep.
template <size_t buffer_size>
class bump_buffer : public core::noncopyable
{
    static constexpr size_t capacity_ = (buffer_size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

public:
    /// Returns a word-aligned block of at least `size` bytes, or nullptr if that does not
    /// fit in what is left of the buffer.
    ///
    /// Every access to the cursor is relaxed: the cursor publishes nothing. A block is
    /// memory nobody has written before, since nothing is ever reclaimed, and whoever
    /// hands its contents to another thread orders that handoff itself.
    void* alloc(size_t size) noexcept {
        // Checked before rounding up: a size within a word of SIZE_MAX would round to 0.
        if (size > capacity_) return nullptr;
        size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

        // A request that visibly does not fit leaves the cursor alone: the tail stays for
        // smaller requests, and an exhausted buffer is only ever read.
        if (cursor_.load(std::memory_order_relaxed) > capacity_ - size) return nullptr;

        const size_t offset = cursor_.fetch_add(size, std::memory_order_relaxed);
        if (offset <= capacity_ - size) return buffer_ + offset;

        // Lost the race at the edge. Only a claim nobody has bumped past may be undone.
        // Subtracting from whatever the cursor holds now would let another loser's undo
        // bring it back inside the buffer while our bytes are still counted, a block be
        // granted above them, and our undo then pull the cursor below that block's end,
        // so the next alloc() would hand the same block out again. If somebody did bump
        // past us, the cursor stays beyond the end and the tail is given up.
        size_t claimed_end = offset + size;
        cursor_.compare_exchange_strong(claimed_end, offset, std::memory_order_relaxed);
        return nullptr;
    }

    /// Whether `addr` points into this buffer.
    bool contains(const void* addr) const noexcept {
        return addr >= begin() && addr < end();
    }

private:
    const char* begin() const noexcept { return buffer_; }
    const char* end() const noexcept { return begin() + capacity_; }

    char buffer_[capacity_];

    // An offset rather than a pointer: a lost race leaves it beyond the end, where pointer
    // arithmetic and comparison with end() would be undefined.
    //
    // Every alloc() call, from every thread, touches this; alignas keeps it
    // off whichever cache line the most recently allocated block landed on.
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> cursor_{0};
};

}  // export namespace wxl::async
