module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

awaitable<async_directory> async_directory::open(const core::path& pattern) {
    return sta_loop::async_call([pattern] {
        core::directory opened = core::directory::open(pattern.c_str());

        if (!opened.opened()) throw system_exception("FindFirstFileExW");

        return async_directory(std::move(opened));
    });
}

awaitable<std::optional<async_directory::entry>> async_directory::next() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call([this]() -> std::optional<entry> {
        entry found;

        if (!directory_.next(found)) return std::nullopt;

        return found;
    });
}

awaitable<void> async_directory::close() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call([this] { directory_.close(); });
}

awaitable<bool> async_directory::exists(const core::path& path) {
    return sta_loop::async_call(
        [path] { return core::directory::exists(path.c_str()); });
}

awaitable<void> async_directory::create_all(const core::path& p) {
    return sta_loop::async_call([copy = p]() mutable {
        if (!core::directory::create_all(copy)) throw system_exception("CreateDirectoryW");
    });
}

awaitable<void> async_directory::remove(const core::path& path) {
    return sta_loop::async_call([path] {
        if (!core::directory::remove(path.c_str())) throw system_exception("RemoveDirectoryW");
    });
}

}  // namespace wxl::async
