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
[[noreturn]] void stop(std::source_location where, std::string_view prefix, std::string_view what,
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

    if (::IsDebuggerPresent()) __debugbreak();

    std::abort();
}

}  // namespace

[[noreturn]]
void abort(std::string_view why, std::source_location where) noexcept {
    stop(where, {}, why, {});
}

[[noreturn]]
void fail(std::string_view cond_str, std::source_location loc) {
    stop(loc, "Precondition ", cond_str, " failed");
}

}  // namespace wxl::core
