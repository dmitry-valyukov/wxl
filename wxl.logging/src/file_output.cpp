module;

#include "abi.h"

module wxl.logging;

import wxl.core;
import fmt;
import std;

namespace wxl::logging {
namespace {

/// How many parts of a previous run rotate_by_run is willing to look for. A
/// bound rather than a promise: a directory full of unrelated files must not
/// turn opening the log into an endless walk.
constexpr unsigned max_parts_to_rotate = 1000;

/// `app.log` -> `app.log.1`, `app.log.1` -> `app.log.2`, and the one that would
/// become number depth + 1 is dropped. Missing links are simply skipped, so a
/// history with holes in it closes them up rather than stopping.
void shift_history(const std::filesystem::path& path, const std::size_t history_depth) {
    const auto numbered = [&path](const std::size_t number) {
        return std::filesystem::path(path).concat("." + wxl::core::to_string(number));
    };

    std::error_code failed;  // renaming files nobody asked about must not throw

    if (history_depth == 0) {
        std::filesystem::remove(path, failed);
        return;
    }

    std::filesystem::remove(numbered(history_depth), failed);

    for (std::size_t number = history_depth; number > 1; --number)
        std::filesystem::rename(numbered(number - 1), numbered(number), failed);

    std::filesystem::rename(path, numbered(1), failed);
}

}  // namespace

file_output::file_output(const std::filesystem::path& path, const file_options options)
    : directory_(path.parent_path()),
      stem_(path.stem().string()),
      extension_(path.extension().string()),
      id_("file:" + path.filename().string()),
      options_(options) {
    ensure(!stem_.empty());

    if (!directory_.empty()) std::filesystem::create_directories(directory_);

    if (options_.rotate_by_run) rotate_history();

    open_part(1);
}

file_output::~file_output() = default;

std::string_view file_output::id() const noexcept {
    return id_;
}

std::filesystem::path file_output::current_path() const {
    const core::lock_guard<core::mutex> lock(mutex_);

    return current_path_;
}

std::uint64_t file_output::current_size() const {
    const core::lock_guard<core::mutex> lock(mutex_);

    return current_size_;
}

unsigned file_output::part_number() const {
    const core::lock_guard<core::mutex> lock(mutex_);

    return part_number_;
}

void file_output::write(const log_entry& entry) {
    if (!options_.shows.contains(entry.level)) return;

    const core::lock_guard<core::mutex> lock(mutex_);

    if (header_pending_) {
        header_pending_ = false;

        on_before_first_line();
    }

    write_to_file(entry.text());

    // Checked after the line and not before it: a line is never split between
    // two files, so a part can end a little past max_size and never in the
    // middle of a message.
    if (options_.max_size != 0 && current_size_ >= options_.max_size)
        open_part(part_number_ + 1);
}

void file_output::write_to_file(const std::string_view text) {
    file_.write(text.data(), static_cast<std::streamsize>(text.size()));
    file_.flush();

    current_size_ += text.size();
}

void file_output::on_before_first_line() {}

std::filesystem::path file_output::make_part_name(const unsigned number) const {
    std::string name = stem_;

    if (options_.rotate_by_date) {
        const auto today = std::chrono::floor<std::chrono::days>(log_clock::now());
        const std::chrono::year_month_day date{today};

        name += fmt::format("-{:04}{:02}{:02}", static_cast<int>(date.year()),
                            static_cast<unsigned>(date.month()),
                            static_cast<unsigned>(date.day()));
    }

    if (number > 1) name += fmt::format("-part{:03}", number);

    name += extension_;

    return directory_ / name;
}

void file_output::open_part(const unsigned number) {
    if (file_.is_open()) file_.close();

    current_path_ = make_part_name(number);
    part_number_ = number;

    // Binary, so the bytes in the file are the bytes that were written: the
    // lines already end in '\n', and text mode would silently add a byte per
    // line that max_size would then have to guess about. A log read on the
    // machine that wrote it is read by tools that have handled bare newlines
    // for a decade.
    std::ios::openmode mode = std::ios::out | std::ios::binary;

    // Appending applies to the first part only. A part that has just been
    // started because the previous one filled up is new by definition, and
    // appending to a leftover of an earlier run would carry its size along.
    mode |= (options_.append && number == 1) ? std::ios::app : std::ios::trunc;

    file_.open(current_path_, mode);

    if (!file_.is_open())
        throw std::system_error(errno, std::generic_category(),
                                "wxl.logging: cannot open " + current_path_.string());

    std::error_code unknown_size;
    const auto size_on_disk = std::filesystem::file_size(current_path_, unknown_size);

    current_size_ = unknown_size ? 0 : size_on_disk;

    header_pending_ = true;
}

void file_output::rotate_history() {
    // Every part of the previous run moves down, not just the first: a run that
    // filled three files leaves three, and shifting only one of them would let
    // this run overwrite the other two the moment it reached them.
    for (unsigned number = 1; number <= max_parts_to_rotate; ++number) {
        const std::filesystem::path part = make_part_name(number);

        if (!std::filesystem::exists(part)) break;

        shift_history(part, options_.history_depth);
    }
}

std::vector<std::filesystem::path> file_output::history_of(const std::filesystem::path& path,
                                                           const std::size_t history_depth) {
    std::vector<std::filesystem::path> found;

    if (!std::filesystem::exists(path)) return found;

    found.push_back(path);

    for (std::size_t number = 1; number <= history_depth; ++number) {
        std::filesystem::path older = std::filesystem::path(path).concat("." + wxl::core::to_string(number));

        if (!std::filesystem::exists(older)) break;

        found.push_back(std::move(older));
    }

    return found;
}

}  // namespace wxl::logging
