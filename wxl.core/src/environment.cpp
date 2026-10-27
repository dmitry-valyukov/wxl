module;
#include "pch.h"

#include <shellapi.h>  // CommandLineToArgvW -- WIN32_LEAN_AND_MEAN keeps it out of windows.h

module wxl.core;
import std;

using namespace wxl::core;

namespace {

// The system's words for the code. Not std::error_code::message(): MSVC's STL takes them
// from FormatMessageA, in the ANSI code page. A code of another category -- the generic
// one, iostream's -- is rare here, the file system reporting the system's own codes; MSVC
// words those from its own table, and they are repaired all the same, since nothing
// promises that table is ASCII.
u8_text explanation(std::error_code const& code) {
    if (code.category() == std::system_category()) {
        auto words = environment::error_explanation(static_cast<std::uint32_t>(code.value()));
        if (!words.empty()) return words;
    }
    return unicode::repaired(code.message());
}

}  // namespace

size_t environment::processor_count() {
    SYSTEM_INFO sysinfo;
    ::GetSystemInfo(&sysinfo);

    return sysinfo.dwNumberOfProcessors;
}

std::filesystem::path environment::application_folder() {
    // MAX_PATH is not a limit here: the buffer grows until the name fits.
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        auto const written =
            ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (written < path.size()) {
            path.resize(written);
            break;
        }
        path.resize(path.size() * 2);
    }
    return std::filesystem::path{path}.parent_path();
}

sta_vector<sta_wstring> environment::command_line() {
    // What CommandLineToArgvW hands out is one LocalAlloc block, the pointers and the
    // strings together, freed by one call; unique_ptr calls this for a non-null pointer only.
    struct local_free_deleter {
        void operator()(LPWSTR* ptr) const noexcept { ::LocalFree(ptr); }
    };

    int argc = 0;
    std::unique_ptr<LPWSTR[], local_free_deleter> argv{::CommandLineToArgvW(::GetCommandLineW(), &argc),
                                                       local_free_deleter{}};

    if (!argv) {
        throw std::system_error{static_cast<int>(::GetLastError()), std::system_category(),
                                "CommandLineToArgvW"};
    }

    sta_vector<sta_wstring> result;
    result.reserve(static_cast<size_t>(argc));

    for (int i = 0; i < argc; ++i) result.emplace_back(argv[i]);

    return result;
}

std::wstring environment::variable(wchar_t const* const name) {
    // Asked with no room, the call answers with the room the value needs, its terminator
    // included; given that room, with the length it wrote. Zero is a value of no characters
    // and a failure alike, and only the last error tells them apart -- cleared before each
    // call, so that one left by an earlier call decides nothing. A length that does not fit
    // the room is a value another thread lengthened in between, and the size is asked again.
    auto const empty_unless_failed = [] {
        DWORD const error = ::GetLastError();
        if (error != ERROR_SUCCESS && error != ERROR_ENVVAR_NOT_FOUND) {
            throw std::system_error{static_cast<int>(error), std::system_category(),
                                    "GetEnvironmentVariableW"};
        }
        return std::wstring{};
    };

    std::wstring value;
    for (;;) {
        ::SetLastError(ERROR_SUCCESS);
        DWORD const room = ::GetEnvironmentVariableW(name, nullptr, 0);
        if (room == 0) return empty_unless_failed();
        if (room == 1) return {};

        value.resize(room);
        ::SetLastError(ERROR_SUCCESS);
        DWORD const length = ::GetEnvironmentVariableW(name, value.data(), room);
        if (length == 0) return empty_unless_failed();
        if (length < room) {
            value.resize(length);
            return value;
        }
    }
}

u8_text environment::error_explanation(std::uint32_t const code) {
    // Language 0 is the system's own order of search: the thread's language, the user's, the
    // system's, English. An insert in the text (%1) has nothing to fill it, and without
    // IGNORE_INSERTS such a phrase is not found at all. An HRESULT is explained the same way.
    wchar_t* buffer = nullptr;
    DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::unique_ptr<wchar_t, decltype(&::LocalFree)> const owned{buffer, &::LocalFree};

    if (!length || !buffer) return {};

    // The system ends the phrase with a line break, some with a space before it; a reader is
    // shown the phrase.
    while (length && (buffer[length - 1] == L'\n' || buffer[length - 1] == L'\r' ||
                      buffer[length - 1] == L' ')) {
        --length;
    }
    // The system's own text, but nothing promises it is well-formed UTF-16.
    return unicode::repaired(std::wstring_view{buffer, length}).to_utf8();
}

u8_text environment::error_message(std::exception const& error) {
    auto const* const failed = dynamic_cast<std::filesystem::filesystem_error const*>(&error);
    if (!failed) return unicode::repaired(error.what());

    u8_text message;
    for (std::filesystem::path const* const path : {&failed->path1(), &failed->path2()}) {
        if (path->empty()) continue;
        if (!message.empty()) message += u8", ";
        message += unicode::to_utf8(*path);
    }
    if (!message.empty()) message += u8": ";
    message += explanation(failed->code());
    return message;
}
