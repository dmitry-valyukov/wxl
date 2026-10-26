module;

#include "abi.h"

export module wxl.core:time;

import :compressed_optional;

import std;

export namespace wxl::core {

class time_span;
class duration;

/// The number of ticks in one microsecond. A tick is 100 nanoseconds for
/// `wxl::core::time_stamp`, `wxl::core::time_span` and `wxl::core::duration`, matching the
/// granularity of the OS interrupt-time clock that `wxl::core::time_stamp::now` reads.
inline constexpr int64_t ticks_per_us = 10;
/// The number of ticks in one millisecond.
inline constexpr int64_t ticks_per_ms = 10'000;
/// The number of ticks in one second.
inline constexpr int64_t ticks_per_sec = 10'000'000;

/// A point in time, in 100ns ticks since an unspecified but fixed epoch (the
/// system's monotonic interrupt-time clock: unaffected by wall-clock
/// adjustments, not comparable across reboots or machines). Meaningful only
/// as a difference against another time_stamp, or as a deadline to wait until.
class time_stamp
{
    int64_t ticks_ = 0;

    explicit constexpr time_stamp(int64_t ticks) noexcept : ticks_(ticks) {}

public:
    constexpr time_stamp() noexcept = default;

    /// Returns the current value of the interrupt-time clock. Reads `KUSER_SHARED_DATA`
    /// directly, without a system call.
    static time_stamp now() noexcept;

    /// Creates a time stamp \p ticks ticks after the clock epoch.
    static constexpr time_stamp from_ticks(int64_t ticks) noexcept { return time_stamp(ticks); }
    /// Gets the number of ticks since the clock epoch.
    constexpr int64_t ticks() const noexcept { return ticks_; }

    /// Moves the time stamp by \p rhs.
    constexpr time_stamp& operator+=(const time_span& rhs) noexcept;
    /// Moves the time stamp back by \p rhs.
    constexpr time_stamp& operator-=(const time_span& rhs) noexcept;

    /// Returns the signed span from \p b to \p a.
    friend constexpr time_span operator-(const time_stamp& a, const time_stamp& b) noexcept;
    /// Returns \p a moved by \p b.
    friend constexpr time_stamp operator+(time_stamp a, const time_span& b) noexcept {
        return a += b;
    }
    /// Returns \p a moved back by \p b.
    friend constexpr time_stamp operator-(time_stamp a, const time_span& b) noexcept {
        return a -= b;
    }

    /// Compares two time stamps by their tick counts.
    friend constexpr auto operator<=>(const time_stamp&, const time_stamp&) noexcept = default;
    /// Tests whether two time stamps have the same tick count.
    friend constexpr bool operator==(const time_stamp&, const time_stamp&) noexcept = default;
};

/// A possibly-negative span of time, in 100ns ticks (e.g. the difference of
/// two time_stamps, or of two durations).
class time_span
{
    int64_t ticks_ = 0;

    explicit constexpr time_span(int64_t ticks) noexcept : ticks_(ticks) {}

public:
    constexpr time_span() noexcept = default;

    /// Converts a duration to a time span. The conversion is implicit. A duration above the
    /// largest `int64_t` tick count, the `nullable<duration>` sentinel included, wraps to a
    /// negative span.
    constexpr time_span(const duration& d) noexcept;

    /// Converts a `std::chrono::duration` to a time span, truncating toward zero to whole
    /// ticks.
    template <class Rep, class Period>
    constexpr time_span(const std::chrono::duration<Rep, Period>& d) noexcept
        : ticks_(
              std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>(
                  d)
                  .count()) {}

    /// Creates a time span of \p ticks ticks.
    static constexpr time_span from_ticks(int64_t ticks) noexcept { return time_span(ticks); }
    /// Creates a time span of \p us microseconds.
    static constexpr time_span from_us(int64_t us) noexcept { return time_span(us * ticks_per_us); }
    /// Creates a time span of \p ms milliseconds.
    static constexpr time_span from_ms(int64_t ms) noexcept { return time_span(ms * ticks_per_ms); }
    /// Creates a time span of \p sec seconds.
    static constexpr time_span from_sec(int64_t sec) noexcept {
        return time_span(sec * ticks_per_sec);
    }

    /// Gets the length of the span in ticks.
    constexpr int64_t ticks() const noexcept { return ticks_; }
    /// Returns the length of the span in whole microseconds, truncated toward zero.
    constexpr int64_t total_microseconds() const noexcept { return ticks_ / ticks_per_us; }
    /// Returns the length of the span in whole milliseconds, truncated toward zero.
    constexpr int64_t total_milliseconds() const noexcept { return ticks_ / ticks_per_ms; }
    /// Returns the length of the span in whole seconds, truncated toward zero.
    constexpr int64_t total_seconds() const noexcept { return ticks_ / ticks_per_sec; }

    /// Tests whether the span is nonzero.
    constexpr explicit operator bool() const noexcept { return ticks_ != 0; }

    /// Returns the span with its sign inverted.
    constexpr time_span operator-() const noexcept { return time_span(-ticks_); }
    /// Returns the absolute value of the span.
    constexpr time_span abs() const noexcept { return ticks_ < 0 ? -*this : *this; }

    /// Adds \p rhs to the span.
    constexpr time_span& operator+=(const time_span& rhs) noexcept {
        ticks_ += rhs.ticks_;
        return *this;
    }
    /// Subtracts \p rhs from the span.
    constexpr time_span& operator-=(const time_span& rhs) noexcept {
        ticks_ -= rhs.ticks_;
        return *this;
    }
    /// Multiplies the span by \p k, truncating the result toward zero to whole ticks.
    constexpr time_span& operator*=(long double k) noexcept {
        ticks_ = static_cast<int64_t>(ticks_ * k);
        return *this;
    }
    /// Divides the span by \p k, truncating the result toward zero to whole ticks.
    constexpr time_span& operator/=(long double k) noexcept {
        ticks_ = static_cast<int64_t>(ticks_ / k);
        return *this;
    }

    /// Returns the sum of \p a and \p b.
    friend constexpr time_span operator+(time_span a, const time_span& b) noexcept {
        return a += b;
    }
    /// Returns the difference of \p a and \p b.
    friend constexpr time_span operator-(time_span a, const time_span& b) noexcept {
        return a -= b;
    }
    /// Returns \p a multiplied by \p k, truncated toward zero to whole ticks.
    friend constexpr time_span operator*(time_span a, long double k) noexcept { return a *= k; }
    /// Returns \p a divided by \p k, truncated toward zero to whole ticks.
    friend constexpr time_span operator/(time_span a, long double k) noexcept { return a /= k; }

    /// Compares two time spans by their tick counts.
    friend constexpr auto operator<=>(const time_span&, const time_span&) noexcept = default;
    /// Tests whether two time spans have the same tick count.
    friend constexpr bool operator==(const time_span&, const time_span&) noexcept = default;
};

/// A non-negative span of time, in 100ns ticks; used as a timeout.
class duration
{
    uint64_t ticks_ = 0;

    explicit constexpr duration(uint64_t ticks) noexcept : ticks_(ticks) {}

public:
    constexpr duration() noexcept = default;

    /// Converts a time span to a duration. \p span must not be negative; this is asserted.
    explicit constexpr duration(const time_span& span) noexcept
        : ticks_(static_cast<uint64_t>(span.ticks())) {
        assert(span.ticks() >= 0);
    }

    /// Converts a `std::chrono::duration` to a duration through `wxl::core::time_span`,
    /// truncating toward zero to whole ticks. \p d must not be negative; this is asserted.
    template <class Rep, class Period>
    constexpr duration(const std::chrono::duration<Rep, Period>& d) noexcept
        : duration(time_span(d)) {}

    /// Creates a duration of \p ticks ticks.
    static constexpr duration from_ticks(uint64_t ticks) noexcept { return duration(ticks); }
    /// Creates a duration of \p us microseconds.
    static constexpr duration from_us(uint64_t us) noexcept { return duration(us * ticks_per_us); }
    /// Creates a duration of \p ms milliseconds.
    static constexpr duration from_ms(uint64_t ms) noexcept { return duration(ms * ticks_per_ms); }
    /// Creates a duration of \p sec seconds.
    static constexpr duration from_sec(uint64_t sec) noexcept {
        return duration(sec * ticks_per_sec);
    }

    /// Returns a duration of zero ticks.
    static constexpr duration zero() noexcept { return duration(); }

    // The `int timeout_in_ms` convention is bridged by timeout_from_ms() at
    // the bottom of this file rather than by a member here: what it returns is
    // nullable<duration>, and that name cannot be formed until the selector
    // below it -- which needs `duration` complete -- has been written.

    /// Gets the length of the duration in ticks.
    constexpr uint64_t ticks() const noexcept { return ticks_; }
    /// Returns the length of the duration in whole microseconds, rounded down.
    constexpr uint64_t total_microseconds() const noexcept { return ticks_ / ticks_per_us; }
    /// Returns the length of the duration in whole milliseconds, rounded down.
    constexpr uint64_t total_milliseconds() const noexcept { return ticks_ / ticks_per_ms; }
    /// Returns the length of the duration in whole seconds, rounded down.
    constexpr uint64_t total_seconds() const noexcept { return ticks_ / ticks_per_sec; }

    /// Tests whether the duration is nonzero.
    constexpr explicit operator bool() const noexcept { return ticks_ != 0; }

    /// Adds \p rhs to the duration. The result remains a duration because the sum of two
    /// non-negative spans is non-negative.
    constexpr duration& operator+=(const duration& rhs) noexcept {
        ticks_ += rhs.ticks_;
        return *this;
    }
    /// Returns the sum of \p a and \p b.
    friend constexpr duration operator+(duration a, const duration& b) noexcept { return a += b; }

    /// Compares two durations by their tick counts.
    friend constexpr auto operator<=>(const duration&, const duration&) noexcept = default;
    /// Tests whether two durations have the same tick count.
    friend constexpr bool operator==(const duration&, const duration&) noexcept = default;
};

/// Milliseconds in the form the OS wait calls take.
///
/// A timeout too large for the field saturates just below 0xFFFFFFFF -- Win32's INFINITE --
/// rather than on it: turning a long wait into an endless one is the one rounding error the
/// caller could never recover from. Waiting with no deadline at all is not expressible here
/// and needs nothing from this function: it is a call of its own everywhere in this library
/// (`wait()` beside `wait_for(timeout)`), and passes INFINITE itself.
constexpr uint32_t to_os_timeout_ms(duration timeout) noexcept {
    constexpr uint32_t forever = 0xFFFF'FFFFu;

    const uint64_t ms = timeout.total_milliseconds();

    return ms < forever ? static_cast<uint32_t>(ms) : forever - 1;
}

// --- out-of-line definitions ---

constexpr time_span::time_span(const duration& d) noexcept
    : ticks_(static_cast<int64_t>(d.ticks())) {}

constexpr time_stamp& time_stamp::operator+=(const time_span& rhs) noexcept {
    ticks_ += rhs.ticks();
    return *this;
}
constexpr time_stamp& time_stamp::operator-=(const time_span& rhs) noexcept {
    ticks_ -= rhs.ticks();
    return *this;
}

/// Returns the signed span from \p b to \p a.
constexpr time_span operator-(const time_stamp& a, const time_stamp& b) noexcept {
    return time_span::from_ticks(a.ticks_ - b.ticks_);
}

/// Returns the signed difference of two durations as a `wxl::core::time_span`.
constexpr time_span operator-(const duration& a, const duration& b) noexcept {
    return time_span(a) - time_span(b);
}

/// Selects the representation of `wxl::core::nullable` for `wxl::core::duration`: the largest
/// tick count stands for no duration at all.
///
/// `wxl::core::to_os_timeout_ms` saturates one below the OS `INFINITE` value and never passes
/// `INFINITE` itself, because turning a long wait into an endless one is a rounding error the
/// caller cannot recover from. The largest representable duration, about 58,000 years, is
/// therefore a value the library never produces, which is what a sentinel requires. Zero
/// remains a valid duration: an empty interval means "as soon as possible", which differs
/// from the absence of a timeout.
template <>
struct optional_selector<duration> {
    /// Provides the sentinel that represents an empty `nullable<duration>`.
    struct forever_sentinel {
        /// Returns the sentinel duration, `std::numeric_limits<uint64_t>::max()` ticks.
        static constexpr duration sentinel() noexcept {
            return duration::from_ticks(std::numeric_limits<uint64_t>::max());
        }

        /// Tests whether \p value is the sentinel duration.
        static constexpr bool is_sentinel(const duration& value) noexcept {
            return value.ticks() == std::numeric_limits<uint64_t>::max();
        }
    };

    /// The type that holds an optional `wxl::core::duration` in the storage of the duration
    /// itself, with `forever_sentinel` as the empty state.
    using nullable = compressed_optional<duration, forever_sentinel>;
};

/// Bridges the `int timeout_in_ms` convention -- milliseconds, and a negative number
/// meaning "no deadline at all" -- spoken by the OS wait calls and by plenty of code
/// on the way to them.
///
/// \return the timeout, or nothing when the number asks for an endless wait. That is
///         deliberately not a duration and never becomes one: waiting without a
///         deadline is a call of its own everywhere here, and the code reading this
///         number is exactly where the two part ways.
constexpr nullable<duration> timeout_from_ms(int timeout_in_ms) noexcept {
    if (timeout_in_ms < 0) return std::nullopt;

    return duration::from_ms(static_cast<uint64_t>(timeout_in_ms));
}

}  // export namespace wxl::core
