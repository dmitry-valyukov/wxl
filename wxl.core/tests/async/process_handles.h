#pragma once

#include "platform.h"

/// The calling thread's own handle, for waiting on its end and asking its id: a thread that a
/// test sees only from the inside opens it there and hands it out.
inline HANDLE open_current_thread() {
    HANDLE self = nullptr;
    ::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(), &self,
                      SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0);
    return self;
}

/// The handles the process has open.
inline long process_handle_count() {
    DWORD count = 0;
    ::GetProcessHandleCount(::GetCurrentProcess(), &count);
    return static_cast<long>(count);
}

/// The handles the process gained over `cycles` calls of `cycle`: what a test counts to see
/// that an object holding a kernel object goes with its last owner -- a thread's handle, the
/// event of a shared state that somebody waited on.
///
/// One call more comes first and is not counted, so that whatever the system or the runtime
/// sets up on first use and keeps is paid for there. Even so the count is the process's own
/// and not exact: anything else in the process may open or close a handle meanwhile. A test
/// therefore compares the gain with the number of cycles, not with zero: an object that every
/// cycle leaves behind holds a handle per cycle, and nothing else comes near that.
template <class F>
long handles_gained(int cycles, F cycle) {
    cycle();

    const long before = process_handle_count();

    for (int i = 0; i < cycles; ++i) cycle();

    return process_handle_count() - before;
}
