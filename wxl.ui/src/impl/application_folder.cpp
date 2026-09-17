#include "application_folder.h"

#include <windows.h>

#include <string>

namespace wxl::impl {

std::filesystem::path application_folder() {
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

std::filesystem::path beside_application(std::filesystem::path const& path) {
    return path.is_absolute() ? path : application_folder() / path;
}

}  // namespace wxl::impl
