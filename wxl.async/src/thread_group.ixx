export module wxl.async:thread_group;

import :future;
import wxl.core;
import std;

export namespace wxl::async {

class thread_group;

/// Smart-pointer for thread_group.
using thread_group_ptr = core::intrusive_ptr<thread_group>;

/// A set of joinable ("managed") threads: work is spawned into the group and the group can be
/// waited on as a whole, or closed, which refuses further spawns and waits.
///
/// It doubles as the static API for free ("detached") threads that belong to no group; those
/// are waited on through the separate join_detached_threads() mechanism.
class thread_group : public core::refcounted_mt
{
public:
    using thread_proc = std::function<void()>;

    /// thread_group can be created only on the heap and its lifetime should be managed by
    /// thread_group_ptr.
    inline static thread_group_ptr create() { return new thread_group(); }

    /// \return ID of the main thread.
    static core::thread_id main_thread_id();

    /// @{
    /// Registers the cleanup function for the current thread.
    ///
    /// @throw an exception if cannot register the cleanup function (for example for externally
    /// created thread).
    inline static void at_thread_exit(void (*cleanup_func)(void)) {
        at_thread_exit_impl(cleanup_func, nullptr, nullptr);
    }

    template <typename t_arg>
    static void at_thread_exit(void (*cleanup_func)(void*), t_arg* arg) {
        at_thread_exit_impl(nullptr, cleanup_func, arg);
    }
    /// @}

    /// \return True if thread was created by thread_group.
    static bool is_internal_thread();

    /// @{ Starts a new thread with thread_proc as its entry.
    ///
    /// \return a future that becomes ready once the thread body has finished, carrying the
    ///         exception it threw if it threw one -- the body's only way out, since an
    ///         exception cannot cross a thread boundary by itself. Ignoring the future is
    ///         allowed, and then the exception is dropped with it.
    inline future<void> spawn(const thread_proc& thread_proc) {
        return spawn_named({}, thread_proc);
    }

    /// \param thread_name is also the debugger-visible name of the OS thread. Checked text,
    ///        so that a name nobody has vouched for is refused here rather than inside the
    ///        thread that was started with it: u8"reader io" needs no call at all, and a name
    ///        built at run time goes through wxl::core::checked() where it is built.
    future<void> spawn_named(core::u8_view thread_name, const thread_proc& thread_proc);
    /// @}

    /// @{ Starts a new detached (background) thread.
    ///
    /// @warning Is NOT recommended to use.
    inline static future<void> spawn_detached(const thread_proc& thread_proc) {
        return spawn_detached_named({}, thread_proc);
    }

    static future<void> spawn_detached_named(core::u8_view thread_name,
                                             const thread_proc& thread_proc);
    /// @}

    /// Returns number of the currently alive detached threads.
    static size_t detached_thread_count() noexcept;

    /// Blocks until every detached thread has finished.
    static void join_detached_threads();

    /// Blocks until every detached thread has finished or \p timeout elapses.
    /// \return \c false only on timeout.
    static bool join_detached_threads_for(core::duration timeout);

    /// Joins all spawned managed threads. Throws an exception if called from one of the spawned
    /// threads.
    void join_all();

    /// Should be called to notify the thread_group instance that it will never be used again.
    ///
    /// It implicitly calls join_all().
    ///
    /// Any attempt to call spawn() after this call will throw.
    void close();

    /// Packages `func` together with its bound arguments into a thread_proc. Replaces the
    /// bind calls the fixed-arity overloads below used to make; the arguments are
    /// decay-copied into the returned callable, as that binder did.
    template <typename t_func, typename... t_args>
    static thread_proc bind_thread_proc(t_func&& func, t_args&&... args) {
        return [f = std::forward<t_func>(func),
                bound = std::tuple<std::decay_t<t_args>...>(std::forward<t_args>(args)...)]() mutable {
            std::apply(f, bound);
        };
    }

    /// @{ spawn/spawn_named with bound arguments. One variadic template each replaces the
    /// C++03 fixed-arity (1..4 argument) overload sets. At least one bound argument is
    /// required, so a plain callable still goes to the thread_proc overload above.
    template <typename t_func, typename t_arg, typename... t_args>
        requires std::invocable<t_func&, t_arg&, t_args&...>
    future<void> spawn(t_func&& func, t_arg&& arg, t_args&&... args) {
        return spawn(bind_thread_proc(std::forward<t_func>(func), std::forward<t_arg>(arg),
                                      std::forward<t_args>(args)...));
    }

    template <typename t_func, typename t_arg, typename... t_args>
        requires std::invocable<t_func&, t_arg&, t_args&...>
    future<void> spawn_named(core::u8_view thread_name, t_func&& func, t_arg&& arg,
                             t_args&&... args) {
        return spawn_named(thread_name, bind_thread_proc(std::forward<t_func>(func),
                                                         std::forward<t_arg>(arg),
                                                         std::forward<t_args>(args)...));
    }
    /// @}

    /// @{ spawn_detached/spawn_detached_named with bound arguments.
    template <typename t_func, typename t_arg, typename... t_args>
        requires std::invocable<t_func&, t_arg&, t_args&...>
    static future<void> spawn_detached(t_func&& func, t_arg&& arg, t_args&&... args) {
        return spawn_detached(bind_thread_proc(std::forward<t_func>(func),
                                               std::forward<t_arg>(arg),
                                               std::forward<t_args>(args)...));
    }

    template <typename t_func, typename t_arg, typename... t_args>
        requires std::invocable<t_func&, t_arg&, t_args&...>
    static future<void> spawn_detached_named(core::u8_view thread_name, t_func&& func,
                                             t_arg&& arg, t_args&&... args) {
        return spawn_detached_named(thread_name, bind_thread_proc(std::forward<t_func>(func),
                                                                  std::forward<t_arg>(arg),
                                                                  std::forward<t_args>(args)...));
    }
    /// @}

    using cleanup_func = void (*)();
    using cleanup_func1 = void (*)(void*);

protected:
    ~thread_group() override;

private:
    thread_group();

    static void at_thread_exit_impl(cleanup_func f, cleanup_func1 f1, void* arg);

    class impl;
    std::unique_ptr<impl> const impl_;

    friend class thread_starter;
};

/// Owns a thread_group for the length of a block: whatever is spawned into it is joined when
/// the scope closes, and no spawn can outlive it.
class thread_scope : public core::noncopyable
{
public:
    using thread_proc = thread_group::thread_proc;

    inline thread_scope() : group_(thread_group::create()) {}

    ~thread_scope();

    inline thread_group* get() { return group_.get(); }

    inline thread_group* operator->() { return group_.get(); }

    inline operator thread_group*() { return group_.get(); }

private:
    thread_group_ptr group_;
};

/// One thread, started by the constructor and joined by the destructor -- the reason it needs
/// no join() call of its own, and what the "joining" in the name is about.
class joining_thread : public core::noncopyable
{
public:
    template <typename t_func, typename... t_args>
        requires std::invocable<t_func&, t_args&...>
    explicit joining_thread(t_func&& func, t_args&&... args)
        : scope_(), result_(scope_->spawn(std::forward<t_func>(func),
                                          std::forward<t_args>(args)...)) {}

    /// Waits for the thread here rather than at the end of the scope.
    inline void join() { scope_->join_all(); }

    /// Ready once the thread body has finished; carries the exception it threw, if any.
    inline const future<void>& result() const noexcept { return result_; }

private:
    thread_scope scope_;
    future<void> result_;
};

}  // export namespace wxl::async
