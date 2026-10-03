// Does a file opened and read through wxl.async come back to a coroutine of an
// application -- under the loop Application::Start runs, with nothing set up by hand?
// A probe, not a sample: it answers that and exits by itself.
#include "platform.h"

#include <stdio.h>

#include "ui.h"
#include "launch.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

void report(const char* text) {
    FILE* out = nullptr;

    if (fopen_s(&out, "async-file-probe.txt", "a") == 0) {
        fputs(text, out);
        fclose(out);
    }
}

async::detached_task reads_itself() {
    const DWORD gui = ::GetCurrentThreadId();

    wchar_t self[MAX_PATH];
    ::GetModuleFileNameW(nullptr, self, MAX_PATH);

    char line[256];

    try {
        async::async_file file = co_await async::async_file::open_read(core::path(self));

        sprintf_s(line, "opened, back on the thread of the window: %s\n",
                  ::GetCurrentThreadId() == gui ? "yes" : "NO");
        report(line);

        std::byte buffer[4096];
        std::size_t total = 0;
        int reads = 0;

        while (const std::size_t got = co_await file.read(buffer)) {
            total += got;
            ++reads;

            if (::GetCurrentThreadId() != gui) report("a read came back on another thread\n");
        }

        const std::uint64_t size = co_await file.size();

        sprintf_s(line, "read %zu bytes in %d reads, the file is %llu bytes\n", total, reads, size);
        report(line);

        // The directory the probe stands in: its own name without the last component.
        std::wstring_view folder = self;

        folder = folder.substr(0, folder.find_last_of(L'\\'));

        const bool exists = co_await async::async_directory::exists(core::path(folder));

        sprintf_s(line, "its directory is there: %s\n", exists ? "yes" : "NO");
        report(line);
    } catch (const std::exception& failure) {
        sprintf_s(line, "failed: %s\n", failure.what());
        report(line);
    }

    report("done\n");
    ::PostQuitMessage(0);
}

}  // namespace

wxl::Teardown wxl_launched() {
    ::DeleteFileW(L"async-file-probe.txt");

    auto window = Window {
        title = L"async_file probe",
        Grid {
            TextBlock {text = L"probing", hAlign.center, vAlign.center},
        },
    };

    window.activate();

    reads_itself();

    return {};
}
