export module wxl.async:bump_buffer;

import wxl.core;
import std;

export namespace wxl::async {

/// A fixed-size byte buffer with an atomic cursor that only ever moves forward: alloc()
/// bumps it by CAS and returns the word-aligned range it just claimed. Blocks therefore
/// never overlap, and none of them is ever reclaimed on its own -- the whole buffer goes
/// when the object does.
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
    bump_buffer() noexcept : cursor_(buffer_) {}

    /// Returns a word-aligned block of at least `size` bytes, or nullptr if the buffer is
    /// exhausted.
    void* alloc(size_t size) noexcept {
        size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);
        char* cr = cursor_.load();

        do {
            if (cr + size > end()) return nullptr;
        } while (!cursor_.compare_exchange_weak(cr, cr + size));

        return cr;
    }

    /// Whether `addr` points into this buffer.
    bool contains(const void* addr) const noexcept {
        return addr >= begin() && addr < end();
    }

private:
    const char* begin() const noexcept { return buffer_; }
    const char* end() const noexcept { return begin() + capacity_; }

    char buffer_[capacity_];

    // Every alloc() call, from every thread, touches this; alignas keeps it
    // off whichever cache line the most recently allocated block landed on.
    alignas(std::hardware_destructive_interference_size) std::atomic<char*> cursor_;
};

}  // export namespace wxl::async
