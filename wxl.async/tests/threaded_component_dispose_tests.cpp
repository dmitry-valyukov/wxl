#include "process_handles.h"

#include <gtest/gtest.h>

#include "stress_test.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// A component whose thread is slow to leave: past run() and past its report that the
/// component has stopped, it still sleeps in a thread-exit callback -- where a real thread
/// joins its group's finished threads and marks its own entry finished, drops its
/// thread_local data and says the runtime's goodbye. The sleep holds that stretch open long
/// enough to be seen; with dispose() waiting for the thread, its length makes no difference.
class slow_exit_component : public threaded_component
{
public:
    slow_exit_component(thread_group* group, HANDLE& thread)
        : threaded_component("slow exit", group), thread_(thread) {}

    ~slow_exit_component() override { dispose(); }

    using threaded_component::dispose;

protected:
    /// On the component's thread, which is what opens it here.
    void on_started() override {
        threaded_component::on_started();

        thread_ = open_current_thread();
        thread_group::at_thread_exit(
            +[] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); });
    }

    void run() override { stopped_.acquire(); }

    void on_stopping() override {
        threaded_component::on_stopping();
        stopped_.release();
    }

private:
    HANDLE& thread_;
    semaphore stopped_{0};
};

void expect_gone_after_dispose(thread_group* group) {
    HANDLE thread = nullptr;
    slow_exit_component c(group, thread);

    c.start_async().get();
    ASSERT_NE(thread, nullptr);

    c.stop_async().get();
    c.dispose();

    // Not waited for: dispose() has to have done that already.
    EXPECT_EQ(::WaitForSingleObject(thread, 0), WAIT_OBJECT_0)
        << "dispose() returned with the component's thread still running";

    ::CloseHandle(thread);
}

void expect_gone_after_destructor(thread_group* group) {
    HANDLE thread = nullptr;

    {
        slow_exit_component c(group, thread);
        c.start_async().get();
        ASSERT_NE(thread, nullptr);
    }

    EXPECT_EQ(::WaitForSingleObject(thread, 0), WAIT_OBJECT_0)
        << "the destructor returned with the component's thread still running";

    ::CloseHandle(thread);
}

/// Stops by itself, and its whole stop -- the report, the ticket, the callbacks on it --
/// happens on its own thread; a callback on the ticket deletes it there.
class self_disposing_component : public threaded_component
{
public:
    self_disposing_component(HANDLE& thread, promise<thread_id> disposed)
        : threaded_component("self disposing"), thread_(thread), disposed_(std::move(disposed)) {}

    /// Told once dispose() is back: which thread it was on.
    ~self_disposing_component() override {
        dispose();
        disposed_.set_value(current_thread_id());
    }

protected:
    void on_started() override {
        threaded_component::on_started();
        thread_ = open_current_thread();
    }

    void run() override {}

private:
    HANDLE& thread_;
    promise<thread_id> disposed_;
};

}  // namespace

TEST(ThreadedComponentDispose, ManagedThreadIsGoneWhenDisposeReturns) {
    thread_scope group;
    expect_gone_after_dispose(group);
}

TEST(ThreadedComponentDispose, DetachedThreadIsGoneWhenDisposeReturns) {
    expect_gone_after_dispose(nullptr);
}

TEST(ThreadedComponentDispose, ManagedThreadIsGoneWhenDestructorReturns) {
    thread_scope group;
    expect_gone_after_destructor(group);
}

TEST(ThreadedComponentDispose, DetachedThreadIsGoneWhenDestructorReturns) {
    expect_gone_after_destructor(nullptr);
}

// The thread cannot wait for its own end: disposed there, the component lets go without
// waiting, and the thread leaves afterwards on its own.
TEST(ThreadedComponentDispose, FromItsOwnThreadDoesNotWaitForItself) {
    HANDLE thread = nullptr;
    promise<thread_id> disposed;
    const future<thread_id> disposed_on = disposed.get_future();

    auto* const c = new self_disposing_component(thread, disposed);
    c->stop_ticket().when_ready([c]() noexcept { delete c; });
    c->start_async().get();

    ASSERT_NE(thread, nullptr);
    ASSERT_EQ(disposed_on.wait_for(big_timeout), future_status::ready)
        << "dispose() on the component's own thread did not return";

    EXPECT_EQ(disposed_on.get(), ::GetThreadId(thread))
        << "the component was not disposed on its own thread, so this checked nothing";

    EXPECT_EQ(::WaitForSingleObject(thread, to_os_timeout_ms(big_timeout)), WAIT_OBJECT_0)
        << "the component's thread did not leave after disposing of it";

    ::CloseHandle(thread);
}
