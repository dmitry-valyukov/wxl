module;

#include "platform.h"

// The standard streams are macros, and macros do not come through import std.
#include <cstdio>


module wxl.logging;

import wxl.core;
import std;

namespace wxl::logging {
namespace {

/// Turns on the terminal's escape-sequence handling, once per stream and only
/// if the stream is a console at all: a redirected stream keeps its mode, and
/// asking for one it cannot have is not an error worth reporting.
void enable_escape_sequences(const DWORD which) noexcept {
    const HANDLE handle = ::GetStdHandle(which);

    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return;

    DWORD mode = 0;

    if (!::GetConsoleMode(handle, &mode)) return;

    ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/// The colour each level is written in. Info is left in whatever the terminal
/// is set to: it is the level a running program mostly speaks at, and colouring
/// all of it colours nothing.
std::string_view color_of(const severity level) noexcept {
    switch (level) {
        case severity::fatal: return "\x1b[1;31m";  // bold red
        case severity::error: return "\x1b[31m";    // red
        case severity::warn: return "\x1b[33m";     // yellow
        case severity::info: return {};
        case severity::trace: return "\x1b[36m";    // cyan
        case severity::debug: return "\x1b[90m";    // grey
    }

    return {};
}

constexpr std::string_view color_off = "\x1b[0m";

void write_piece(std::FILE* const stream, const std::string_view piece) noexcept {
    if (!piece.empty()) std::fwrite(piece.data(), 1, piece.size(), stream);
}

}  // namespace

console_output::console_output(const console_stream stream, const severity_range shows,
                               const console_options options)
    : stream_(stream), shows_(shows), options_(options) {
    if (options_.colored)
        enable_escape_sequences(stream_ == console_stream::standard_output ? STD_OUTPUT_HANDLE
                                                                          : STD_ERROR_HANDLE);
}

std::string_view console_output::id() const noexcept {
    return stream_ == console_stream::standard_output ? "console:stdout" : "console:stderr";
}

void console_output::write(const log_entry& entry) {
    if (!shows_.contains(entry.level)) return;

    std::FILE* const stream = stream_ == console_stream::standard_output ? stdout : stderr;
    const std::string_view color = options_.colored ? color_of(entry.level) : std::string_view();

    const core::lock_guard<core::mutex> lock(mutex_);

    write_piece(stream, color);
    write_piece(stream, options_.show_prefix ? entry.text() : entry.message());
    write_piece(stream, color.empty() ? std::string_view() : color_off);
}

}  // namespace wxl::logging
