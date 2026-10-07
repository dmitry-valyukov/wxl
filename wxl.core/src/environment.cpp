module;
#include "pch.h"

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
