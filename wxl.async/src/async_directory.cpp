module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

task<async_directory> async_directory::open(const core::path& pattern) {
    return sta_loop::async_call(orphanable, [pattern] {
        core::directory opened = core::directory::open(pattern.c_str());

        if (!opened.opened()) throw system_exception("FindFirstFileExW");

        return async_directory(std::move(opened));
    });
}

task<std::vector<async_directory::listed_entry>> async_directory::list(const core::path& pattern) {
    return sta_loop::async_call(orphanable, [pattern](const orphan_stage& stage) {
        std::vector<listed_entry> found;

        core::directory listing = core::directory::open(pattern.c_str());

        if (!listing.opened()) {
            // Nothing matched in a directory that is there: FindFirstFileExW
            // calls that a file not found, and for a listing it is an empty one.
            if (::GetLastError() == ERROR_FILE_NOT_FOUND) return found;

            throw system_exception("FindFirstFileExW");
        }

        for (entry next; listing.next(next) && !stage.given_up();)
            found.push_back({std::wstring(next.name), next.is_directory, next.size});

        return found;
    });
}

task<std::optional<async_directory::entry>> async_directory::next() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call([this]() -> std::optional<entry> {
        entry found;

        if (!directory_.next(found)) return std::nullopt;

        return found;
    });
}

task<> async_directory::close() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call([this] { directory_.close(); });
}

task<bool> async_directory::exists(const core::path& path) {
    return sta_loop::async_call(
        orphanable, [path] { return core::directory::exists(path.c_str()); });
}

task<> async_directory::create_all(const core::path& p) {
    return sta_loop::async_call(orphanable, [copy = p](const orphan_stage& stage) mutable {
        const auto wanted = [&stage] { return !stage.given_up(); };

        if (!core::directory::create_all(copy, wanted)) throw system_exception("CreateDirectoryW");
    });
}

task<> async_directory::remove(const core::path& path) {
    return sta_loop::async_call(orphanable, [path] {
        if (!core::directory::remove(path.c_str())) throw system_exception("RemoveDirectoryW");
    });
}

}  // namespace wxl::async
