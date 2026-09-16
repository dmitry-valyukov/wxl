#pragma once

// A wall-clock instant, as WinRT hands one over -- and the sentinel that lets
// it be optional without costing a byte.
//
// WinRT's DateTime is a count of 100ns ticks from 1601-01-01, which is what
// cppwinrt's own clock counts too. wxl says it in std::chrono against
// `system_clock` instead: the epoch differs (1970), the conversion happens
// once in impl/conversions.h, and what an application gets back is a time
// point the rest of its C++ already speaks -- comparable, formattable, and
// convertible with everything in <chrono>.
//
// Deliberately *not* core::time_stamp, which is wxl's own point in time: that
// one reads the monotonic interrupt-time clock from an unspecified epoch, is
// not comparable across reboots, and is exactly right for a deadline and
// exactly wrong for a date somebody picked in a calendar.

#include "core.h"

namespace wxl {

/// The instant a WinRT DateTime stands for.
using DateTime = std::chrono::time_point<std::chrono::system_clock,
                                         std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>>;

}  // namespace wxl

namespace wxl::core {

// "No date" is WinRT's own zero -- 1601-01-01T00:00:00Z, the instant a
// default-constructed DateTime denotes on that side of the ABI. Empty
// therefore means the same thing in both directions, and nothing has to
// remember which side it is on.
//
// It costs a representable date, and the one it costs is the one no calendar
// offers: WinUI's own pickers refuse anything before 1601 to begin with, since
// that is where their tick count starts.
template <>
struct optional_selector<wxl::DateTime> {
    struct winrt_epoch_sentinel {
        // 1601-01-01 to 1970-01-01 is 11 644 473 600 seconds, and system_clock
        // counts from the later of the two.
        static constexpr wxl::DateTime::duration winrt_epoch{-116'444'736'000'000'000};

        static wxl::DateTime sentinel() noexcept { return wxl::DateTime{winrt_epoch}; }

        static bool is_sentinel(wxl::DateTime const& value) noexcept {
            return value.time_since_epoch() == winrt_epoch;
        }
    };

    using nullable = compressed_optional<wxl::DateTime, winrt_epoch_sentinel>;
};

}  // namespace wxl::core
