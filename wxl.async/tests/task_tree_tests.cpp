

#include <gtest/gtest.h>

#include "stress_test.h"

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

static std::exception_ptr specific_exception_instance =
    std::make_exception_ptr(specific_exception());

typedef future<void> task_future;

STRESS_TEST_CASE(task_tree, create_wait_and_dispose) {
    task_future f;
    ASSERT_TRUE(!f.valid());

    {
        task_tree_handle task = task_tree::create();

        ASSERT_TRUE(task.valid());
        ASSERT_TRUE(!task.succeeded());
        ASSERT_TRUE(!task.failed());
        ASSERT_TRUE(!task.finished());
        ASSERT_TRUE(!task.root().cancellation_token().is_ready());

        f = task.get_future();

        ASSERT_TRUE(f.valid());
        ASSERT_EQ(future_status::timeout, f.wait_for(duration::from_ms(1)));

        ASSERT_TRUE(task.valid());
        ASSERT_TRUE(!task.succeeded());
        ASSERT_TRUE(!task.failed());
        ASSERT_TRUE(!task.finished());
        ASSERT_TRUE(!task.root().cancellation_token().is_ready());
    }

    ASSERT_TRUE(f.valid());
    // The Promise had been destroyed before the Future was set to the ready state.
    ASSERT_THROW(f.get(), std::future_error);
}

namespace {

struct continuation {
    struct state : public refcounted_mt {
        bool volatile* run_flag_;
        bool volatile* delete_flag_;

        state(bool volatile* run_flag, bool volatile* delete_flag)
            : run_flag_(run_flag), delete_flag_(delete_flag) {}

        ~state() override { *delete_flag_ = true; }
    };

    continuation(bool volatile* run_flag, bool volatile* delete_flag)
        // Adopts the implicit first reference (refcounted_mt starts at ref_count() == 1).
        : state_(new state(run_flag, delete_flag), false) {}

    void operator()() noexcept { *state_->run_flag_ = true; }

    wxl::core::intrusive_ptr<state> state_;
};
}  // namespace

STRESS_TEST_CASE(task_tree, memory_check1) {
    bool volatile run_flag = false;
    bool volatile delete_flag = false;
    {
        task_tree_handle task = task_tree::create();
        task_future f = task.get_future();

        {
            continuation continuation(&run_flag, &delete_flag);
            f.when_ready(continuation);
        }

        ASSERT_TRUE(!run_flag);
        ASSERT_TRUE(!delete_flag);

        task.set_succeeded();

        ASSERT_TRUE(run_flag);
        ASSERT_TRUE(delete_flag);
    }
}

STRESS_TEST_CASE(task_tree, memory_check2) {
    bool volatile run_flag = false;
    bool volatile delete_flag = false;
    {
        task_future f;
        {
            task_tree_handle p = task_tree::create();

            f = p.get_future();
            {
                continuation cont(&run_flag, &delete_flag);
                f.when_ready(cont);
            }
            ASSERT_TRUE(!run_flag);
            ASSERT_TRUE(!delete_flag);
        }

        ASSERT_TRUE(run_flag);
        ASSERT_TRUE(delete_flag);
        ASSERT_TRUE(exception_is<std::future_error>(f.get_exception_ptr()));
    }
}

STRESS_TEST_CASE(task_tree, set_value_test) {
    task_tree_handle p = task_tree::create();
    p.set_succeeded();

    task_future f = p.get_future();

    ASSERT_TRUE(f.valid());
    f.get();
}

STRESS_TEST_CASE(task_tree, set_value_from_another_thread) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    joining_thread thread(&task_tree_handle::set_succeeded, &p);

    f.get();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(!f.has_exception());
}

STRESS_TEST_CASE(task_tree, set_exception_test) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    p.set_failed(specific_exception_instance);

    ASSERT_THROW(f.get(), specific_exception);

    ASSERT_TRUE(!f.has_value());
    ASSERT_TRUE(f.has_exception());
}

STRESS_TEST_CASE(task_tree, set_exception_from_another_thread) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    joining_thread thread(&task_tree_handle::set_failed, &p, specific_exception_instance);

    ASSERT_THROW(f.get(), specific_exception);
}

namespace {

class future_tester
{
public:
    class state;
    typedef wxl::core::intrusive_ptr<state> state_ptr;

    class state : public refcounted_mt
    {
        bool volatile succeeded_;
        bool volatile failed_;
        std::exception_ptr exception_;
        bool volatile finished_;

        bool volatile succeeded2_;
        bool volatile failed2_;
        std::exception_ptr exception2_;
        bool volatile finished2_;

        void when_succeeded() { succeeded_ = true; }

        struct when_succeeded_func {
            state_ptr this_;
            void operator()() noexcept { this_->when_succeeded(); }
        };

        void when_failed(const std::exception_ptr& ex) {
            failed_ = true;
            exception_ = ex;
        }

        struct when_failed_func {
            state_ptr this_;
            void operator()(const std::exception_ptr& ex) noexcept { this_->when_failed(ex); }
        };

        void when_ready() { finished_ = true; }

        void when_ready_with_future(task_future fut) {
            finished2_ = true;

            if (fut.has_exception()) {
                failed2_ = true;
                exception2_ = fut.get_exception_ptr();
            } else
                succeeded2_ = true;
        }

        struct when_ready_func {
            state_ptr this_;
            void operator()() noexcept { this_->when_ready(); }
        };

        struct when_ready_with_future_func {
            state_ptr this_;
            void operator()(task_future fut) noexcept { this_->when_ready_with_future(fut); }
        };

    public:
        state() noexcept
            : succeeded_(),
              failed_(),
              exception_(),
              finished_(),
              succeeded2_(),
              failed2_(),
              exception2_(),
              finished2_() {}

        void subscribe(task_future f) {
            when_succeeded_func wd = {this};
            when_failed_func wf = {this};
            when_ready_func wc = {this};
            when_ready_with_future_func wc2 = {this};
            f.when_succeeded(wd);
            f.when_failed(wf);
            f.when_ready(wc);
            f.when_ready(wc2);
        }

        struct next_func {
            wxl::core::intrusive_ptr<state> this_;
            void operator()() {
                this_->when_succeeded();
                this_->when_ready();
                this_->when_ready_with_future(make_ready_future());
            }
        };

        void assert_value() const {
            ASSERT_TRUE(succeeded_);
            ASSERT_TRUE(!failed_);
            ASSERT_EQ(exception_, std::exception_ptr());
            ASSERT_TRUE(finished_);

            ASSERT_TRUE(succeeded2_);
            ASSERT_TRUE(!failed2_);
            ASSERT_EQ(exception2_, std::exception_ptr());
            ASSERT_TRUE(finished2_);
        }

        void assert_exception() const {
            ASSERT_TRUE(!succeeded_);
            ASSERT_TRUE(failed_);
            ASSERT_EQ(exception_, specific_exception_instance);
            ASSERT_TRUE(finished_);

            ASSERT_TRUE(!succeeded2_);
            ASSERT_TRUE(failed2_);
            ASSERT_EQ(exception2_, specific_exception_instance);
            ASSERT_TRUE(finished2_);
        }

        void assert_nothing() const {
            ASSERT_TRUE(!succeeded_);
            ASSERT_TRUE(!failed_);
            ASSERT_EQ(exception_, std::exception_ptr());
            ASSERT_TRUE(!finished_);

            ASSERT_TRUE(!succeeded2_);
            ASSERT_TRUE(!failed2_);
            ASSERT_EQ(exception2_, std::exception_ptr());
            ASSERT_TRUE(!finished2_);
        }
    };

    future_tester() noexcept : state_(new state(), false) {}

    explicit future_tester(const task_future& f) : state_(new state(), false) {
        state_->subscribe(f);
    }

    // add_ref=false adopts the implicit first reference of a freshly constructed state
    // (refcounted_mt starts at ref_count() == 1); add_ref=true (the default) shares an
    // already-owned state, matching wxl::core::intrusive_ptr's own convention.
    explicit future_tester(state* state, bool add_ref = true) : state_(state, add_ref) {}

    void subscribe(const task_future& f) { state_->subscribe(f); }

    state* get() { return state_.get(); }

    state* operator->() { return get(); }

    state::next_func next_func() {
        const state::next_func f = {get()};
        return f;
    }

    void assert_value() const { state_->assert_value(); }

    void assert_exception() const { state_->assert_exception(); }

    void assert_nothing() const { state_->assert_nothing(); }

private:
    state_ptr state_;
};

}  // namespace

STRESS_TEST_CASE(task_tree, when_ready_by_value) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    future_tester tester(f);
    tester.assert_nothing();

    p.set_succeeded();
    tester.assert_value();
}

STRESS_TEST_CASE(task_tree, when_ready_by_exception) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    future_tester tester(f);
    tester.assert_nothing();

    p.set_failed(specific_exception_instance);
    tester.assert_exception();
}

STRESS_TEST_CASE(task_tree, late_when_ready_by_value) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    future_tester tester;
    p.set_succeeded();
    tester.subscribe(f);

    tester.assert_value();
}

STRESS_TEST_CASE(task_tree, late_when_ready_by_exception) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    future_tester tester;

    p.set_failed(specific_exception_instance);
    tester.subscribe(f);

    tester.assert_exception();
}

STRESS_TEST_CASE(task_tree, next_completed_by_value) {
    task_tree_handle p = task_tree::create();
    task_future f1 = p.get_future();

    future_tester tester;

    task_future f2 = f1.next(tester.next_func());
    tester.assert_nothing();

    p.set_succeeded();
    tester.assert_value();

    f2.get();  // both should not throw
    f1.get();
}

STRESS_TEST_CASE(task_tree, next_completed_by_exception1) {
    task_tree_handle p = task_tree::create();
    task_future f1 = p.get_future();

    future_tester tester;

    task_future f2 = f1.next(tester.next_func());
    tester.assert_nothing();

    p.set_failed(specific_exception_instance);
    tester.assert_nothing();

    // first future was failed, so second future should fail too
    ASSERT_THROW(f2.get(), specific_exception);
}

namespace {
struct broken_continuation {
    void operator()() { throw specific_exception(); }
};
}  // namespace

STRESS_TEST_CASE(task_tree, next_completed_by_exception2) {
    task_tree_handle p = task_tree::create();
    task_future first_f = p.get_future();

    broken_continuation broken_continuation;
    task_future last_f = first_f.next(broken_continuation);
    p.set_succeeded();

    // First future should not throw
    ASSERT_NO_THROW(first_f.get());

    // but last should
    ASSERT_THROW(last_f.get(), specific_exception);
}

STRESS_TEST_CASE(task_tree, next_completed_by_value_async) {
    task_tree_handle p = task_tree::create();
    task_future f1 = p.get_future();

    future_tester tester;

    task_future f2 = f1.next(tester.next_func());
    ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
    tester.assert_nothing();

    joining_thread thread(&task_tree_handle::set_succeeded, &p);

    f2.get();
    tester.assert_value();
}

STRESS_TEST_CASE(task_tree, next_completed_by_exception_async) {
    task_tree_handle p = task_tree::create();
    task_future f1 = p.get_future();

    future_tester tester;

    task_future f2 = f1.next(tester.next_func());
    ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
    tester.assert_nothing();

    joining_thread thread(&task_tree_handle::set_failed, &p, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

// Deliberately *not* an anonymous namespace: async_func is fed to future::next_unwrap(),
// which instantiates wxl.core's next_async_task<> with it. With an internal-linkage
// argument type MSVC (19.51) ends up emitting intrusive_ptr<future_shared_state>'s
// module-attached members into two COMDATs in this object file, and the linker rejects
// that with LNK1179. Giving the type external linkage avoids the compiler bug.
namespace task_tree_test_detail {
class async_future_tester : public future_tester
{
    typedef future_tester base;

public:
    class async_state : public state
    {
    public:
        async_state() noexcept : task_tree_handle_(task_tree::create()) {}

        void set_succeeded() { task_tree_handle_.set_succeeded(); }

        task_tree_handle task_tree_handle_;
    };

    struct async_func {
        wxl::core::intrusive_ptr<async_state> this_;
        task_future operator()() {
            future_tester t(this_.get());
            t.next_func()();
            return this_->task_tree_handle_.get_future();
        }
    };

    async_future_tester() noexcept : base(new async_state(), false) {}

    async_state* get() { return static_cast<async_state*>(base::get()); }

    async_func make_async_func() {
        const async_func f = {get()};
        return f;
    }

    void set_succeeded() { get()->task_tree_handle_.set_succeeded(); }

private:
};

}  // namespace task_tree_test_detail

using task_tree_test_detail::async_future_tester;

namespace {

template <typename T>
struct future_is_ready {
    wxl::async::future<T> f_;
    future_is_ready(const wxl::async::future<T>& f) : f_(f) {}
    bool operator()() const { return f_.is_ready(); }
};

template <typename T>
future_is_ready<T> is_ready(const wxl::async::future<T>& f) {
    return future_is_ready<T>(f);
}
}  // namespace

STRESS_TEST_CASE(task_tree, then_completed_by_value_async) {
    task_tree_handle p1 = task_tree::create();
    task_future f1 = p1.get_future();

    async_future_tester tester;
    task_future f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_TRUE(f1.wait_for(duration::zero()) == future_status::timeout);
    tester.assert_nothing();

    joining_thread thread(&task_tree_handle::set_succeeded, &p1);

    WXL_REQUIRE_BECOME_TRUE(is_ready(f1), big_timeout);

    f1.get();  // should not throw

    ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
    tester.set_succeeded();

    WXL_REQUIRE_BECOME_TRUE(is_ready(f2), big_timeout);  // try to speed up the stress test

    f2.get();  // should not throw
    tester.assert_value();
}

STRESS_TEST_CASE(task_tree, then_completed_by_exception_async) {
    task_tree_handle p1 = task_tree::create();
    task_future f1 = p1.get_future();

    async_future_tester tester;

    task_future f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_TRUE(f1.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&task_tree_handle::set_failed, &p1, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

STRESS_TEST_CASE(task_tree, when_all_completed_by_value) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(task_tree, when_all_completed_by_exception1) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_failed(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(task_tree, when_all_completed_by_exception2) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_failed(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

namespace {
struct when_all_completed_by_value_async_func {
    task_tree_handle p1, p2;
    task_future result;

    void operator()() {
        p1.set_succeeded();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_succeeded();
    }
};
}  // namespace

STRESS_TEST_CASE(task_tree, when_all_completed_by_value_async) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(f1.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_by_value_async_func func = {p1, p2, result};

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
}

namespace {
struct when_all_completed_by_exception_async1_func {
    task_tree_handle p1, p2;
    task_future result;

    void operator()() {
        p1.set_succeeded();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_failed(specific_exception_instance);
    }
};
}  // namespace

STRESS_TEST_CASE(task_tree, when_all_completed_by_exception_async1) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_by_exception_async1_func func = {p1, p2, result};

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(task_tree, when_all_completed_by_exception_async2) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&task_tree_handle::set_failed, &p1, specific_exception_instance);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(task_tree, when_any_completed_by_value) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(task_tree, when_any_completed_by_exception) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_failed(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(task_tree, when_any_completed_by_value_async) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&task_tree_handle::set_succeeded, &p1);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(task_tree, when_any_completed_by_exception_async) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&task_tree_handle::set_failed, &p2, specific_exception_instance);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
    ASSERT_TRUE(!result.has_value());
}

STRESS_TEST_CASE(task_tree, when_all_completed_completed_by_value) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

STRESS_TEST_CASE(task_tree, when_all_completed_completed_by_value_and_exception) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_failed(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_succeeded();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
struct when_all_completed_completed_by_value_async_func {
    task_tree_handle p1, p2;
    task_future result;

    void operator()() {
        p1.set_succeeded();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_succeeded();
    }
};
}  // namespace

STRESS_TEST_CASE(task_tree, when_all_completed_completed_by_value_async) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_async_func func = {p1, p2, result};

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
struct when_all_completed_completed_by_value_and_exception_async_func {
    task_tree_handle p1, p2;
    task_future result;

    void operator()() {
        p1.set_succeeded();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_failed(specific_exception_instance);
    }
};
}  // namespace

// --log_level=test_suite
STRESS_TEST_CASE(task_tree, when_all_completed_completed_by_value_and_exception_async) {
    task_tree_handle p1 = task_tree::create();
    task_tree_handle p2 = task_tree::create();
    task_future f1 = p1.get_future();
    task_future f2 = p2.get_future();

    task_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_and_exception_async_func func = {p1, p2, result};

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
struct complex_completed_by_value_func {
    complex_completed_by_value_func() noexcept
        : counter_(), first_callback_n_(), second_callback_n_() {}

    void first_callback() { first_callback_n_ = ++counter_; }

    void second_callback() { second_callback_n_ = ++counter_; }

    int counter_;
    int first_callback_n_;
    int second_callback_n_;
};
}  // namespace

STRESS_TEST_CASE(task_tree, complex_completed_by_value) {
    task_tree_handle task = task_tree::create();
    task_future f = task.get_future();

    complex_completed_by_value_func func;

    f.when_succeeded([&func]() noexcept { func.first_callback(); });
    f.when_succeeded([&func]() noexcept { func.second_callback(); });

    task.set_succeeded();

    ASSERT_TRUE(func.first_callback_n_ > 0 && func.first_callback_n_ < 3);
    ASSERT_TRUE(func.second_callback_n_ > 0 && func.second_callback_n_ < 3);

    ASSERT_TRUE(func.first_callback_n_ != func.second_callback_n_);
}

namespace {
struct complex_completed_by_exception_func {
    complex_completed_by_exception_func() noexcept
        : counter_(), first_callback_n_(), second_callback_n_() {}

    void first_callback(const std::exception_ptr& ex_ptr) {
        first_callback_n_ = ++counter_;
        ex_ptr1 = ex_ptr;
    }

    void second_callback(const std::exception_ptr& ex_ptr) {
        second_callback_n_ = ++counter_;
        ex_ptr2 = ex_ptr;
    }

    int counter_;
    int first_callback_n_;
    std::exception_ptr ex_ptr1;
    int second_callback_n_;
    std::exception_ptr ex_ptr2;
};
}  // namespace

STRESS_TEST_CASE(task_tree, complex_completed_by_exception) {
    task_tree_handle p = task_tree::create();
    task_future f = p.get_future();

    complex_completed_by_exception_func func;

    f.when_failed([&func](const std::exception_ptr& e) noexcept { func.first_callback(e); });
    f.when_failed([&func](const std::exception_ptr& e) noexcept { func.second_callback(e); });

    p.set_failed(specific_exception_instance);

    ASSERT_TRUE(func.first_callback_n_ > 0 && func.first_callback_n_ < 3);
    ASSERT_TRUE(func.second_callback_n_ > 0 && func.second_callback_n_ < 3);
    ASSERT_TRUE(func.first_callback_n_ != func.second_callback_n_);

    ASSERT_EQ(func.ex_ptr1, func.ex_ptr2);
}

STRESS_TEST_CASE(task_tree, subtask_complete) {
    task_tree_handle parent = task_tree::create();
    ASSERT_EQ(parent, parent.root());

    task_tree_handle child = task_tree::create(parent);
    ASSERT_EQ(parent, child.root());

    future_tester tester(parent.get_future());
    tester.assert_nothing();

    parent.set_succeeded();
    tester.assert_nothing();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!child.succeeded());
    ASSERT_TRUE(!parent.root().cancellation_token().is_ready());

    child.set_succeeded();

    tester.assert_value();

    ASSERT_TRUE(parent.succeeded());
    ASSERT_TRUE(child.succeeded());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());
}

STRESS_TEST_CASE(task_tree, parent_cancel) {
    task_tree_handle parent = task_tree::create();
    task_tree_handle child = task_tree::create(parent);

    future_tester tester(parent.get_future());
    tester.assert_nothing();

    parent.set_failed(specific_exception_instance);

    tester.assert_nothing();

    ASSERT_TRUE(!parent.failed());  // this will be known only when all group tasks are resolved.
    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!parent.finished());
    ASSERT_TRUE(
        parent.root()
            .cancellation_token()
            .is_ready());  // early notification about the cancellation of one of group tasks.

    ASSERT_TRUE(!child.failed());
    ASSERT_TRUE(!child.succeeded());
    ASSERT_TRUE(!child.finished());

    child.set_succeeded();

    tester.assert_exception();

    ASSERT_TRUE(parent.failed());
    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(parent.finished());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());

    ASSERT_TRUE(!child.failed());
    ASSERT_TRUE(child.succeeded());
    ASSERT_TRUE(child.finished());
}

STRESS_TEST_CASE(task_tree, child_cancel) {
    task_tree_handle parent = task_tree::create();
    task_tree_handle child = task_tree::create(parent);

    future_tester tester(parent.get_future());
    tester.assert_nothing();

    child.set_failed(specific_exception_instance);
    tester.assert_nothing();

    ASSERT_TRUE(!parent.failed());
    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!parent.finished());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());

    ASSERT_TRUE(child.failed());
    ASSERT_TRUE(child.finished());
    ASSERT_TRUE(!child.succeeded());

    parent.set_succeeded();

    tester.assert_exception();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(parent.finished());
    ASSERT_TRUE(parent.failed());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());

    ASSERT_TRUE(child.failed());
    ASSERT_TRUE(!child.succeeded());
    ASSERT_TRUE(child.finished());
}

STRESS_TEST_CASE(task_tree, subtask_complete2) {
    task_tree_handle parent = task_tree::create();
    ASSERT_EQ(parent, parent.root());

    task_tree_handle child1 = task_tree::create(parent);
    ASSERT_EQ(parent, child1.root());

    task_tree_handle child2 = task_tree::create(child1);
    ASSERT_EQ(parent, child2.root());

    future_tester parent_tester(parent.get_future());
    future_tester child_tester(child1.get_future());
    parent_tester.assert_nothing();
    child_tester.assert_nothing();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!child1.succeeded());
    ASSERT_TRUE(!child2.succeeded());

    child2.set_succeeded();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!child1.succeeded());
    ASSERT_TRUE(child2.succeeded());
    ASSERT_TRUE(!parent.root().cancellation_token().is_ready());

    parent_tester.assert_nothing();
    child_tester.assert_nothing();

    child1.set_succeeded();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(child1.succeeded());
    ASSERT_TRUE(child2.succeeded());
    ASSERT_TRUE(!parent.root().cancellation_token().is_ready());

    parent_tester.assert_nothing();
    child_tester.assert_value();
    ASSERT_TRUE(!parent.root().cancellation_token().is_ready());

    parent.set_succeeded();

    parent_tester.assert_value();
    ASSERT_TRUE(parent.succeeded());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());
}

STRESS_TEST_CASE(task_tree, subtask_cancel3) {
    task_tree_handle parent = task_tree::create();

    ASSERT_EQ(parent, parent.root());

    task_tree_handle child1 = parent.create_subtask();
    ASSERT_EQ(parent, child1.root());

    task_tree_handle child2 = parent.create_subtask();
    ASSERT_EQ(parent, child2.root());

    future_tester parent_tester(parent.get_future());
    future_tester cancellation_tester(parent.root().cancellation_token());
    future_tester child_tester1(child1.get_future());
    future_tester child_tester2(child2.get_future());
    parent_tester.assert_nothing();
    child_tester1.assert_nothing();
    child_tester2.assert_nothing();

    parent.set_succeeded();

    ASSERT_TRUE(!parent.succeeded());
    ASSERT_TRUE(!parent.finished());
    ASSERT_TRUE(!parent.root().cancellation_token().is_ready());
    parent_tester.assert_nothing();

    child1.set_failed(specific_exception_instance);

    ASSERT_TRUE(child1.failed());
    ASSERT_TRUE(!parent.finished());
    ASSERT_TRUE(!parent.failed());
    ASSERT_TRUE(parent.root().cancellation_token().is_ready());

    child_tester1.assert_exception();
    parent_tester.assert_nothing();
    cancellation_tester.assert_exception();

    child2.set_succeeded();

    ASSERT_TRUE(parent.root().cancellation_token().is_ready());
    child_tester2.assert_value();
    parent_tester.assert_exception();
}

STRESS_TEST_CASE(task_tree, complete_by_error) {
    task_tree_handle parent = task_tree::create();
    ASSERT_EQ(parent, parent.root());

    task_tree_handle child = task_tree::create(parent);
    ASSERT_EQ(parent, child.root());

    future_tester tester(parent.get_future());
    tester.assert_nothing();

    parent.set_succeeded();
    tester.assert_nothing();

    child.set_failed(specific_exception_instance);
    tester.assert_exception();
}

STRESS_TEST_CASE(task_tree, accepts_subtasks_until_its_own_work_is_reported) {
    task_tree_handle parent = task_tree::create();
    ASSERT_TRUE(parent.accepts_subtasks());

    task_tree_handle child = parent.create_subtask();
    ASSERT_TRUE(parent.accepts_subtasks());  // a subtask under it changes nothing

    // Reporting the parent's own work closes it to new subtasks, and that happens well
    // before the node itself finishes -- the child is still running.
    parent.set_succeeded();
    ASSERT_TRUE(!parent.accepts_subtasks());
    ASSERT_TRUE(!parent.finished());
    ASSERT_THROW(parent.create_subtask(), std::logic_error);

    child.set_succeeded();
    ASSERT_TRUE(parent.finished());
    ASSERT_TRUE(!parent.accepts_subtasks());
}

STRESS_TEST_CASE(task_tree, a_failed_task_accepts_no_subtasks_either) {
    task_tree_handle task = task_tree::create();
    task.set_failed(specific_exception_instance);

    ASSERT_TRUE(!task.accepts_subtasks());
    ASSERT_THROW(task.create_subtask(), std::logic_error);
}
