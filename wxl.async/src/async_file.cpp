module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

awaitable<async_file> async_file::open_read(const core::path& path) {
    return sta_loop::async_call([path] {
        core::file opened = core::file::open_read(path.c_str());

        if (!opened.opened()) throw system_exception("CreateFileW");

        return async_file(std::move(opened));
    });
}

awaitable<async_file> async_file::create(const core::path& path) {
    return sta_loop::async_call([path] {
        core::file created = core::file::create(path.c_str());

        if (!created.opened()) throw system_exception("CreateFileW");

        return async_file(std::move(created));
    });
}

awaitable<std::size_t> async_file::read(std::span<std::byte> into) {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::async_call([this, into] { return file_.read(into); });
}

awaitable<std::size_t> async_file::write(std::span<const std::byte> from) {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::async_call([this, from] {
        const std::size_t done = file_.write(from);

        if (done != from.size()) throw system_exception("WriteFile");

        return done;
    });
}

awaitable<std::uint64_t> async_file::size() {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::async_call([this] {
        const core::nullable<std::uint64_t> length = file_.size();

        if (!length) throw system_exception("GetFileSizeEx");

        return *length;
    });
}

awaitable<void> async_file::flush() {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::async_call([this] {
        if (!file_.flush()) throw system_exception("FlushFileBuffers");
    });
}

awaitable<void> async_file::close() {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::async_call([this] { file_.close(); });
}

}  // namespace wxl::async
