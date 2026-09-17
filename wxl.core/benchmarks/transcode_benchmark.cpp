// UTF-8 to UTF-16 and back: wxl.core against the Windows call the tree used
// to make in three places.
//
// The question is whether writing the conversion by hand is worth anything
// over MultiByteToWideChar. Both do the same two passes -- measure, then
// convert -- so what is being compared is the loop, not the shape.
//
// Three texts, because the answer depends on the alphabet: English, where
// every byte is one unit; Russian, where every letter is two bytes; and one
// with characters above the basic plane, where a pair has to be built.

// Before windows.h, or its min/max macros eat std::numeric_limits below.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

import wxl.core;

namespace {

using clock_t_ = std::chrono::steady_clock;

std::wstring windows_to_utf16(std::string_view utf8) {
    if (utf8.empty()) return {};

    const int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                           nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(),
                          size);

    return result;
}

std::string windows_to_utf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), result.data(),
                          size, nullptr, nullptr);

    return result;
}

std::string repeated(std::string_view piece, std::size_t times) {
    std::string text;
    text.reserve(piece.size() * times);
    for (std::size_t at = 0; at != times; ++at) text += piece;
    return text;
}

/// The best of several passes, not the average of them. What is being timed
/// takes the same work every pass, so anything above the fastest one is the
/// machine getting in the way -- a scheduler, another program, the clock
/// changing speed. Averaging keeps that noise in the answer; the minimum is
/// the closest to what the code actually costs.
template <typename Run>
double milliseconds(unsigned rounds, Run run) {
    constexpr unsigned passes = 5;

    double best = std::numeric_limits<double>::max();

    for (unsigned pass = 0; pass != passes; ++pass) {
        const auto started = clock_t_::now();

        for (unsigned round = 0; round != rounds; ++round) run();

        const auto took = clock_t_::now() - started;
        const double milli = std::chrono::duration<double, std::milli>(took).count();

        if (milli < best) best = milli;
    }

    return best;
}

void compare(std::string_view name, std::string_view utf8, unsigned rounds) {
    // The text is built by the generator above, so it is UTF-8 by
    // construction; what is being timed is the conversion, not the check, and
    // the check is timed separately below.
    const wxl::core::u8_view checked = wxl::core::unicode::assume_valid(utf8);
    const wxl::core::u16_text utf16_owned = checked.to_utf16();
    const std::wstring utf16(utf16_owned.wchars());
    const wxl::core::u16_view checked_wide = utf16_owned;

    std::size_t sink = 0;

    const double ours_wide = milliseconds(rounds, [&] { sink += checked.to_utf16().size(); });
    const double windows_wide = milliseconds(rounds, [&] { sink += windows_to_utf16(utf8).size(); });
    const double ours_narrow = milliseconds(rounds, [&] { sink += checked_wide.to_utf8().size(); });
    const double windows_narrow =
        milliseconds(rounds, [&] { sink += windows_to_utf8(utf16).size(); });

    const double checking = milliseconds(rounds, [&] { sink += wxl::core::unicode::is_valid_utf8(utf8); });

    std::printf("%-10s %9zu bytes  x%u\n", std::string(name).c_str(), utf8.size(), rounds);
    std::printf("    to utf16   wxl.core %8.2f ms   windows %8.2f ms   %5.2fx\n", ours_wide,
                windows_wide, windows_wide / ours_wide);
    std::printf("    to utf8    wxl.core %8.2f ms   windows %8.2f ms   %5.2fx\n", ours_narrow,
                windows_narrow, windows_narrow / ours_narrow);
    std::printf("    validating          %8.2f ms\n", checking);

    if (sink == 0) std::printf("");  // keeps the work from being optimized away
}

}  // namespace

int main() {
    const std::string english = repeated("The quick brown fox jumps over the lazy dog. ", 2000);
    const std::string russian = repeated("Съешь ещё этих мягких французских булок. ", 2000);
    const std::string wide = repeated("Съешь 😀 ещё 字 этих мягких булок. ", 2000);

    constexpr unsigned rounds = 100;

    compare("english", english, rounds);
    compare("russian", russian, rounds);
    compare("mixed", wide, rounds);

    return 0;
}
