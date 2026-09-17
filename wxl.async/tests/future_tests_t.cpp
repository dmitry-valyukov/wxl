

#include <gtest/gtest.h>


#include "stress_test.h"


import std;
import wxl.core;
import wxl.async;

// Specify the following to run all Future tests:
// --run_test=System/FutureT/* --log_level=test_suite

using namespace wxl::core;
using namespace wxl::async;
using std::string;

// Test case template for Future<T> specializations
// where T is int/string/ExceptionPtr
#define FUTURE_T_TEST_CASE(test_name) \
    template<typename T> void WXL_TEST_NAME(test_name, body)(); \
    STRESS_TEST_CASE(future_t, test_name) \
    { \
        WXL_TEST_NAME(test_name, body)<int>(); \
        WXL_TEST_NAME(test_name, body)<string>(); \
        WXL_TEST_NAME(test_name, body)<std::exception_ptr>(); \
    } \
    template<typename T> void WXL_TEST_NAME(test_name, body)()

static std::exception_ptr specific_exception_instance = std::make_exception_ptr(specific_exception());

template<typename T>
struct test_value {
    static T instance;
};

template<> int test_value<int>::instance = 42;
template<> string test_value<string>::instance = "Test string";
template<> std::exception_ptr test_value<std::exception_ptr>::instance = specific_exception_instance;

template<typename T>
const T & get_test_value()
{
    return test_value<T>::instance;
}

// --run_test=System/FutureT/createWaitAndDispose --log_level=test_suite
FUTURE_T_TEST_CASE(create_wait_and_dispose)
{
    future<T> f;
    ASSERT_TRUE(!f.valid());

    {
        promise<T> p;

        f = p.get_future();
        ASSERT_TRUE(f.valid());

        const bool failed_with_timeout = (f.wait_for(duration::zero()) == future_status::timeout);
        ASSERT_TRUE(failed_with_timeout);
    }

    // here the promise was destroyed before the future was set to the ready state.
    ASSERT_THROW(f.get(), std::future_error);
}

namespace {
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

// --run_test=System/FutureT/memoryCheck --log_level=test_suite
FUTURE_T_TEST_CASE(memory_check)
{
    bool volatile run_flag = false;
    bool volatile delete_flag = false;

    {
        promise<T> p;
        future<T> f = p.get_future();

        {
            continuation cont(&run_flag, &delete_flag);
            f.when_ready(cont);
        }

        ASSERT_TRUE(!run_flag);
        ASSERT_TRUE(!delete_flag);

        p.set_value(get_test_value<T>());
    }

    ASSERT_TRUE(run_flag);
    ASSERT_TRUE(delete_flag);
}

// --run_test=System/FutureT/setValueTest --log_level=test_suite
FUTURE_T_TEST_CASE(set_value_test)
{
    promise<T> p;
    future<T> f = p.get_future();
    p.set_value(get_test_value<T>());

    ASSERT_EQ(f.get(), get_test_value<T>());
}

// --run_test=System/FutureT/setValueFromAnotherThread --log_level=test_suite
FUTURE_T_TEST_CASE(set_value_from_another_thread)
{
    promise<T> p;
    future<T> f = p.get_future();

    joining_thread thread(&promise<T>::set_value, &p, get_test_value<T>());

    ASSERT_EQ(f.get(), get_test_value<T>());
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(!f.has_exception());
}

// --run_test=System/FutureT/setExceptionTest --log_level=test_suite
FUTURE_T_TEST_CASE(set_exception_test)
{
    promise<T> p;
    future<T> f = p.get_future();

    p.set_exception(specific_exception_instance);

    ASSERT_THROW(f.get(), specific_exception);

    ASSERT_TRUE(!f.has_value());
    ASSERT_TRUE(f.has_exception());
}

// --run_test=System/FutureT/setExceptionFromAnotherThread --log_level=test_suite
FUTURE_T_TEST_CASE(set_exception_from_another_thread)
{
    promise<T> p;
    future<T> f = p.get_future();

    joining_thread thread(&promise<T>::set_exception, &p, specific_exception_instance);

    ASSERT_THROW(f.get(), specific_exception);
}

namespace {
template<typename T>
class future_tester : public noncopyable
{
    bool volatile succeeded_;
    T value_;
    bool volatile failed_;
    std::exception_ptr exception_;
    bool volatile ready_;

    void when_succeeded(T v) {
        succeeded_ = true;
        value_ = v;
    }

    struct when_succeeded_func {
        future_tester<T> * this_;
        void operator()(const T & value) noexcept {
            this_->when_succeeded(value);
        }
    };

    void when_failed(const std::exception_ptr & ex) {
        failed_ = true;
        exception_ = ex;
    }

    struct when_failed_func {
        future_tester<T> * this_;
        void operator()(const std::exception_ptr & ex) noexcept {
            this_->when_failed(ex);
        }
    };

    void when_ready() {
        ready_ = true;
    }

    struct when_ready_func {
        future_tester<T> * this_;
        void operator()() noexcept {
            this_->when_ready();
        }
    };

public:
    future_tester() noexcept
        : succeeded_()
        , value_()
        , failed_()
        , exception_()
        , ready_()
    {}

    future_tester(future<T> & f)
        : succeeded_()
        , value_()
        , failed_()
        , exception_()
        , ready_() {
        subscribe(f);
    }

    void subscribe(future<T> f) {
        const when_succeeded_func wd = { this };
        const when_failed_func wf = { this };
        const when_ready_func wc = { this };
        f.when_succeeded(wd);
        f.when_failed(wf);
        f.when_ready(wc);
    }

    struct func {
        typedef void result_type;

        future_tester<T> * this_;
        void operator()(const T & value) noexcept {
            this_->when_succeeded(value);
            this_->when_ready();
        }
    };

    func make_func() {
        func f = { this };
        return f;
    }

    void assert_value() {
        ASSERT_TRUE(succeeded_);
        ASSERT_EQ(value_, get_test_value<T>());
        ASSERT_TRUE(!failed_);
        ASSERT_EQ(exception_, std::exception_ptr());
        ASSERT_TRUE(ready_);
    }

    void assert_exception() {
        ASSERT_TRUE(!succeeded_);
        ASSERT_EQ(value_, T());
        ASSERT_TRUE(failed_);
        ASSERT_EQ(exception_, specific_exception_instance);
        ASSERT_TRUE(ready_);
    }

    void assert_nothing() {
        ASSERT_TRUE(!succeeded_);
        ASSERT_EQ(value_, T());
        ASSERT_TRUE(!failed_);
        ASSERT_EQ(exception_, std::exception_ptr());
        ASSERT_TRUE(!ready_);
    }
};
}

// --run_test=System/FutureT/whenReadyByValue --log_level=test_suite
FUTURE_T_TEST_CASE(when_ready_by_value)
{
    promise<T> p;
    future<T> f = p.get_future();

    future_tester<T> tester(f);
    tester.assert_nothing();

    p.set_value(get_test_value<T>());
    tester.assert_value();
}

// --run_test=System/FutureT/whenReadyByException --log_level=test_suite
FUTURE_T_TEST_CASE(when_ready_by_exception)
{
    promise<T> p;
    future<T> f = p.get_future();

    future_tester<T> tester(f);
    tester.assert_nothing();

    p.set_exception(specific_exception_instance);
    tester.assert_exception();
}

// --run_test=System/FutureT/lateWhenReadyByValue --log_level=test_suite
FUTURE_T_TEST_CASE(late_when_ready_by_value)
{
    promise<T> p;
    future<T> f = p.get_future();

    future_tester<T> tester;
    p.set_value(get_test_value<T>());
    tester.subscribe(f);

    tester.assert_value();
}

// --run_test=System/FutureT/lateWhenReadyByException --log_level=test_suite
FUTURE_T_TEST_CASE(late_when_ready_by_exception)
{
    promise<T> p;
    future<T> f = p.get_future();

    future_tester<T> tester;

    p.set_exception(specific_exception_instance);
    tester.subscribe(f);

    tester.assert_exception();
}

// --run_test=System/FutureT/nextCompletedByValue --log_level=test_suite
FUTURE_T_TEST_CASE(next_completed_by_value)
{
    promise<T> p;
    future<T> f1 = p.get_future();

    future_tester<T> tester;

    future<void> f2 = f1.next(tester.make_func());
    tester.assert_nothing();

    p.set_value(get_test_value<T>());
    tester.assert_value();

    f2.get();   // both should not throw
    f1.get();
}

// --run_test=System/FutureT/nextCompletedByException1 --log_level=test_suite
FUTURE_T_TEST_CASE(next_completed_by_exception1)
{
    promise<T> p;
    future<T> f1 = p.get_future();

    future_tester<T> tester;

    future<void> f2 = f1.next(tester.make_func());
    tester.assert_nothing();

    p.set_exception(specific_exception_instance);
    tester.assert_nothing();

    // first future was failed, so second future should fail too
    ASSERT_THROW(f2.get(), specific_exception);
}

// --run_test=System/FutureT/lateNextCompletedByException --log_level=test_suite
FUTURE_T_TEST_CASE(late_next_completed_by_exception)
{
    promise<T> p;
    future<T> f1 = p.get_future();

    // The future fails BEFORE the continuation is attached: next() has to take the
    // already-ready fast path, the way every other subscription does.
    p.set_exception(specific_exception_instance);

    future_tester<T> tester;
    future<void> f2 = f1.next(tester.make_func());

    // The continuation runs on success only; the exception goes to the future it returns.
    tester.assert_nothing();
    ASSERT_THROW(f2.get(), specific_exception);

    // And attaching must not have pushed the resolved state back to pending.
    ASSERT_TRUE(f1.is_ready());
    ASSERT_TRUE(f1.has_exception());
    ASSERT_THROW(f1.get(), specific_exception);
}

namespace {
template<typename T>
struct broken_continuation {
    typedef void result_type;

    void operator()(const T &) {
        throw specific_exception();
    }
};
}

// --run_test=System/FutureT/nextCompletedByException2 --log_level=test_suite
FUTURE_T_TEST_CASE(next_completed_by_exception2)
{
    promise<T> p;
    future<T> first_f = p.get_future();

    future<void> last_f = first_f.next(broken_continuation<T>());
    p.set_value(get_test_value<T>());

    // First future should not throw
    ASSERT_NO_THROW(first_f.get());

    // but last should
    ASSERT_THROW(last_f.get(), specific_exception);
}

// --run_test=System/FutureT/nextCompletedByValueAsync --log_level=test_suite
FUTURE_T_TEST_CASE(next_completed_by_value_async)
{
#ifdef NDEBUG
    const unsigned thread_count = 32;
#else
    const unsigned thread_count = 8;
#endif

    struct here {
        static void next_completed_by_value_async_body(barrier * enter_barrier, barrier * exit_barrier) {
            promise<T> p;
            future<T> f1 = p.get_future();

            future_tester<T> tester;

            future<void> f2 = f1.next(tester.make_func());
            ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
            tester.assert_nothing();

            enter_barrier->enter();
            thread_group::spawn_detached([&p] { p.set_value(get_test_value<T>()); });

            f2.get();
            tester.assert_value();

            exit_barrier->enter();
        }
    };

    thread_scope manager;
    barrier enter_barrier(thread_count + 1);
    barrier exit_barrier(thread_count + 1);

    for (unsigned i = 0; i < thread_count; i++) {
        const std::string name = "nextCompletedByValueAsync#" + std::to_string(i);

        manager->spawn_named(wxl::core::unicode::assume_valid(name), here::next_completed_by_value_async_body,
                             &enter_barrier, &exit_barrier);
    }

    here::next_completed_by_value_async_body(&enter_barrier, &exit_barrier);
}

// --run_test=System/FutureT/nextCompletedByExceptionAsync --log_level=test_suite
FUTURE_T_TEST_CASE(next_completed_by_exception_async)
{
    promise<T> p;
    future<T> f1 = p.get_future();

    future_tester<T> tester;

    future<void> f2 = f1.next(tester.make_func());
    ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
    tester.assert_nothing();

    joining_thread thread(&promise<T>::set_exception, &p, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

namespace {
template<typename T>
class async_future_tester : public future_tester<T>
{
    typedef future_tester<T> base;

public:
    struct async_func {
        async_future_tester<T> * this_;
        future<void> operator()(const T & value) {
            this_->make_func()(value);
            return this_->async_promise_.get_future();
        }
    };

    async_func make_async_func() {
        async_func f = { this };
        return f;
    }

    void set_value() {
        async_promise_.set_value();
    }

private:
    promise<void> async_promise_;
};
}

// --run_test=System/FutureT/thenCompletedByValueAsync --log_level=test_suite
FUTURE_T_TEST_CASE(then_completed_by_value_async)
{
    promise<T> p1;
    future<T> f1 = p1.get_future();

    async_future_tester<T> tester;
    future<void> f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_TRUE(f1.wait_for(duration::from_ms(1)) == future_status::timeout);
    tester.assert_nothing();

    joining_thread thread(&promise<T>::set_value, &p1, get_test_value<T>());

    ASSERT_TRUE(f1.wait_for(big_timeout) == future_status::ready);
    f1.get();

    ASSERT_TRUE(f2.wait_for(duration::zero()) == future_status::timeout);
    tester.set_value();
    ASSERT_TRUE(f2.wait_for(big_timeout) == future_status::ready);

    f2.get();
    tester.assert_value();
}

// --run_test=System/FutureT/nextCompletedByExceptionAsync --log_level=test_suite
FUTURE_T_TEST_CASE(then_completed_by_exception_async)
{
    promise<T> p1;
    future<T> f1 = p1.get_future();

    async_future_tester<T> tester;

    future<void> f2 = f1.next_unwrap(tester.make_async_func());

    ASSERT_TRUE(f1.wait_for(duration::zero()) == future_status::timeout);

    joining_thread thread(&promise<T>::set_exception, &p1, specific_exception_instance);

    ASSERT_THROW(f2.get(), specific_exception);
    tester.assert_nothing();
}

//// --run_test=System/FutureT/whenAllCompletedByValue --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByValue)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<void> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p1.set_value(testValue<T>());
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p2.set_value(testValue<T>());
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_value());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByException1 --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByException1)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<void> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p1.set_value(testValue<T>());
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p2.set_exception(specificExceptionInstance);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByException2 --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByException2)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p1.set_exception(specificExceptionInstance);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByValueAsync --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByValueAsync)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(f1.wait_for(duration::zero()) == FutureStatus::timeout);
//
//    ScopedThread thread([=]() mutable {
//        p1.set_value();
//        ASSERT_TRUE(result.wait_for(duration::from_ms(1)) == FutureStatus::timeout);
//        p2.set_value();
//    });
//
//    ASSERT_TRUE(result.wait(BigTimeout) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_value());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByExceptionAsync1 --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByExceptionAsync1)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//
//    ScopedThread thread([=]() mutable {
//        p1.set_value();
//        ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//        p2.set_exception(specificExceptionInstance);
//    });
//
//    ASSERT_TRUE(result.wait(BigTimeout) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByExceptionAsync2 --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAllCompletedByExceptionAsync2)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = whenAllSucceededOrAnyFailed(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//
//    ScopedThread thread([=]() mutable {
//        p1.set_exception(specificExceptionInstance);
//    });
//
//    ASSERT_TRUE(result.wait(BigTimeout) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByValue --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAnyCompletedByValue)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = when_any(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p1.set_value();
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_value());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByException --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAnyCompletedByException)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = when_any(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//    p2.set_exception(specificExceptionInstance);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//}
//
//// --run_test=System/FutureT/whenAnyCompletedByValueAsync --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAnyCompletedByValueAsync)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = when_any(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//
//    ScopedThread thread([=]() mutable {
//        p1.set_value();
//    });
//
//    ASSERT_TRUE(result.wait(BigTimeout) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_value());
//}
//
//// --run_test=System/FutureT/whenAllCompletedByExceptionAsync --log_level=test_suite
//FUTURE_T_TEST_CASE(whenAnyCompletedByExceptionAsync)
//{
//    Promise<T> p1;
//    Promise<T> p2;
//    Future<T> f1 = p1.get_future();
//    Future<T> f2 = p2.get_future();
//
//    Future<T> result = when_any(f1, f2);
//    ASSERT_TRUE(result.wait_for(duration::zero()) == FutureStatus::timeout);
//
//    ScopedThread thread([=]() mutable {
//        p2.set_exception(specificExceptionInstance);
//    });
//
//    ASSERT_TRUE(result.wait(BigTimeout) == FutureStatus::ready);
//    ASSERT_TRUE(result.has_exception());
//    ASSERT_TRUE(!result.has_value());
//}

// --run_test=System/FutureT/whenAllCompleted_CompletedByValue --log_level=test_suite
FUTURE_T_TEST_CASE(when_all_completed_completed_by_value)
{
    promise<T> p1;
    promise<T> p2;
    future<T> f1 = p1.get_future();
    future<T> f2 = p2.get_future();

    future<void> result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p1.set_value(get_test_value<T>());
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_value(get_test_value<T>());
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

// --run_test=System/FutureT/whenAllCompleted_CompletedByValueAndException --log_level=test_suite
FUTURE_T_TEST_CASE(when_all_completed_completed_by_value_and_exception)
{
    promise<T> p1;
    promise<T> p2;
    future<T> f1 = p1.get_future();
    future<T> f2 = p2.get_future();

    future<void> result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::from_ms(1)) == future_status::timeout);
    p1.set_exception(specific_exception_instance);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
    p2.set_value(get_test_value<T>());
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
template<typename T>
struct when_all_completed_completed_by_value_async_func {
    promise<T> p1;
    promise<T> p2;
    future<void> result;

    void operator()() {
        p1.set_value(get_test_value<T>());
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_value(get_test_value<T>());
    }
};
}

// --run_test=System/FutureT/whenAllCompleted_CompletedByValueAsync --log_level=test_suite
FUTURE_T_TEST_CASE(when_all_completed_completed_by_value_async)
{
    promise<T> p1;
    promise<T> p2;
    future<T> f1 = p1.get_future();
    future<T> f2 = p2.get_future();

    future<void> result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_async_func<T> func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}

namespace {
template<typename T>
struct when_all_completed_completed_by_value_and_exception_async_func {
    promise<T> p1;
    promise<T> p2;
    future<void> result;

    void operator()() {
        p1.set_value(get_test_value<T>());
        ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);
        p2.set_exception(specific_exception_instance);
    }
};
}

// --run_test=System/FutureT/whenAllCompleted_CompletedByValueAndExceptionAsync --log_level=test_suite
FUTURE_T_TEST_CASE(when_all_completed_completed_by_value_and_exception_async)
{
    promise<T> p1;
    promise<T> p2;
    future<T> f1 = p1.get_future();
    future<T> f2 = p2.get_future();

    future<void> result = when_all_ready(f1, f2);
    ASSERT_TRUE(result.wait_for(duration::zero()) == future_status::timeout);

    when_all_completed_completed_by_value_and_exception_async_func<T> func = { p1, p2, result };

    joining_thread thread(func);

    ASSERT_TRUE(result.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(!result.has_exception());
}
