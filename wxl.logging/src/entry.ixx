export module wxl.logging:entry;

import :severity;
import wxl.core;
import wxl.fmt;
import std;

export namespace wxl::logging {

/// The clock log lines are stamped with: wall-clock time, because a log is read
/// next to other logs and next to what people remember about the day. The
/// monotonic wxl::core::time_stamp is the better clock for measuring, and the
/// wrong one to print.
using log_clock = std::chrono::system_clock;

/// A point on that clock.
using log_time = log_clock::time_point;

/// Width of the timestamp a line starts with: `YYYYMMDD-HH:MM:SS.fffffffff`,
/// in UTC. Fixed, so the columns of a log line up and a reader -- human or
/// otherwise -- can cut the prefix off by counting bytes.
inline constexpr std::size_t timestamp_size = 27;

/// Writes the timestamp of \p time into \p out, which is exactly that wide.
///
/// The date arithmetic is std::chrono's own and the digits are written by hand,
/// which is both faster than a format string and free of any locale: no
/// separator, digit or month name here can be changed by what the process
/// happens to be set to.
///
/// The clock is read to whatever precision the system offers -- 100ns on
/// Windows -- and printed to nanoseconds, so the last two digits stand still.
/// Nine digits rather than seven because that is the width other logs and the
/// tools that read them expect.
void write_timestamp(std::span<char, timestamp_size> out, log_time time) noexcept;

/// One log line on its way to an output.
///
/// The line is assembled once, whole, into `buffer`: prefix first, then the
/// message, then the newline that ends it. An output writes `text()` and is
/// done -- there is nothing left for it to format, and nothing it has to join.
///
/// `prefix_size` is what makes the prefix optional without formatting the line
/// twice: an output that shows its own timestamp, or none at all, writes
/// `message()` instead and the same buffer serves both.
struct log_entry {
    /// When the message was made -- stamped by the logger, on the thread that
    /// made it, and not by the output that eventually writes it. In the
    /// asynchronous case those are different threads and can be far apart.
    log_time time{};

    severity level = severity::info;

    /// Bytes of `buffer` taken by the prefix: timestamp, level, facility.
    std::size_t prefix_size = 0;

    /// The whole line, newline included.
    core::text_builder<> buffer;

    /// The line as written, prefix and all.
    inline std::string_view text() const noexcept { return buffer.view(); }

    /// The line without its prefix.
    inline std::string_view message() const noexcept { return text().substr(prefix_size); }

    /// Empties the line, keeping the memory: an entry reused for the next
    /// message allocates nothing after the first one it ever held.
    inline void reset() noexcept {
        buffer.reset();
        prefix_size = 0;
    }
};

}  // export namespace wxl::logging
