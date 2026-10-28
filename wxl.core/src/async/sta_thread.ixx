export module wxl.core:async.sta_thread;

import :checks;
import :sta_allocator;
import std;

export namespace wxl::async {

/// The STA thread: the one thread wxl's single-threaded machinery belongs to -- the thread
/// the coroutines live on, the STA side of `sta_loop` runs on, and `core::sta_memory_pool`
/// serves. It is the thread the pool is built on, during the program's static
/// initialization, so every program has it, with a GUI or without: an application with no
/// window, a tool, a test binary. A wxl application brings XAML up on it, and there it is
/// the UI thread.
///
/// **There is nothing here to build.** The thread is one and it is the process's, so the type
/// names it through static members alone, as `sta_loop` names its loop.
class sta_thread
{
public:
    /// Static from top to bottom, and therefore never made.
    sta_thread() = delete;

    /// Whether the calling thread is the STA thread. False on every thread before the pool is
    /// built and after it is gone.
    inline static bool is_current() noexcept { return core::sta_memory_pool::is_safe(); }

    /// Stops the process through `core::abort` when the calling thread is not the STA thread:
    /// a call from another thread into what belongs to the STA thread is a broken rule. The
    /// check is in every build, and the report names the caller's line.
    inline static void ensure_current(
        std::source_location where = std::source_location::current()) noexcept {
        if (!is_current()) [[unlikely]]
            core::abort("called from a thread other than the STA thread", where);
    }
};

}  // export namespace wxl::async
