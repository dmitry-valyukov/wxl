

#include <gtest/gtest.h>


#include "stress_test.h"

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

typedef promise<void> test_promise;
typedef future<void> test_future;

static std::exception_ptr specific_exception_instance = std::make_exception_ptr(specific_exception());

namespace {
/// Returns \c true if the given Future object has the exception of the \e E type, \c false otherwise.
template <typename E>
bool has_exception(const test_future & f) {
    if(!f.has_exception())
        return false;

    try {
        f.get();
    }
    catch(const E &) {
        return true;
    }
    catch(...) {
        // ignore
    }

    return false;
}
}

STRESS_TEST_CASE(future, create_copy_move_dispose)
{
    {
        // Create and dispose
        test_future f;

        ASSERT_TRUE(!f.valid());

        // SharedStateHolder does not refers to a shared state:
        ASSERT_THROW(f.get(), std::logic_error);
        ASSERT_THROW(f.is_ready(), std::logic_error);
        ASSERT_THROW(f.has_exception(), std::logic_error);
        ASSERT_THROW(f.has_value(), std::logic_error);

        {
            test_promise p;

            f = p.get_future();
            ASSERT_TRUE(f.valid());
            ASSERT_TRUE(! f.is_ready());

            ASSERT_EQ(future_status::timeout, f.wait_for(duration::from_ms(1)));
            ASSERT_TRUE(f.valid());
            ASSERT_TRUE(!f.is_ready());
        }

        // Here the Promise was destroyed before the corresponding Future had been set to the Ready state.
        ASSERT_THROW(f.get(), std::future_error);
        ASSERT_TRUE(f.is_ready());
        ASSERT_TRUE(f.has_exception());
        ASSERT_TRUE(! f.has_value());
    }

    {
        // Create, move and dispose
        test_future f1, f2;

        {
            test_promise p1, p2;

            f1 = p1.get_future();
            f2 = p2.get_future();

            ASSERT_TRUE(p1.valid());
            ASSERT_TRUE(p2.valid());

            ASSERT_TRUE(! f1.is_ready());
            ASSERT_TRUE(! f2.is_ready());

            p2 = std::move(p1);

            ASSERT_TRUE(! p1.valid());
            ASSERT_TRUE(p2.valid());

            // Here the promise p2 was destroyed before the corresponding Future had been set to the Ready state.
            ASSERT_TRUE(has_exception<std::future_error>(f2));
            ASSERT_TRUE(! f1.is_ready());
        }

        // Here the promise p1 was destroyed before the corresponding Future had been set to the Ready state.
        ASSERT_TRUE(has_exception<std::future_error>(f1));
    }

    {
        // Create, copy, move and dispose
        test_future f;

        test_promise p1;
        test_promise p2 = p1;    // two owners over a single SharedState.

        f = p1.get_future();

        ASSERT_EQ(future_status::timeout, f.wait_for(duration::from_ms(1)));

        p2 = std::move(p1);
        ASSERT_TRUE(!p1.valid());
        ASSERT_TRUE(p2.valid());

        // Here the promise p1 was destroyed.
        ASSERT_TRUE(!f.is_ready());

        p2 = std::move(p1);
        ASSERT_TRUE(!p2.valid());

        // Here the promise p2 was destroyed .
        ASSERT_TRUE(has_exception<std::future_error>(f));
    }
}

namespace {

/// In asynchronous programming, it is very common for one asynchronous operation, on completion, to invoke a second operation and pass data to it.
struct continuation {
    struct state {
        bool volatile * run_flag_;
        bool volatile * delete_flag_;

        state(bool volatile * run_flag, bool volatile * delete_flag)
            : run_flag_(run_flag)
            , delete_flag_(delete_flag)
        {}

        ~state() {
            *delete_flag_ = true;
        }
    };

    continuation(bool volatile * run_flag, bool volatile * delete_flag)
        : state_(std::make_shared<state>(run_flag, delete_flag))
    {}

    void operator()() noexcept {
        *state_->run_flag_ = true;
    }

    std::shared_ptr<state> state_;
};
}

STRESS_TEST_CASE(future, memory_check)
{
    bool volatile run_flag = false;
    bool volatile delete_flag = false;
    {
        test_promise p;
        test_future f = p.get_future();

        {
            continuation cont(&run_flag, &delete_flag);
            f.when_ready(cont);
        }

        ASSERT_TRUE(! run_flag);
        ASSERT_TRUE(! delete_flag);

        p.set_value();

        ASSERT_TRUE(run_flag);
        ASSERT_TRUE(delete_flag);
    }
}

STRESS_TEST_CASE(future, set_value_test)
{
    test_promise p;
    p.set_value();

    test_future f = p.get_future();

    ASSERT_TRUE(f.valid());
    f.get();
    ASSERT_TRUE(f.valid()); // still valid

    ASSERT_THROW(f.get_exception_ptr(), std::logic_error);
}

STRESS_TEST_CASE(future, set_value_from_another_thread)
{
    test_promise p;
    test_future f = p.get_future();

    joining_thread thread(&test_promise::set_value, &p);

    f.get();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(!f.has_exception());
}

namespace {
void set_value(barrier * enter_barrier, test_promise* promise) {
    enter_barrier->enter();

    promise->set_value();
}
}

STRESS_TEST_CASE(future, set_value_from_multiple_threads)
{
    test_promise p;
    test_future f = p.get_future();

    thread_scope manager;
    const unsigned thread_count = 32;

    barrier enter_barrier(thread_count);

    for (unsigned i = 0; i < thread_count; i++) {
        manager->spawn(set_value, &enter_barrier, &p);
    }

    f.get();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(!f.has_exception());
}

STRESS_TEST_CASE(future, set_exception_test)
{
    test_promise p;
    test_future f = p.get_future();

    p.set_exception(specific_exception_instance);

    std::exception_ptr second_exception = std::make_exception_ptr(std::logic_error("The second exception is ignored"));

    p.set_exception(second_exception);

    ASSERT_THROW(f.get(), specific_exception);

    ASSERT_TRUE(!f.has_value());
    ASSERT_TRUE(f.has_exception());
}

STRESS_TEST_CASE(future, set_exception_from_another_thread)
{
    test_promise p;
    test_future f = p.get_future();

    joining_thread thread(&test_promise::set_exception, &p, specific_exception_instance);

    ASSERT_THROW(f.get(), specific_exception);
}

namespace{
struct atomic_continuation {
    struct state {
        state()
            : counter_()
        {}

        void increase_counter() {
            counter_.fetch_add(1, std::memory_order_relaxed);
        }

        std::atomic<size_t> counter_;
    };

    atomic_continuation()
        : state_(std::make_shared<state>())
    {}

    void operator()() noexcept {
        state_->increase_counter();
    }

    std::shared_ptr<state> state_;
};

void add_continuation(barrier * enter_barrier, test_future& future, atomic_continuation& continuation, size_t number_of_cycles) {
    enter_barrier->enter();

    for (size_t i = 0; i < number_of_cycles; ++i) {
        future.when_ready(continuation);
    }
}

};

STRESS_TEST_CASE(future, concurrent_continuations)
{
    test_promise promise;
    test_future future = promise.get_future();

    thread_scope manager;
    const size_t thread_count = 32;

    barrier enter_barrier(thread_count);
    atomic_continuation continuation;

    const size_t number_of_cycles = 500;

    for (size_t i = 0; i < thread_count; i++) {
        manager->spawn(add_continuation, &enter_barrier, future, continuation, number_of_cycles);
    }

    manager->join_all();

    ASSERT_EQ(0u, continuation.state_->counter_.load());

    promise.set_value();

    ASSERT_EQ(thread_count * number_of_cycles, continuation.state_->counter_.load());
}

namespace {
struct when_done_with_value{
    when_done_with_value(int* value)
        : value_(value)
    {}

    void operator()(int value) noexcept {
        *value_ = value;
    }

    int* value_;
};
}

STRESS_TEST_CASE(future, when_succeeded_with_value)
{
    promise<int> p;
    future<int> f = p.get_future();

    int first_value = 0;
    when_done_with_value first_continuation(&first_value);

    int second_value = 0;
    when_done_with_value second_continuation(&second_value);

    f.when_succeeded(first_continuation); // WhenDoneWithValue will be copied.
    f.when_succeeded(second_continuation);

    p.set_value(7);

    ASSERT_EQ(7, *first_continuation.value_);
    ASSERT_EQ(7, *second_continuation.value_);

    int third_value = 0;
    when_done_with_value third_continuation(&third_value);
    f.when_succeeded(third_continuation);
    ASSERT_EQ(7, *third_continuation.value_);
}


namespace {
class future_tester : public noncopyable
{
    bool volatile succeeded_;
    bool volatile failed_;
    std::exception_ptr exception_;
    bool volatile ready_;
    bool volatile succeeded2_;
    bool volatile failed2_;
    std::exception_ptr exception2_;
    bool volatile ready2_;

    void when_succeeded() {
        succeeded_ = true;
    }

    struct when_succeeded_func {
        future_tester * this_;
        void operator()() noexcept {
            this_->when_succeeded();
        }
    };

    void when_failed(const std::exception_ptr & ex) {
        failed_ = true;
        exception_ = ex;
    }

    struct when_failed_func {
        future_tester * this_;
        void operator()(const std::exception_ptr & ex) noexcept {
            this_->when_failed(ex);
        }
    };

    void when_ready() {
        ready_ = true;
    }

    void when_ready_with_future(test_future fut) {
        ready2_ = true;

        if(fut.has_exception()) {
            failed2_ = true;
            exception2_ = fut.get_exception_ptr();
        }
        else
            succeeded2_ = true;
    }

    struct when_ready_func {
        future_tester * this_;
        void operator()() noexcept {
            this_->when_ready();
        }
    };

    struct when_ready_with_future_func {
        future_tester * this_;
        void operator()(test_future fut) noexcept {
            this_->when_ready_with_future(fut);
        }
    };

public:
    future_tester() noexcept
        : succeeded_()
        , failed_()
        , exception_()
        , ready_()
        , succeeded2_()
        , failed2_()
        , exception2_()
        , ready2_()
    {}

    future_tester(test_future & f)
        : succeeded_()
        , failed_()
        , exception_()
        , ready_()
        , succeeded2_()
        , failed2_()
        , exception2_()
        , ready2_()
    {
        subscribe(f);
    }

    void subscribe(test_future f) {
        const when_succeeded_func wd = { this };
        const when_failed_func wf = { this };
        const when_ready_func wc = { this };
        const when_ready_with_future_func wc2 = { this };
        f.when_succeeded(wd);
        f.when_failed(wf);
        f.when_ready(wc);
        f.when_ready(wc2);
    }

    struct next_func {
        future_tester * this_;

        void operator()() noexcept {
            this_->when_succeeded();
            this_->when_ready();
            this_->when_ready_with_future(make_ready_future());
        }
    };

    next_func make_next_func() {
        next_func f = { this };
        return f;
    }

    void assert_value() {
        ASSERT_TRUE(succeeded_);
        ASSERT_TRUE(!failed_);
        ASSERT_EQ(exception_, std::exception_ptr());
        ASSERT_TRUE(ready_);
        ASSERT_TRUE(succeeded2_);
        ASSERT_TRUE(!failed2_);
        ASSERT_EQ(exception2_, std::exception_ptr());
        ASSERT_TRUE(ready2_);
    }

    void assert_exception() {
        ASSERT_TRUE(!succeeded_);
        ASSERT_TRUE(failed_);
        ASSERT_EQ(exception_, specific_exception_instance);
        ASSERT_TRUE(ready_);
        ASSERT_TRUE(!succeeded2_);
        ASSERT_TRUE(failed2_);
        ASSERT_EQ(exception2_, specific_exception_instance);
        ASSERT_TRUE(ready2_);
    }

    void assert_nothing() {
        ASSERT_TRUE(!succeeded_);
        ASSERT_TRUE(!failed_);
        ASSERT_EQ(exception_, std::exception_ptr());
        ASSERT_TRUE(!ready_);
        ASSERT_TRUE(!succeeded2_);
        ASSERT_TRUE(!failed2_);
        ASSERT_EQ(exception2_, std::exception_ptr());
        ASSERT_TRUE(!ready2_);
    }
};
}

STRESS_TEST_CASE(future, when_ready_by_value)
{
    test_promise p;
    test_future f = p.get_future();

    future_tester tester(f);
    tester.assert_nothing();

    p.set_value();
    tester.assert_value();
}

STRESS_TEST_CASE(future, when_ready_by_exception)
{
    test_promise p;
    test_future f = p.get_future();

    future_tester tester(f);
    tester.assert_nothing();

    p.set_exception(specific_exception_instance);
    tester.assert_exception();
}

STRESS_TEST_CASE(future, late_when_ready_by_value)
{
    test_promise p;
    test_future f = p.get_future();
    p.set_value();

    future_tester tester;
    tester.subscribe(f);
    tester.assert_value();
}

STRESS_TEST_CASE(future, late_when_ready_by_exception)
{
    test_promise p;
    test_future f = p.get_future();
    p.set_exception(specific_exception_instance);

    future_tester tester;
    tester.subscribe(f);
    tester.assert_exception();
}

namespace {
/// A functor callable both ways -- with the future and with nothing at all. Such a functor makes
/// when_ready(..) choose, and it must choose the form that carries the outcome: the other one
/// would silently turn a subscription that reports a failure into one that cannot see it. No
/// production code in the tree is shaped like this today, which is precisely why the rule needs
/// a functor of its own here.
struct both_forms_func {
    int * no_arg_calls_;
    int * future_calls_;
    bool * saw_exception_;

    void operator()() noexcept { ++*no_arg_calls_; }

    void operator()(test_future fut) noexcept {
        ++*future_calls_;
        *saw_exception_ = fut.has_exception();
    }
};

// Guards the two tests below: drop either operator() and they would keep passing while
// checking nothing, because a functor with one form has no choice to make.
static_assert(std::is_nothrow_invocable_v<both_forms_func &>);
static_assert(std::is_nothrow_invocable_v<both_forms_func &, test_future>);
}

STRESS_TEST_CASE(future, when_ready_prefers_the_form_with_future)
{
    int no_arg_calls = 0;
    int future_calls = 0;
    bool saw_exception = false;
    const both_forms_func func = { &no_arg_calls, &future_calls, &saw_exception };

    test_promise p;
    test_future f = p.get_future();

    f.when_ready(func);   // pending: the callback is stored and called on resolution
    p.set_value();
    f.when_ready(func);   // ready already: the immediate path picks the same overload

    ASSERT_EQ(future_calls, 2);
    ASSERT_EQ(no_arg_calls, 0);
    ASSERT_TRUE(!saw_exception);

    // when_succeeded() has no future to hand over, so the same functor goes the other way.
    f.when_succeeded(func);

    ASSERT_EQ(no_arg_calls, 1);
    ASSERT_EQ(future_calls, 2);
}

STRESS_TEST_CASE(future, when_ready_prefers_the_form_with_future_by_exception)
{
    int no_arg_calls = 0;
    int future_calls = 0;
    bool saw_exception = false;
    const both_forms_func func = { &no_arg_calls, &future_calls, &saw_exception };

    test_promise p;
    test_future f = p.get_future();

    f.when_ready(func);
    p.set_exception(specific_exception_instance);
    f.when_ready(func);

    ASSERT_EQ(future_calls, 2);
    ASSERT_EQ(no_arg_calls, 0);
    ASSERT_TRUE(saw_exception);   // the whole point: the failure reached the callback

    // A failed future never calls the success subscription, so the no-argument form stays
    // unreached by this route as well.
    f.when_succeeded(func);

    ASSERT_EQ(no_arg_calls, 0);
}

STRESS_TEST_CASE(future, next_completed_by_value)
{
    test_promise p;
    test_future f1 = p.get_future();

    future_tester tester;

    test_future f2 = f1.next(tester.make_next_func());
    tester.assert_nothing();

    p.set_value();

    tester.assert_value();

    test_future f3 = f1.next(tester.make_next_func());

    // Should not throw:
    f2.get();
    f1.get();
    f3.get();
}

STRESS_TEST_CASE(future, next_completed_by_exception1)
{
    test_promise p;
    test_future f1 = p.get_future();

    future_tester tester;

    test_future f2 = f1.next(tester.make_next_func());
    tester.assert_nothing();

    p.set_exception(specific_exception_instance);
    tester.assert_nothing();

    // first future was failed, so second future should fail too
    ASSERT_THROW(f2.get(), specific_exception);
}

// next() attached AFTER the future has already failed. Every other subscription takes its fast
// path on ready(), so this one has to as well: the continuation must not run, the exception has
// to reach the future next() returns, and the source future must stay failed -- attaching a
// pending node over a resolved state would push it back to pending and lose the failure.
STRESS_TEST_CASE(future, late_next_completed_by_exception)
{
    test_promise p;
    test_future f1 = p.get_future();

    p.set_exception(specific_exception_instance);

    future_tester tester;

    test_future f2 = f1.next(tester.make_next_func());

    // The continuation runs on success only; the exception goes to the future next() returns.
    tester.assert_nothing();
    ASSERT_TRUE(f2.is_ready());
    ASSERT_THROW(f2.get(), specific_exception);

    // And the source future is still the failed one it was.
    ASSERT_TRUE(f1.is_ready());
    ASSERT_TRUE(f1.has_exception());
    ASSERT_THROW(f1.get(), specific_exception);
}

namespace {
struct broken_continuation {
    void operator()() {
        throw specific_exception();
    }
};
}

STRESS_TEST_CASE(future, next_completed_by_exception2)
{
    test_promise p;
    test_future first_f = p.get_future();

    test_future last_f = first_f.next(broken_continuation());

    p.set_value();

    // First future should not throw
    ASSERT_NO_THROW(first_f.get());

    // but last should
    ASSERT_THROW(last_f.get(), specific_exception);
}

namespace {
    void next_completed_by_value_async_body(barrier * enter_barrier, barrier * exit_barrier) {
        test_promise p;
        test_future f1 = p.get_future();

        future_tester tester;

        test_future f2 = f1.next(tester.make_next_func());
        ASSERT_EQ(future_status::timeout, f2.wait_for(duration::zero()));

        tester.assert_nothing();

        enter_barrier->enter();

        joining_thread thread(&test_promise::set_value, &p);

        f2.get();
        tester.assert_value();

        exit_barrier->enter();
    }
}

STRESS_TEST_CASE(future, next_completed_by_value_async_2)
{
    thread_scope manager;

#ifdef NDEBUG
    const unsigned thread_count = 32;
#else
    const unsigned thread_count = 8;
#endif

    barrier enter_barrier(thread_count + 1), exit_barrier(thread_count + 1);

    for (unsigned i = 0; i < thread_count; i++) {
        const std::string name = "nextCompletedByValueAsync_2 # " + std::to_string(i);

        manager->spawn_named(wxl::core::assume_valid(name), next_completed_by_value_async_body,
                             &enter_barrier, &exit_barrier);
    }

    next_completed_by_value_async_body(&enter_barrier, &exit_barrier);
}

STRESS_TEST_CASE(future, next_completed_by_exception_async)
{
    test_promise p;
    test_future f1 = p.get_future();

    future_tester tester;

    test_future f2 = f1.next(tester.make_next_func());

    ASSERT_EQ(future_status::timeout, f2.wait_for(duration::zero()));

    tester.assert_nothing();

    joining_thread thread(&test_promise::set_exception, &p, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

namespace {
class async_future_tester : public future_tester
{
    typedef future_tester base;

public:
    /// Asynchronous continuation.
    struct async_func {
        async_future_tester * this_;

        test_future operator()() {
            this_->make_next_func()();
            return this_->promise_.get_future();
        }
    };

    async_func make_async_func() {
        async_func f = { this };
        return f;
    }

    void set_value() {
        promise_.set_value();
    }

private:
    promise<void> promise_;
};
}

namespace {

template<typename T>
struct future_is_ready {
    future<T> f_;

    future_is_ready(const wxl::async::future<T> & f)
        : f_(f)
    {}

    bool operator()() const {
        return f_.is_ready();
    }
};

template<typename T>
future_is_ready<T> is_ready(const wxl::async::future<T> & f)
{
    return future_is_ready<T>(f);
}
}

STRESS_TEST_CASE(future, then_completed_by_value_async)
{
    test_promise p1;
    test_future f1 = p1.get_future();

    async_future_tester tester;
    test_future f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_EQ(future_status::timeout, f1.wait_for(duration::from_ms(1)));
    tester.assert_nothing();

    joining_thread thread(&test_promise::set_value, &p1);

    WXL_REQUIRE_BECOME_TRUE(is_ready(f1), big_timeout);

    f1.get(); // should not throw

    ASSERT_EQ(future_status::timeout, f2.wait_for(duration::zero()));

    tester.set_value();

    WXL_REQUIRE_BECOME_TRUE(is_ready(f2), big_timeout);

    f2.get(); // should not throw
    tester.assert_value();
}

STRESS_TEST_CASE(future, then_completed_by_exception_async)
{
    test_promise p1;
    test_future f1 = p1.get_future();

    async_future_tester tester;

    test_future f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_EQ(future_status::timeout, f1.wait_for(duration::zero()));

    joining_thread thread(&test_promise::set_exception, &p1, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

STRESS_TEST_CASE(future, when_all_completed_by_value)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p1.set_value();
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p2.set_value();
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_all_completed_by_exception1)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_value();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_exception(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(future, when_all_completed_by_exception2)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_exception(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

namespace {
struct when_all_completed_by_value_async_func {
    test_promise p1;
    test_promise p2;
    test_future result;

    void operator()() {
        p1.set_value();
        ASSERT_TRUE(result.wait_for(duration::from_ms(1)) == future_status::timeout);
        p2.set_value();
    }
};
}

STRESS_TEST_CASE(future, when_all_completed_by_value_async)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(f1.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_by_value_async_func func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_EQ(future_status::ready , result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_value());
}

namespace {
struct when_all_completed_by_exception_async1_func {
    test_promise p1;
    test_promise p2;
    test_future result;

    void operator()() {
        p1.set_value();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_exception(specific_exception_instance);
    }
};
}

STRESS_TEST_CASE(future, when_all_completed_by_exception_async1)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_by_exception_async1_func func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_EQ(future_status::ready, result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(future, when_all_completed_by_exception_async2)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_succeeded_or_any_failed(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&test_promise::set_exception, &p1, specific_exception_instance);

    ASSERT_EQ(future_status::ready , result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(future, when_any_completed_by_value)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_value();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_any_completed_by_exception)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_exception(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(future, when_any_completed_by_value_async)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&test_promise::set_value, &p1);

    ASSERT_EQ(future_status::ready, result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_any_completed_by_exception_async)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_any(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&test_promise::set_exception, &p2, specific_exception_instance);

    ASSERT_EQ(future_status::ready , result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_exception());
    ASSERT_TRUE(!result.has_value());
}

STRESS_TEST_CASE(future, when_all_completed_completed_by_value)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_value();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_value();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

STRESS_TEST_CASE(future, when_all_completed_completed_by_value_and_exception)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::from_ms(1)) == future_status::timeout);
    p1.set_exception(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_value();
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
struct when_all_completed_completed_by_value_async_func {
    test_promise p1;
    test_promise p2;
    test_future result;

    void operator()() {
        p1.set_value();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_value();
    }
};
}

STRESS_TEST_CASE(future, when_all_completed_completed_by_value_async)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_async_func func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_EQ(future_status::ready , result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
struct when_all_completed_completed_by_value_and_exception_async_func {
    test_promise p1;
    test_promise p2;
    test_future result;

    void operator()() {
        p1.set_value();
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_exception(specific_exception_instance);
    }
};
}

STRESS_TEST_CASE(future, when_all_completed_completed_by_value_and_exception_async)
{
    test_promise p1;
    test_promise p2;
    test_future f1 = p1.get_future();
    test_future f2 = p2.get_future();

    test_future result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_and_exception_async_func func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_EQ(future_status::ready , result.wait_for(big_timeout));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}


namespace {
struct complex_completed_by_value_func {
    complex_completed_by_value_func() noexcept
        : counter_()
        , first_callback_n_()
        , second_callback_n_()
    {}

    void first_callback() {
        first_callback_n_ = ++counter_;
    }

    void second_callback() {
        second_callback_n_ = ++counter_;
    }

    int counter_;
    int first_callback_n_;
    int second_callback_n_;
};
}

STRESS_TEST_CASE(future, complex_completed_by_value)
{
    test_promise p;
    test_future f = p.get_future();

    complex_completed_by_value_func func;

    f.when_succeeded([&func]() noexcept { func.first_callback(); });
    f.when_succeeded([&func]() noexcept { func.second_callback(); });

    p.set_value();

    ASSERT_TRUE(func.first_callback_n_ > 0 && func.first_callback_n_ < 3);
    ASSERT_TRUE(func.second_callback_n_ > 0 && func.second_callback_n_ < 3);
    ASSERT_TRUE(func.first_callback_n_ != func.second_callback_n_);
}

namespace {
struct complex_completed_by_exception_func {
    complex_completed_by_exception_func() noexcept
        : counter_()
        , first_callback_n_()
        , second_callback_n_()
    {}

    void first_callback(const std::exception_ptr & ex_ptr) {
        first_callback_n_ = ++counter_;
        ex_ptr1 = ex_ptr;
    }

    void second_callback(const std::exception_ptr & ex_ptr) {
        second_callback_n_ = ++counter_;
        ex_ptr2 = ex_ptr;
    }

    int counter_;
    int first_callback_n_;
    std::exception_ptr ex_ptr1;
    int second_callback_n_;
    std::exception_ptr ex_ptr2;
};
}

STRESS_TEST_CASE(future, complex_completed_by_exception)
{
    test_promise p;
    test_future f = p.get_future();

    complex_completed_by_exception_func func;

    f.when_failed([&func](const std::exception_ptr& e) noexcept { func.first_callback(e); });
    f.when_failed([&func](const std::exception_ptr& e) noexcept { func.second_callback(e); });

    p.set_exception(specific_exception_instance);

    ASSERT_TRUE(func.first_callback_n_ > 0 && func.first_callback_n_ < 3);
    ASSERT_TRUE(func.second_callback_n_ > 0 && func.second_callback_n_ < 3);
    ASSERT_TRUE(func.first_callback_n_ != func.second_callback_n_);

    ASSERT_EQ(func.ex_ptr1, func.ex_ptr2);
}

// A `boost_future_has_deadlock` stress test used to live here, behind #if TEST_BOOST_FUTURE.
// It exercised a deadlock in boost::shared_future::then(), not in anything wxl owns, and
// wxl no longer depends on Boost at all -- so it was removed rather than ported.

// The combinators take a pack of any size, of futures of any mix of result types, or a range of
// them -- the cases the fixed two- and three-argument overloads could not express.

STRESS_TEST_CASE(future, when_all_ready_over_a_mixed_pack)
{
    test_promise p1;
    promise<int> p2;
    promise<std::string> p3;

    test_future result = when_all_ready(p1.get_future(), p2.get_future(), p3.get_future());
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p1.set_value();
    p2.set_value(42);
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p3.set_exception(specific_exception_instance);  // failing still counts as ready
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_all_ready_over_a_container)
{
    std::vector<test_promise> promises(4);
    std::vector<test_future> futures;

    for (test_promise& p : promises) futures.push_back(p.get_future());

    test_future result = when_all_ready(futures);

    for (test_promise& p : promises) {
        ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));
        p.set_value();
    }

    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
}

STRESS_TEST_CASE(future, when_all_succeeded_over_a_mixed_pack)
{
    test_promise p1;
    promise<int> p2;
    promise<std::string> p3;

    test_future result =
        when_all_succeeded_or_any_failed(p1.get_future(), p2.get_future(), p3.get_future());
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p1.set_value();
    p2.set_value(42);
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p3.set_value("done");
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_all_succeeded_over_a_mixed_pack_failing)
{
    test_promise p1;
    promise<int> p2;

    test_future result = when_all_succeeded_or_any_failed(p1.get_future(), p2.get_future());

    p2.set_exception(specific_exception_instance);
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_exception());
}

STRESS_TEST_CASE(future, when_any_over_a_mixed_pack)
{
    test_promise p1;
    promise<int> p2;

    test_future result = when_any(p1.get_future(), p2.get_future());
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p2.set_value(42);
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_value());
}

STRESS_TEST_CASE(future, when_any_over_an_empty_container)
{
    const std::vector<test_future> futures;

    ASSERT_TRUE(when_any(futures).is_ready());
}

// when_all is the one combinator that carries its futures out: a tuple for a pack, a vector for
// a range or a pair of iterators. It never fails -- every outcome is read off the future it
// belongs to, which is what lets the pack hold futures of differing result types.

STRESS_TEST_CASE(future, when_all_over_a_mixed_pack)
{
    test_promise p1;
    promise<int> p2;
    promise<std::string> p3;

    future<std::tuple<test_future, future<int>, future<std::string>>> result =
        when_all(p1.get_future(), p2.get_future(), p3.get_future());
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p1.set_value();
    p2.set_value(42);
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    p3.set_exception(specific_exception_instance);  // failing still counts as ready

    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_TRUE(result.has_value());  // and does not fail the combination

    ASSERT_TRUE(std::get<0>(result.get()).has_value());
    ASSERT_EQ(42, std::get<1>(result.get()).get());
    ASSERT_TRUE(std::get<2>(result.get()).has_exception());
}

STRESS_TEST_CASE(future, when_all_over_a_container)
{
    std::vector<promise<int>> promises(3);
    std::vector<future<int>> futures;

    for (promise<int>& p : promises) futures.push_back(p.get_future());

    future<std::vector<future<int>>> result = when_all(futures);

    int value = 0;

    for (promise<int>& p : promises) {
        ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));
        p.set_value(++value);
    }

    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_EQ(3u, result.get().size());
    ASSERT_EQ(2, result.get()[1].get());

    ASSERT_TRUE(futures[0].valid());  // copied in, not moved out of the caller's hands
}

STRESS_TEST_CASE(future, when_all_over_an_iterator_pair)
{
    std::vector<test_promise> promises(2);
    std::vector<test_future> futures;

    for (test_promise& p : promises) futures.push_back(p.get_future());

    future<std::vector<test_future>> result = when_all(futures.begin(), futures.end());

    promises[0].set_value();
    ASSERT_EQ(future_status::timeout, result.wait_for(duration::zero()));

    promises[1].set_value();
    ASSERT_EQ(future_status::ready, result.wait_for(duration::zero()));
    ASSERT_EQ(2u, result.get().size());
}

STRESS_TEST_CASE(future, when_all_over_nothing)
{
    const std::vector<test_future> none;

    ASSERT_TRUE(when_all(none).is_ready());
    ASSERT_TRUE(when_all(none).get().empty());

    future<std::tuple<>> nothing = when_all();

    ASSERT_TRUE(nothing.is_ready());
}

// A shared state whose producer is gone has to answer its waiters, and abandon() is the one
// answer: std::future_error(broken_promise). A promise reaches it by being destroyed; a
// producer that is not a promise -- wxl::async's component, whose stop ticket is its own
// shared state -- calls it directly on the way out.

TEST(FutureTest, ADestroyedPromiseBreaksItsFuture) {
    test_future f;

    {
        test_promise p;
        f = p.get_future();
        EXPECT_FALSE(f.is_ready());
    }

    ASSERT_TRUE(f.is_ready());
    ASSERT_TRUE(f.has_exception());

    try {
        f.get();
        FAIL() << "the future of a destroyed promise must not report success";
    }
    catch (const std::future_error & ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    }
}

TEST(FutureTest, AbandonBreaksAPendingStateTheSameWay) {
    // Adopts the reference the state is born with, instead of adding a second one.
    const future_shared_state_ptr state(new future_shared_state(future_shared_state::init_promise), false);
    const test_future f(state);

    EXPECT_FALSE(f.is_ready());

    state->abandon();

    ASSERT_TRUE(f.is_ready());

    try {
        f.get();
        FAIL() << "an abandoned state must not report success";
    }
    catch (const std::future_error & ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    }
}

TEST(FutureTest, AbandonLeavesAResolvedStateAlone) {
    const future_shared_state_ptr state(new future_shared_state(future_shared_state::init_promise), false);
    const test_future f(state);

    state->set_value();
    ASSERT_TRUE(f.is_ready());

    state->abandon();

    EXPECT_FALSE(f.has_exception());
    EXPECT_NO_THROW(f.get());
}
