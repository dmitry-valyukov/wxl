module;
#include "pch.h"

#include <intrin.h>

module wxl.core;
import std;

namespace wxl::core {

namespace {

/// One line, `file(line): what`, the form a compiler reports in, so the debugger's
/// output window takes it to the place. Into a buffer on the stack: the report
/// may be about memory that has run out.
void write_line(std::source_location where, std::string_view prefix, std::string_view what,
                std::string_view suffix) noexcept {
    char line[1024];
    const int length = std::snprintf(line, sizeof line, "%s(%u): %.*s%.*s%.*s\n", where.file_name(),
                                     static_cast<unsigned>(where.line()), static_cast<int>(prefix.size()),
                                     prefix.data(), static_cast<int>(what.size()), what.data(),
                                     static_cast<int>(suffix.size()), suffix.data());

    // A report cut short keeps its end of line.
    int used = length < 0 ? 0 : length;
    if (used >= static_cast<int>(sizeof line)) {
        used = static_cast<int>(sizeof line) - 1;
        line[used - 1] = '\n';
    }

    std::cerr.write(line, used);
    std::cerr.flush();

    // The source is compiled as UTF-8, and so are the file names it reports.
    wchar_t wide[1024];
    const int widened =
        ::MultiByteToWideChar(CP_UTF8, 0, line, used, wide, static_cast<int>(std::size(wide)) - 1);
    wide[widened > 0 ? widened : 0] = L'\0';
    ::OutputDebugStringW(wide);
}

/// The line, then the stop: right there under a debugger, an abort otherwise.
[[noreturn]] void stop(std::source_location where, std::string_view prefix, std::string_view what,
                       std::string_view suffix) noexcept {
    write_line(where, prefix, what, suffix);

    if (::IsDebuggerPresent()) __debugbreak();

    std::abort();
}

}  // namespace

[[noreturn]]
void abort(std::string_view why, std::source_location where) noexcept {
    stop(where, {}, why, {});
}

void report(std::string_view what, std::source_location where) noexcept {
    write_line(where, {}, what, {});
}

[[noreturn]]
void fail(std::string_view cond_str, std::source_location loc) {
    stop(loc, "Precondition ", cond_str, " failed");
}

// ---- What std::terminate says --------------------------------------------------------

namespace impl {

namespace {

/// Set by the layer that knows the type, before the threads it is to serve start, and
/// read by the handler on whichever thread terminates.
std::atomic<exception_namer> foreign_exception_namer{nullptr};

}  // namespace

void terminate_report::put(const std::string_view part) noexcept {
    if (cut_) return;

    const std::size_t taken = std::min(part.size(), text_.size() - used_);
    std::copy_n(part.data(), taken, text_.data() + used_);
    used_ += taken;

    if (taken != part.size()) cut();
}

void terminate_report::put(const std::wstring_view part) noexcept {
    if (cut_) return;

    const auto done = unicode::write_utf8_repaired(text_.subspan(used_), part);
    used_ += done.written;

    if (done.read != part.size()) cut();
}

void terminate_report::cut() noexcept {
    // The dots take the last three bytes of the room, moved back to the start of the code
    // point they would split.
    const std::size_t end =
        unicode::floor_code_point_boundary(std::string_view{text_.data(), used_}, text_.size() - 3);
    std::copy_n("...", 3, text_.data() + end);
    used_ = end + 3;
    cut_ = true;
}

void set_exception_namer(const exception_namer namer) noexcept {
    if (foreign_exception_namer.exchange(namer, std::memory_order_acq_rel) != nullptr)
        core::abort("wxl: an exception namer is set already; it is set once in a process");
}

void describe_terminate(terminate_report& report) noexcept {
    if (!std::current_exception()) {
        report.put("wxl: std::terminate without an exception");
        return;
    }

    try {
        throw;
    } catch (const std::exception& error) {
        report.put("wxl: unhandled exception: ");
        report.put(std::string_view{error.what()});
    } catch (...) {
        const exception_namer namer = foreign_exception_namer.load(std::memory_order_acquire);

        if (!namer || !namer(report)) report.put("wxl: unhandled exception of an unknown type");
    }
}

// Smaller than the line of stop(), which adds the place in front of it.
[[noreturn]] void terminate_with_reason() noexcept {
    thread_local bool describing = false;
    if (describing) core::abort("wxl: std::terminate again while its exception was being described");
    describing = true;

    char text[512];
    terminate_report report{text};
    describe_terminate(report);
    core::abort(report.text());
}

std::terminate_handler set_terminate_with_reason() noexcept {
    return std::set_terminate(&terminate_with_reason);
}

[[noreturn]] void terminate_with_exception(const std::exception_ptr error) noexcept {
    static_cast<void>(set_terminate_with_reason());

    if (error) {
        try {
            std::rethrow_exception(error);
        } catch (...) {
            std::terminate();
        }
    }

    std::terminate();
}

}  // namespace impl

}  // namespace wxl::core
