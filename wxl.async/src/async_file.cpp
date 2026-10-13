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

awaitable<std::string> async_file::read_all(const core::path& path) {
    return sta_loop::async_call(orphanable, [path](const orphan_stage& stage) {
        core::file source = core::file::open_read(path.c_str());

        if (!source.opened()) throw system_exception("CreateFileW");

        const core::nullable<std::uint64_t> length = source.size();

        if (!length) throw system_exception("GetFileSizeEx");

        std::string bytes(static_cast<std::size_t>(*length), '\0');

        // In pieces, so that a body given up between two of them leaves: the
        // call under way is not cut short, a file read whole is not a pipe, and
        // a piece is as long as the body is uninterruptible. The end of the file
        // is reached early only if the file shrank since it was measured, and
        // reaching it is the one short read that is not a failure.
        constexpr std::size_t piece = 4 * 1024 * 1024;
        std::size_t done = 0;

        ::SetLastError(ERROR_SUCCESS);

        while (done < bytes.size() && !stage.given_up()) {
            const std::size_t got = source.read(std::as_writable_bytes(
                std::span(bytes).subspan(done, std::min(piece, bytes.size() - done))));

            if (got == 0) break;

            done += got;
        }

        if (done < bytes.size()) {
            if (stage.given_up()) return std::string();

            if (const DWORD why = ::GetLastError(); why != ERROR_SUCCESS && why != ERROR_HANDLE_EOF)
                throw system_exception("ReadFile", static_cast<int>(why));

            bytes.resize(done);
        }

        return bytes;
    });
}

awaitable<void> async_file::write_all(const core::path& path, std::string bytes) {
    // All three names are made here, on the thread the pool belongs to: a path
    // built inside the body would be built on whatever thread carries it.
    std::wstring temporary_name(path.native());
    temporary_name += L".tmp";

    return sta_loop::async_call(
        orphanable,
        [target = path, temporary = core::path(std::wstring_view(temporary_name)),
         parent = core::path(path.parent_path()),
         bytes = std::move(bytes)](const orphan_stage& stage) mutable {
            const auto wanted = [&stage] { return !stage.given_up(); };

            core::file out = core::file::create(temporary.c_str());

            // The directories are made only when the file could not be, and the
            // save that finds them in place -- every save but the first -- pays
            // for none of them.
            if (!out.opened() && ::GetLastError() == ERROR_PATH_NOT_FOUND && !parent.empty()) {
                if (!core::directory::create_all(parent, wanted))
                    throw system_exception("CreateDirectoryW");

                if (!wanted()) return;

                out = core::file::create(temporary.c_str());
            }

            if (!out.opened()) throw system_exception("CreateFileW");

            // A temporary left behind by a body given up or failed is deleted,
            // best effort: the next save would overwrite it anyway, and nothing
            // reads it.
            const auto drop = [&] {
                out.close();
                ::DeleteFileW(temporary.c_str());
            };

            if (out.write(std::as_bytes(std::span(bytes))) != bytes.size()) {
                const system_exception failure("WriteFile");
                drop();
                throw failure;
            }

            if (!wanted()) return drop();

            // Pushed to the device before the rename: without that, a power cut
            // could leave the rename done and the content not, and the name
            // would point at zeroes.
            if (!out.flush()) {
                const system_exception failure("FlushFileBuffers");
                drop();
                throw failure;
            }

            out.close();

            if (!wanted()) return (void)::DeleteFileW(temporary.c_str());

            // WRITE_THROUGH, so that the rename itself reaches the disk rather
            // than the cache before this returns.
            if (!::MoveFileExW(temporary.c_str(), target.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const system_exception failure("MoveFileExW");
                ::DeleteFileW(temporary.c_str());
                throw failure;
            }
        });
}

awaitable<bool> async_file::exists(const core::path& path) {
    return sta_loop::async_call(orphanable, [path] { return core::file::exists(path.c_str()); });
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
