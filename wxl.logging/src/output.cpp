module;

#include "abi.h"

module wxl.logging;

import wxl.core;
import std;

namespace wxl::logging {
namespace {

/// Says on stderr that an output has been dropped. stderr and not the log: the
/// log is the thing that just stopped working.
void report_failure(const log_output& output, const std::string_view reason) {
    std::cerr << "wxl.logging: output '" << output.id() << "' failed and was dropped: " << reason
              << '\n';
}

}  // namespace

null_output& null_output::instance() noexcept {
    static null_output the_only_one;

    return the_only_one;
}

void multicast_output::subscribe(log_output& output) {
    const core::lock_guard<core::mutex> lock(mutex_);

    ensure(std::ranges::find(outputs_, &output) == outputs_.end());

    outputs_.push_back(&output);
}

void multicast_output::unsubscribe(log_output& output) {
    const core::lock_guard<core::mutex> lock(mutex_);

    const auto at = std::ranges::find(outputs_, &output);

    ensure(at != outputs_.end());

    outputs_.erase(at);
}

std::size_t multicast_output::size() const {
    const core::lock_guard<core::mutex> lock(mutex_);

    return outputs_.size();
}

void multicast_output::write(const log_entry& entry) {
    const core::lock_guard<core::mutex> lock(mutex_);

    write_unlocked(entry);
}

void multicast_output::write_unlocked(const log_entry& entry) {
    bool anything_failed = false;

    // Marked first and erased afterwards: erasing inside the loop would move
    // the outputs still to be written past the iterator standing on them.
    for (log_output*& output : outputs_) {
        try {
            output->write(entry);
        } catch (const std::exception& error) {
            report_failure(*output, error.what());
            output = nullptr;
            anything_failed = true;
        } catch (...) {
            report_failure(*output, "an exception that is not a std::exception");
            output = nullptr;
            anything_failed = true;
        }
    }

    if (anything_failed) std::erase(outputs_, nullptr);
}

}  // namespace wxl::logging
