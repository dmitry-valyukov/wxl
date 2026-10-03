// Do the loop's completions keep arriving while a continuation stands in a modal loop?
// One coroutine reads a file and, on its first read's continuation, opens a MessageBox
// with no owner window; a second coroutine keeps reading meanwhile and counts what it
// got while the box was up. A helper thread closes the box after a while. A probe: it
// answers into a file and exits by itself.
#include "platform.h"

#include <stdio.h>

#include "ui.h"
#include "launch.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr wchar_t box_title[] = L"modal loop probe";
constexpr DWORD box_open_ms = 1500;

std::atomic<bool> box_is_up{false};
int reads_while_box_up = 0;
bool second_finished = false;

void report(const char* text) {
    FILE* out = nullptr;

    if (fopen_s(&out, "modal-loop-probe.txt", "a") == 0) {
        fputs(text, out);
        fclose(out);
    }
}

core::path own_path() {
    wchar_t self[MAX_PATH];
    ::GetModuleFileNameW(nullptr, self, MAX_PATH);

    return core::path(self);
}

/// Reads the whole file, counting the reads that came back while the box was up.
async::detached_task reads_all_the_time() {
    try {
        async::async_file file = co_await async::async_file::open_read(own_path());
        std::byte buffer[4096];

        while (co_await file.read(buffer) != 0)
            if (box_is_up.load()) ++reads_while_box_up;

        second_finished = true;
    } catch (const std::exception& failure) {
        char line[256];

        sprintf_s(line, "second reader failed: %s\n", failure.what());
        report(line);
    }
}

/// Closes the box after it has been up for a while: the probe runs unattended.
void closes_the_box() {
    std::thread([] {
        ::Sleep(box_open_ms);

        if (const HWND box = ::FindWindowW(nullptr, box_title)) ::PostMessageW(box, WM_CLOSE, 0, 0);
    }).detach();
}

async::detached_task opens_a_box_in_its_continuation() {
    char line[256];

    try {
        async::async_file file = co_await async::async_file::open_read(own_path());
        std::byte buffer[4096];

        co_await file.read(buffer);

        // Resumed inside the loop's drain: the modal loop below runs nested in it.
        reads_all_the_time();
        closes_the_box();

        box_is_up = true;
        ::MessageBoxW(nullptr, L"reading behind this box", box_title, MB_OK);
        box_is_up = false;

        sprintf_s(line, "reads delivered while the box was up: %d\n", reads_while_box_up);
        report(line);
        report(reads_while_box_up > 0 ? "completions reach a modal loop: yes\n"
                                      : "completions reach a modal loop: NO\n");

        while (!second_finished) co_await file.read(buffer);
    } catch (const std::exception& failure) {
        sprintf_s(line, "failed: %s\n", failure.what());
        report(line);
    }

    report("done\n");
    ::PostQuitMessage(0);
}

}  // namespace

wxl::Teardown wxl_launched() {
    ::DeleteFileW(L"modal-loop-probe.txt");

    auto window = Window {
        title = L"modal loop probe",
        Grid {
            TextBlock {text = L"probing", hAlign.center, vAlign.center},
        },
    };

    window.activate();

    opens_a_box_in_its_continuation();

    return {};
}
