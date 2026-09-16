export module wxl.core:atomic_trigger;

import std;

export namespace wxl::core {

/// A boolean flag whose set()/reset()/invert() each report whether the call actually
/// changed the value, backed by std::atomic<bool>.
///
/// The mutating operations default to seq_cst rather than acq_rel, because the flag is
/// typically half of a store/load exchange between two threads -- one side writes the
/// flag and reads a queue, the other writes the queue and reads the flag. acq_rel orders
/// neither side's store against its own later load, so such a pair needs the operations
/// themselves to be sequentially consistent (or a seq_cst fence on both sides, which
/// costs an extra instruction). On x86-64 and ARM64 the default costs nothing: exchange
/// lowers to `xchg` and to `swpal` respectively, the same instruction either way.
/// Every operation also takes an explicit order, for a caller that has established it
/// does not need that much.
class atomic_trigger
{
public:
    explicit atomic_trigger(bool initial_value = false) noexcept : value_(initial_value) {}

    atomic_trigger(const atomic_trigger&) = delete;
    atomic_trigger& operator=(const atomic_trigger&) = delete;

    /// \return `true` if the state was changed (was false, became true).
    bool set(std::memory_order order = std::memory_order_seq_cst) noexcept {
        return !value_.exchange(true, order);
    }

    /// Clears the trigger; see read() for the same operation named after its result.
    /// \return `true` if the state was changed (was true, became false).
    bool reset(std::memory_order order = std::memory_order_seq_cst) noexcept {
        return value_.exchange(false, order);
    }

    /// Inverts the state.
    /// \return the new value.
    bool invert(std::memory_order order = std::memory_order_seq_cst) noexcept {
        // atomic<bool> has no fetch_xor (that's only defined for atomic<Integral>), so
        // the flip is done via a compare_exchange retry loop instead.
        bool old_value = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(old_value, !old_value, order,
                                             std::memory_order_relaxed));
        return !old_value;
    }

    /// Consumes the trigger: returns the current value and clears it.
    /// Same operation as reset(), but named for the caller that wants the value --
    /// hence [[nodiscard]], while reset() may be called for the clearing alone.
    [[nodiscard]] bool read(std::memory_order order = std::memory_order_seq_cst) noexcept {
        return value_.exchange(false, order);
    }

    /// Returns the current value, without ordering anything by default: an observer
    /// that acts on the answer passes acquire (or stronger) instead.
    bool value(std::memory_order order = std::memory_order_relaxed) const noexcept {
        return value_.load(order);
    }

private:
    std::atomic<bool> value_;
};

}  // namespace wxl::core
