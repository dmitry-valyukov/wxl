module wxl.logging;

import std;

namespace wxl::logging {
namespace {

/// Writes \p value right-aligned into \p width digits, zero-padded, with no
/// terminator and no bounds check: every caller below knows its own field.
constexpr void put_digits(char* out, int value, int width) noexcept {
    while (width-- > 0) {
        out[width] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
}

}  // namespace

void write_timestamp(const std::span<char, timestamp_size> out, const log_time time) noexcept {
    using namespace std::chrono;

    // floor rather than a division: the epoch is in 1970 and time before it
    // would round the wrong way, which is the sort of thing a log finds out
    // about on the one day the clock is set backwards.
    const auto midnight = floor<days>(time);
    const year_month_day date{midnight};

    const auto since_midnight = duration_cast<nanoseconds>(time - midnight);
    const auto whole_seconds = floor<seconds>(since_midnight);

    char* const at = out.data();

    put_digits(at + 0, static_cast<int>(date.year()), 4);
    put_digits(at + 4, static_cast<int>(static_cast<unsigned>(date.month())), 2);
    put_digits(at + 6, static_cast<int>(static_cast<unsigned>(date.day())), 2);
    at[8] = '-';
    put_digits(at + 9, static_cast<int>(duration_cast<hours>(whole_seconds).count()), 2);
    at[11] = ':';
    put_digits(at + 12, static_cast<int>(duration_cast<minutes>(whole_seconds).count() % 60), 2);
    at[14] = ':';
    put_digits(at + 15, static_cast<int>(whole_seconds.count() % 60), 2);
    at[17] = '.';
    put_digits(at + 18, static_cast<int>((since_midnight - whole_seconds).count()), 9);
}

}  // namespace wxl::logging
