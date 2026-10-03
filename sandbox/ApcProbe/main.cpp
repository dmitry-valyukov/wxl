// Does the loop Application::Start runs deliver user APCs to the GUI thread?
// A probe, not a sample: it answers one question and exits by itself.
#include "platform.h"

#include <stdio.h>

#include "ui.h"
#include "launch.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

using probe_clock = std::chrono::steady_clock;

std::atomic<long long> apc_ran_at{-1};
std::atomic<long long> read_done_at{-1};
std::atomic<int> apcs_ran{0};
probe_clock::time_point started;

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(probe_clock::now() - started)
        .count();
}

void __stdcall on_apc(ULONG_PTR) {
    apcs_ran.fetch_add(1);
    long long expected = -1;
    apc_ran_at.compare_exchange_strong(expected, now_ms());
}

OVERLAPPED overlapped{};
char buffer[4096];

void __stdcall on_read_done(DWORD, DWORD, OVERLAPPED*) {
    read_done_at.store(now_ms());
}

}  // namespace

wxl::Teardown wxl_launched() {
    started = probe_clock::now();

    auto window = Window {
        title = L"APC probe",
        Grid {
            TextBlock {text = L"probing", hAlign.center, vAlign.center},
        },
    };

    window.activate();

    HANDLE gui = nullptr;
    ::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(), &gui, 0,
                      FALSE, DUPLICATE_SAME_ACCESS);

    // A read started on the GUI thread itself: its completion routine can only run here.
    wchar_t self[MAX_PATH];
    ::GetModuleFileNameW(nullptr, self, MAX_PATH);
    const HANDLE file = ::CreateFileW(self, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_OVERLAPPED, nullptr);
    const BOOL read_started = ::ReadFileEx(file, buffer, sizeof(buffer), &overlapped, on_read_done);
    const long long read_started_at = now_ms();

    std::thread([gui, read_started, read_started_at] {
        std::FILE* out = nullptr;
        _wfopen_s(&out, L"apc-probe.txt", L"w");

        std::fprintf(out, "ReadFileEx on the GUI thread: started=%d at %lld ms\n", read_started,
                     read_started_at);

        // Let the window come up and the loop go idle.
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::fprintf(out, "after 2000 ms idle: read completion ran at %lld ms (-1 = never)\n",
                     read_done_at.load());

        for (int i = 0; i < 3; ++i) {
            const long long queued = now_ms();
            const DWORD ok = ::QueueUserAPC(on_apc, gui, 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            std::fprintf(out, "APC %d: queued=%lu at %lld ms, ran so far=%d, first ran at %lld ms\n",
                         i, ok, queued, apcs_ran.load(), apc_ran_at.load());
        }

        std::fprintf(out, "final: apcs ran=%d, read completion at %lld ms\n", apcs_ran.load(),
                     read_done_at.load());
        std::fclose(out);
        ::ExitProcess(0);
    }).detach();

    return {};
}
