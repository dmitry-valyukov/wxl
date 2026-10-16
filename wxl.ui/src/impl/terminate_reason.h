#pragma once

// What std::terminate says before the process ends. A noexcept call that an exception
// reaches -- a listener of a window or of a field, the window's own procedure -- ends in
// std::terminate, and the handler the runtime starts with ends the process through abort
// without a word. wWinMain sets the one below first of all, on the thread the windows and
// their listeners run on: it names the exception that got there and stops through
// core::abort, which writes to stderr and to the debugger, and stops right there under a
// debugger.
//
// The exception is still there while the handler runs: once terminate is entered because of a
// throw, the standard counts an implicit handler of it as active ([except.handle]), so
// std::current_exception() hands it over, and rethrowing it inside a try tells its type.
//
// Private: names winrt::hresult_error. Included by launch.cpp and the tests, after the
// projection and ahead of the wxl.core import.

#include <winrt/base.h>

#include "../core.h"

namespace wxl::impl {

// A report put together in a fixed buffer: what does not fit is dropped, and the report says
// it was cut. Nothing is allocated -- the terminate may have come from a std::bad_alloc.
class terminate_report {
public:
    explicit terminate_report(std::span<char> text) noexcept : text_{text} {}

    void put(std::string_view part) noexcept {
        std::size_t const taken = std::min(part.size(), text_.size() - used_);
        std::copy_n(part.data(), taken, text_.data() + used_);
        used_ += taken;
        cut_ = cut_ || taken < part.size();
    }

    // UTF-16, as an hstring holds it, written as UTF-8; a lone surrogate becomes U+FFFD.
    void put(std::wstring_view part) noexcept {
        for (std::size_t at = 0; at < part.size(); ++at) {
            char32_t point = static_cast<char16_t>(part[at]);
            if (point >= 0xD800 && point < 0xDC00 && at + 1 < part.size() &&
                static_cast<char16_t>(part[at + 1]) >= 0xDC00 &&
                static_cast<char16_t>(part[at + 1]) < 0xE000) {
                char32_t const low = static_cast<char16_t>(part[++at]);
                point = 0x10000 + ((point - 0xD800) << 10) + (low - 0xDC00);
            } else if (point >= 0xD800 && point < 0xE000) {
                point = 0xFFFD;
            }
            char units[4];
            put(std::string_view{units, encode(point, units)});
        }
    }

    // The report; a cut one ends in "...", after the last whole code point that fits.
    std::string_view text() noexcept {
        if (!cut_) return {text_.data(), used_};
        std::size_t end = text_.size() - 3;
        while (end > 0 && (static_cast<unsigned char>(text_[end]) & 0xC0) == 0x80) --end;
        std::copy_n("...", 3, text_.data() + end);
        return {text_.data(), end + 3};
    }

private:
    static std::size_t encode(char32_t point, char (&units)[4]) noexcept {
        if (point < 0x80) {
            units[0] = static_cast<char>(point);
            return 1;
        }
        if (point < 0x800) {
            units[0] = static_cast<char>(0xC0 | (point >> 6));
            units[1] = static_cast<char>(0x80 | (point & 0x3F));
            return 2;
        }
        if (point < 0x10000) {
            units[0] = static_cast<char>(0xE0 | (point >> 12));
            units[1] = static_cast<char>(0x80 | ((point >> 6) & 0x3F));
            units[2] = static_cast<char>(0x80 | (point & 0x3F));
            return 3;
        }
        units[0] = static_cast<char>(0xF0 | (point >> 18));
        units[1] = static_cast<char>(0x80 | ((point >> 12) & 0x3F));
        units[2] = static_cast<char>(0x80 | ((point >> 6) & 0x3F));
        units[3] = static_cast<char>(0x80 | (point & 0x3F));
        return 4;
    }

    std::span<char> text_;
    std::size_t used_ = 0;
    bool cut_ = false;
};

// What the handler says when describing the exception brings std::terminate back: set before
// the one step that can -- the message of a winrt::hresult_error -- and cleared after it.
struct terminate_fallback {
    char text[160];
    std::size_t size = 0;
};

inline terminate_fallback& fallback_report() noexcept {
    thread_local terminate_fallback fallback;
    return fallback;
}

// The report of how std::terminate was reached, written into `text` and returned as the
// part of it written; at least four bytes, so that a cut report still has room for "...".
//
// std::current_exception() hands the exception over as a copy at MSVC, and the copy of a
// std::exception copies its string (__std_exception_copy), which allocates: with no memory
// for it the text is lost, and the report says "Unknown exception", what the copy then holds.
//
// A winrt::hresult_error is named by its code and by its message, which for an error of XAML
// or of the composition carries everything the error says. message() is noexcept and builds
// an hstring on the process heap: with no memory for it, its std::bad_alloc meets noexcept and
// brings std::terminate back, and the handler ends with the code alone (fallback_report).
template <std::size_t N>
std::string_view describe_terminate(char (&text)[N]) noexcept {
    static_assert(N >= 4, "wxl: a terminate report needs room for \"...\" after a byte");
    terminate_report report{text};
    std::exception_ptr const current = std::current_exception();
    if (!current) {
        report.put("wxl: std::terminate without an exception");
        return report.text();
    }
    try {
        std::rethrow_exception(current);
    } catch (winrt::hresult_error const& error) {
        char code[16];
        auto const written = std::format_to_n(code, sizeof code, "0x{:08X}",
                                              static_cast<std::uint32_t>(error.code()));
        std::string_view const named{code, static_cast<std::size_t>(written.size)};
        report.put("wxl: unhandled exception: winrt::hresult_error ");
        report.put(named);

        terminate_fallback& fallback = fallback_report();
        terminate_report said{fallback.text};
        said.put("wxl: unhandled exception: winrt::hresult_error ");
        said.put(named);
        said.put(", no memory for its message");
        fallback.size = said.text().size();

        winrt::hstring const message = error.message();
        fallback.size = 0;
        if (!message.empty()) {
            report.put(": ");
            report.put(std::wstring_view{message});
        }
    } catch (std::exception const& error) {
        report.put("wxl: unhandled exception: ");
        report.put(std::string_view{error.what()});
    } catch (...) {
        report.put("wxl: unhandled exception of an unknown type");
    }
    return report.text();
}

// The handler: the report, then the stop. Smaller than core::abort's own line, which adds the
// place in front of it. Entered again from inside -- the message of a winrt::hresult_error
// with no memory left -- it ends with what was set aside for that, and does not recurse.
[[noreturn]] inline void terminate_with_reason() noexcept {
    thread_local bool reporting = false;
    if (reporting) {
        terminate_fallback const& fallback = fallback_report();
        core::abort(fallback.size != 0
                        ? std::string_view{fallback.text, fallback.size}
                        : std::string_view{"wxl: std::terminate again while its exception was "
                                           "being described"});
    }
    reporting = true;
    char text[512];
    core::abort(describe_terminate(text));
}

// Sets terminate_with_reason as the terminate handler -- of the calling thread at least:
// MSVC documents its terminate functions as kept separately for each thread.
inline void set_terminate_with_reason() noexcept {
    static_cast<void>(std::set_terminate(&terminate_with_reason));
}

// Ends the process over `error`, an exception nobody is left to catch -- what a coroutine
// nobody holds does when it fails: terminate is called from inside the catch of the
// rethrown exception, so the handler finds it as the one being handled and names it, once.
[[noreturn]] inline void terminate_with_exception(std::exception_ptr error) noexcept {
    if (error) {
        try {
            std::rethrow_exception(error);
        } catch (...) {
            std::terminate();
        }
    }
    std::terminate();
}

}  // namespace wxl::impl
