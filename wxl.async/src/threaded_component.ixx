export module wxl.async:threaded_component;

import :component;
import wxl.core;
import :thread_group;
import std;

export namespace wxl::async {

/// Component that runs its own thread.
///
/// Derivatives should override the run() method.
///
/// Disposing of it -- dispose(), which the destructor of the most derived type calls --
/// returns once its thread has exited, not merely left run(): the component says it has
/// stopped from inside the thread, which still has its way out to go, and a process that
/// exits meanwhile would kill the thread on that way. This holds for a detached thread as
/// well as for one in a group, whether or not anybody closes the group. The one exception is
/// disposal on that thread itself -- the component deleted by a callback on its stop ticket,
/// run where the stop finished: a thread cannot wait for its own end, so there dispose()
/// returns without waiting and the thread exits once the callback is done.
class threaded_component : public component
{
public:
    ~threaded_component() override = default;

    /// \return the id of the running thread, or 0 while there is none.
    ///
    /// To wait for the thread to leave run(), wait on the component's stop ticket --
    /// stop_ticket().wait() or wait_for(timeout). It is ready once the component has
    /// stopped completely, which includes its thread having left run(). The thread's exit
    /// is what dispose() waits for.
    core::thread_id get_thread_id() const;

protected:
    /// Thread will be started as a background one (detached).
    explicit threaded_component(std::string_view thread_name,
                                core::nullable<core::sync_root> sync_root_arg = nullptr);

    /// group can be nullptr, in which case the thread will be started as a background one
    /// (detached).
    threaded_component(std::string_view thread_name, thread_group* group,
                       core::nullable<core::sync_root> sync_root_arg = nullptr);

    /// Main threaded method; should be overridden in derivatives.
    virtual void run() = 0;

    void on_starting() override;

private:

    class impl_t;

    core::not_null<impl_t> impl();
    core::not_null<const impl_t> impl() const;
};

}  // export namespace wxl::async
