module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

namespace {

// The bodies of the operations, each made once and handed to the form without a token and
// to the form with one: the two differ in nothing else. Those that make the object itself
// are members: its constructor is private.

auto listing_all(const core::path& pattern) {
    return [pattern](const orphan_stage& stage) {
        std::vector<async_directory::listed_entry> found;

        core::directory listing = core::directory::open(pattern.c_str());

        if (!listing.opened()) {
            // Nothing matched in a directory that is there: FindFirstFileExW
            // calls that a file not found, and for a listing it is an empty one.
            if (::GetLastError() == ERROR_FILE_NOT_FOUND) return found;

            throw system_exception("FindFirstFileExW");
        }

        // A listing that stops halfway is no answer: a body cut short between two names
        // answers the cancellation, and so does one whose FindNextFileW was cut short,
        // which ends the listing the way its end does and is told apart by its code.
        for (async_directory::entry next; listing.next(next);) {
            if (stage.cut_short()) throw operation_canceled_exception();

            found.push_back({std::wstring(next.name), next.is_directory, next.size});
        }

        if (::GetLastError() == ERROR_OPERATION_ABORTED) throw operation_canceled_exception();

        return found;
    };
}

auto asking_exists(const core::path& path) {
    return [path] {
        // `false` is the system's answer, or a call cut short, which answers the
        // cancellation: the code tells them apart, cleared first, since a call that
        // succeeds leaves the last one's.
        ::SetLastError(ERROR_SUCCESS);

        const bool there = core::directory::exists(path.c_str());

        if (!there && ::GetLastError() == ERROR_OPERATION_ABORTED)
            throw operation_canceled_exception();

        return there;
    };
}

auto making_all(const core::path& p) {
    return [copy = p](const orphan_stage& stage) mutable {
        // create_all() answers `false` for a no and for a failure alike: the no is
        // remembered to tell the two apart.
        bool stopped = false;
        const auto wanted = [&stage, &stopped] {
            stopped = stage.cut_short();
            return !stopped;
        };

        if (!core::directory::create_all(copy, wanted)) {
            if (stopped) throw operation_canceled_exception();

            throw system_exception("CreateDirectoryW");
        }
    };
}

auto removing(const core::path& path) {
    return [path] {
        if (!core::directory::remove(path.c_str())) throw system_exception("RemoveDirectoryW");
    };
}

}  // namespace

auto async_directory::opening(const core::path& pattern) {
    return [pattern] {
        core::directory opened = core::directory::open(pattern.c_str());

        if (!opened.opened()) throw system_exception("FindFirstFileExW");

        return async_directory(std::move(opened));
    };
}

auto async_directory::next_entry() {
    return [this]() -> std::optional<entry> {
        entry found;

        if (!directory_.next(found)) return std::nullopt;

        return found;
    };
}

task<async_directory> async_directory::open(const core::path& pattern) {
    return sta_loop::async_call(orphanable, opening(pattern));
}

task<async_directory> async_directory::open(const core::path& pattern, cancellation_token stop) {
    return cancellation_detail::call_under(orphanable, opening(pattern), std::move(stop));
}

task<std::vector<async_directory::listed_entry>> async_directory::list(const core::path& pattern) {
    return sta_loop::async_call(orphanable, listing_all(pattern));
}

task<std::vector<async_directory::listed_entry>> async_directory::list(const core::path& pattern,
                                                                       cancellation_token stop) {
    return cancellation_detail::call_under(orphanable, listing_all(pattern), std::move(stop));
}

task<std::optional<async_directory::entry>> async_directory::next() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call(next_entry());
}

task<std::optional<async_directory::entry>> async_directory::next(cancellation_token stop) {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return cancellation_detail::call_under(next_entry(), std::move(stop));
}

task<> async_directory::close() {
    ensure(directory_.opened() && "async_directory: no listing was opened");

    return sta_loop::async_call([this] { directory_.close(); });
}

task<bool> async_directory::exists(const core::path& path) {
    return sta_loop::async_call(orphanable, asking_exists(path));
}

task<bool> async_directory::exists(const core::path& path, cancellation_token stop) {
    return cancellation_detail::call_under(orphanable, asking_exists(path), std::move(stop));
}

task<> async_directory::create_all(const core::path& p) {
    return sta_loop::async_call(orphanable, making_all(p));
}

task<> async_directory::create_all(const core::path& p, cancellation_token stop) {
    return cancellation_detail::call_under(orphanable, making_all(p), std::move(stop));
}

task<> async_directory::remove(const core::path& path) {
    return sta_loop::async_call(orphanable, removing(path));
}

task<> async_directory::remove(const core::path& path, cancellation_token stop) {
    return cancellation_detail::call_under(orphanable, removing(path), std::move(stop));
}

}  // namespace wxl::async
