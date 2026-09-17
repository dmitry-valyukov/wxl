export module wxl.core:module_cleanup;

import :noncopyable;
import std;

export namespace wxl::core {

/// Registers a callback that runs once, when run_now() is called. An instance stays
/// registered until then, so it has to outlive that call.
///
/// Callbacks are grouped into three ordering categories (first/normal/last); every
/// callback in one category runs, in LIFO registration order, before any callback in
/// the next category runs.
class module_cleanup : noncopyable
{
public:
    using cleanup_func = void();
    using cleanup_func1 = void(void*);

    enum class priority : int
    {
        cleanup_first = -1,
        cleanup_normal = 0,
        cleanup_last = 1
    };

    using enum priority;

    inline explicit module_cleanup(cleanup_func* func, priority p = cleanup_normal) {
        init(func, nullptr, nullptr, p);
    }

    template <typename t_arg>
    module_cleanup(void (*func)(t_arg*), t_arg* arg, priority p = cleanup_normal) {
        init(nullptr, reinterpret_cast<cleanup_func1*>(func), arg, p);
    }

    inline static bool process_is_terminating() {
        return s_process_is_terminating.load(std::memory_order_acquire);
    }

    /// Runs every registered cleanup now, in priority order (first, then
    /// normal, then last), exactly once for the process -- later calls do
    /// nothing. After it returns the shutdown latch is set, so a cleanup
    /// registered afterwards runs at once on registration instead of being
    /// deferred. A wxl application calls it in one place, at the end of
    /// wWinMain, after its Teardown handler; a program that never calls it
    /// never runs its cleanups.
    inline static void run_now() noexcept { execute_at_exit(); }

private:
    void init(cleanup_func* f, cleanup_func1* f1, void* arg, priority p);
    void call_cleanup() const;

    static void execute_at_exit();

    static std::atomic<bool> s_process_is_terminating;

    cleanup_func* cleanup_func_ = nullptr;
    cleanup_func1* cleanup_func1_ = nullptr;
    void* arg_ = nullptr;
    std::atomic<module_cleanup*> prev_{nullptr};
};

}  // export namespace wxl::core
