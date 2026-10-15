module;
#include "pch.h"

#include <shellapi.h>  // CommandLineToArgvW -- WIN32_LEAN_AND_MEAN keeps it out of windows.h

module wxl.core;
import std;

using namespace wxl::core;

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
