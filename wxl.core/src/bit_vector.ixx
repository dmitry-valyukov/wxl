export module wxl.core:bit_vector;

import :noncopyable;
import std;

export namespace wxl::core {

/// Thread-safe atomic bit-vector; T is an integer or bit-flag enum type.
template <typename T>
    requires std::integral<T> || std::is_enum_v<T>
class bit_vector : noncopyable
{
public:
    explicit bit_vector(T value = T()) noexcept : value_(value) {}

    /// \return true if the bits' state changed.
    bool set(T mask) noexcept {
        T old_value = value_.load(std::memory_order_relaxed);
        T new_value = static_cast<T>(old_value | mask);

        while (new_value != old_value &&
               !value_.compare_exchange_weak(old_value, new_value, std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
            new_value = static_cast<T>(old_value | mask);
        }

        return new_value != old_value;
    }

    /// \return true if the bits' state changed.
    bool reset(T mask) noexcept {
        T old_value = value_.load(std::memory_order_relaxed);
        T new_value = static_cast<T>(old_value & ~mask);

        while (new_value != old_value &&
               !value_.compare_exchange_weak(old_value, new_value, std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
            new_value = static_cast<T>(old_value & ~mask);
        }

        return new_value != old_value;
    }

    T value() const noexcept { return value_.load(); }

    /// Atomically resets the given bits and returns their value from before the reset.
    T read_and_reset(T mask) noexcept {
        T old_value = value_.load(std::memory_order_relaxed);
        T new_value = static_cast<T>(old_value & ~mask);

        while (new_value != old_value &&
               !value_.compare_exchange_weak(old_value, new_value, std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
            new_value = static_cast<T>(old_value & ~mask);
        }

        return static_cast<T>(old_value & mask);
    }

private:
    std::atomic<T> value_;
};

}  // export namespace wxl::core
