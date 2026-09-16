module;

#include "abi.h"

export module wxl.core:time;

import :compressed_optional;

import std;

export namespace wxl::core {

class time_span;
class duration;

/// One tick is 100ns for all three types below, matching the granularity of
/// the OS interrupt-time clock time_stamp::now() reads.
inline constexpr int64_t ticks_per_us = 10;
inline constexpr int64_t ticks_per_ms = 10'000;
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

    /// Current time. Reads KUSER_SHARED_DATA directly, no syscall.
    static time_stamp now() noexcept;

    static constexpr time_stamp from_ticks(int64_t ticks) noexcept { return time_stamp(ticks); }
    constexpr int64_t ticks() const noexcept { return ticks_; }

    constexpr time_stamp& operator+=(const time_span& rhs) noexcept;
    constexpr time_stamp& operator-=(const time_span& rhs) noexcept;

    friend constexpr time_span operator-(const time_stamp& a, const time_stamp& b) noexcept;
    friend constexpr time_stamp operator+(time_stamp a, const time_span& b) noexcept {
        return a += b;
    }
    friend constexpr time_stamp operator-(time_stamp a, const time_span& b) noexcept {
        return a -= b;
    }

    friend constexpr auto operator<=>(const time_stamp&, const time_stamp&) noexcept = default;
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

    /// Widening: a duration is always a valid (non-negative) time_span.
    constexpr time_span(const duration& d) noexcept;

    template <class Rep, class Period>
    constexpr time_span(const std::chrono::duration<Rep, Period>& d) noexcept
        : ticks_(
              std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>(
                  d)
                  .count()) {}

    static constexpr time_span from_ticks(int64_t ticks) noexcept { return time_span(ticks); }
    static constexpr time_span from_us(int64_t us) noexcept { return time_span(us * ticks_per_us); }
    static constexpr time_span from_ms(int64_t ms) noexcept { return time_span(ms * ticks_per_ms); }
    static constexpr time_span from_sec(int64_t sec) noexcept {
        return time_span(sec * ticks_per_sec);
    }

    constexpr int64_t ticks() const noexcept { return ticks_; }
    constexpr int64_t total_microseconds() const noexcept { return ticks_ / ticks_per_us; }
    constexpr int64_t total_milliseconds() const noexcept { return ticks_ / ticks_per_ms; }
    constexpr int64_t total_seconds() const noexcept { return ticks_ / ticks_per_sec; }

    constexpr explicit operator bool() const noexcept { return ticks_ != 0; }

    constexpr time_span operator-() const noexcept { return time_span(-ticks_); }
    constexpr time_span abs() const noexcept { return ticks_ < 0 ? -*this : *this; }

    constexpr time_span& operator+=(const time_span& rhs) noexcept {
        ticks_ += rhs.ticks_;
        return *this;
    }
    constexpr time_span& operator-=(const time_span& rhs) noexcept {
        ticks_ -= rhs.ticks_;
        return *this;
    }
    constexpr time_span& operator*=(long double k) noexcept {
        ticks_ = static_cast<int64_t>(ticks_ * k);
        return *this;
    }
    constexpr time_span& operator/=(long double k) noexcept {
        ticks_ = static_cast<int64_t>(ticks_ / k);
        return *this;
    }

    friend constexpr time_span operator+(time_span a, const time_span& b) noexcept {
        return a += b;
    }
    friend constexpr time_span operator-(time_span a, const time_span& b) noexcept {
        return a -= b;
    }
    friend constexpr time_span operator*(time_span a, long double k) noexcept { return a *= k; }
    friend constexpr time_span operator/(time_span a, long double k) noexcept { return a /= k; }

    friend constexpr auto operator<=>(const time_span&, const time_span&) noexcept = default;
    friend constexpr bool operator==(const time_span&, const time_span&) noexcept = default;
};

/// A non-negative span of time, in 100ns ticks; used as a timeout.
class duration
{
    uint64_t ticks_ = 0;

    explicit constexpr duration(uint64_t ticks) noexcept : ticks_(ticks) {}

public:
    constexpr duration() noexcept = default;

    /// Narrowing: span must not be negative.
    explicit constexpr duration(const time_span& span) noexcept
        : ticks_(static_cast<uint64_t>(span.ticks())) {
        assert(span.ticks() >= 0);
    }

    template <class Rep, class Period>
    constexpr duration(const std::chrono::duration<Rep, Period>& d) noexcept
        : duration(time_span(d)) {}

    static constexpr duration from_ticks(uint64_t ticks) noexcept { return duration(ticks); }
    static constexpr duration from_us(uint64_t us) noexcept { return duration(us * ticks_per_us); }
    static constexpr duration from_ms(uint64_t ms) noexcept { return duration(ms * ticks_per_ms); }
    static constexpr duration from_sec(uint64_t sec) noexcept {
        return duration(sec * ticks_per_sec);
    }

    static constexpr duration zero() noexcept { return duration(); }

    /// Bridges the `int timeout_in_ms` convention -- milliseconds, and a negative number
    /// meaning "no deadline at all" -- spoken by the OS wait calls and by plenty of code
    /// on the way to them.
    ///
    /// \return the timeout, or nothing when the number asks for an endless wait. That is
    ///         deliberately not a duration and never becomes one: waiting without a
    ///         deadline is a call of its own everywhere here, and the code reading this
    ///         number is exactly where the two part ways.
    // The `int timeout_in_ms` convention is bridged by timeout_from_ms() at
    // the bottom of this file rather than by a member here: what it returns is
    // nullable<duration>, and that name cannot be formed until the selector
    // below it -- which needs `duration` complete -- has been written.

    constexpr uint64_t ticks() const noexcept { return ticks_; }
    constexpr uint64_t total_microseconds() const noexcept { return ticks_ / ticks_per_us; }
    constexpr uint64_t total_milliseconds() const noexcept { return ticks_ / ticks_per_ms; }
    constexpr uint64_t total_seconds() const noexcept { return ticks_ / ticks_per_sec; }

    constexpr explicit operator bool() const noexcept { return ticks_ != 0; }

    /// Stays a duration: the sum of two non-negative spans is non-negative.
    constexpr duration& operator+=(const duration& rhs) noexcept {
        ticks_ += rhs.ticks_;
        return *this;
    }
    friend constexpr duration operator+(duration a, const duration& b) noexcept { return a += b; }

    friend constexpr auto operator<=>(const duration&, const duration&) noexcept = default;
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

constexpr time_span operator-(const time_stamp& a, const time_stamp& b) noexcept {
    return time_span::from_ticks(a.ticks_ - b.ticks_);
}

constexpr time_span operator-(const duration& a, const duration& b) noexcept {
    return time_span(a) - time_span(b);
}

// "No duration at all", in the one tick count that can never be one.
//
// The reservation is not new here, only named: to_os_timeout_ms above already
// refuses to hand the OS its INFINITE, saturating one below it, because
// turning a long wait into an endless one is the rounding error a caller
// could never recover from. The largest representable duration is therefore
// already a value the library never produces, which is exactly what a
// sentinel has to be -- 58 000 years, and zero stays a legal duration (an
// interval of nothing means "as soon as possible", and that is not the same
// as not asking).
template <>
struct optional_selector<duration> {
    struct forever_sentinel {
        static constexpr duration sentinel() noexcept {
            return duration::from_ticks(std::numeric_limits<uint64_t>::max());
        }

        static constexpr bool is_sentinel(const duration& value) noexcept {
            return value.ticks() == std::numeric_limits<uint64_t>::max();
        }
    };

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
