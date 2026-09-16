module wxl.logging;

import std;

namespace wxl::logging {

void base_logger::start_line(log_entry& entry, const severity level,
                             const facility* const where) const {
    entry.time = log_clock::now();
    entry.level = level;

    char stamp[timestamp_size];
    write_timestamp(stamp, entry.time);
    entry.buffer.append(std::string_view(stamp, timestamp_size));

    // An ordinary message goes unmarked, and that is the format this library
    // was ported from: info is what a running program mostly says, and a column
    // of <INFO> down the left of the file tells the reader nothing they could
    // not have assumed. Anything else is worth a word.
    if (level != severity::info) {
        entry.buffer.append(" <");
        entry.buffer.append(severity_name(level));
        entry.buffer.append('>');
    }

    if (where) {
        entry.buffer.append(" [");
        entry.buffer.append(where->full_name());
        entry.buffer.append(']');
    }

    entry.buffer.append(' ');

    entry.prefix_size = entry.buffer.size();
}

}  // namespace wxl::logging
