module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

async_file::async_file(core::file&& opened) : file_(std::move(opened)) {
    skips_port_ = sta_loop::port().attach(file_.native_handle());

    // A pipe has no attributes to give, and nothing of the kind to be asked about.
    if (FILE_BASIC_INFO info{}; ::GetFileInformationByHandleEx(file_.native_handle(), FileBasicInfo,
                                                               &info, sizeof(info)))
        reads_hold_the_caller_ =
            (info.FileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED)) != 0;
}

awaitable<async_file> async_file::open_read(const core::path& path) {
    return sta_loop::async_call(orphanable, [path] {
        core::file opened = core::file::open_read_overlapped(path.c_str());

        if (!opened.opened()) throw system_exception("CreateFileW");

        return async_file(std::move(opened));
    });
}

awaitable<async_file> async_file::create(const core::path& path) {
    return sta_loop::async_call(orphanable, [path] {
        core::file created = core::file::create_overlapped(path.c_str());

        if (!created.opened()) throw system_exception("CreateFileW");

        return async_file(std::move(created));
    });
}

awaitable<std::size_t> async_file::read(std::span<std::byte> into) {
    ensure(file_.opened() && "async_file: no file was opened");

    std::unique_ptr<io_op> op(new io_op(io_op::kind::read, file_.native_handle(), into.data(),
                                        into.size(), position_, skips_port_));

    position_ += into.size();

    if (reads_hold_the_caller_ || into.size() > direct_read_limit) [[unlikely]]
        return sta_loop::async_run(std::unique_ptr<async_op_t<std::size_t>>(std::move(op)));

    return sta_loop::async_start(std::move(op));
}

awaitable<std::size_t> async_file::write(std::span<const std::byte> from) {
    ensure(file_.opened() && "async_file: no file was opened");

    std::unique_ptr<async_op_t<std::size_t>> op(
        new io_op(io_op::kind::write, file_.native_handle(), const_cast<std::byte*>(from.data()),
                  from.size(), position_, skips_port_));

    position_ += from.size();

    return sta_loop::async_run(std::move(op));
}

awaitable<std::uint64_t> async_file::size() {
    ensure(file_.opened() && "async_file: no file was opened");

    return sta_loop::call_here([this] {
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

    return sta_loop::call_here([this] { file_.close(); });
}

}  // namespace wxl::async
