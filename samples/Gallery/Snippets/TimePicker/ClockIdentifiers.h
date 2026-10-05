// Both show the time of day it is now.
auto const now = std::chrono::time_point_cast<DateTime::duration>(std::chrono::system_clock::now());
auto const sinceMidnight = core::duration {now - std::chrono::floor<std::chrono::days>(now)};

auto pickers = StackPanel {
    spacing = 8.0,
    TimePicker {clockIdentifier = u"12HourClock", header = u"12-hour clock", selectedTime = sinceMidnight},
    TimePicker {clockIdentifier = u"24HourClock", header = u"24-hour clock", selectedTime = sinceMidnight},
};
