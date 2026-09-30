// The default date is two months from today, the years run from this one to
// five years on, and the day shows as a number with its weekday.
auto const now = std::chrono::time_point_cast<DateTime::duration>(std::chrono::system_clock::now());

auto picker = DatePicker {
    dayFormat = u"{day.integer} ({dayofweek.abbreviated})",
    yearVisible = false,
    date = now + std::chrono::months {2},
    minYear = now,
    maxYear = now + std::chrono::years {5},
};