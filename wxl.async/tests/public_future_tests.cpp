

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage

#include <gtest/gtest.h>


#include "stress_test.h"

import std;
import wxl.core;
import wxl.async;

// This is a template for public API-s for SharedFuture's

// The following defines are used:
#define WXL_SOMEAPI_EXPORTED

#ifdef WXL_CORE_CXX11
#define WXL_SOMEAPI_CXX11
#endif

#define WXL_SOMEAPI_NOEXCEPT noexcept
#define WXL_SOMEAPI_NULLPTR nullptr

// replace above defines with ones from target Handler

using namespace wxl::core;
using namespace wxl::async;

// ============================================================================================================================
namespace some_handler::threading {

/// State of a \c SharedFuture object (similar to std::future_status, @see http://en.cppreference.com/w/cpp/thread/future_status ).
struct future_status
{
    enum enum_
    {
        /// the shared state is ready.
        ready,
        /// the shared state did not become ready before specified timeout duration has passed.
        timeout,
        /// the shared state contains a deferred function, so the result will be computed only when explicitly requested.
        deferred
    };
};

namespace implementation {

class future_helper;

/// Base implementation of SharedFuture<T>.
class future_base
{
public:
    /// Check if a future instance is associated with an asynchronous result.
    ///
    /// Returns \c true if the *this has an associated asynchronous result, false otherwise.
    bool valid() const WXL_SOMEAPI_NOEXCEPT {
        return state_ != nullptr;
    }

    /// Returns \c true if the asynchronous result associated with this Future is ready
    /// (has a value or exception stored in the shared state), \c false otherwise.
    ///
    /// There are often situations where a \c get() call on a Future may not be a blocking call,
    /// or is only a blocking call under certain circumstances.
    /// This method gives the ability to test for early completion and allows us to avoid
    /// associating a continuation, which needs to be scheduled with some non-trivial overhead
    /// and near-certain loss of cache efficiency.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    WXL_SOMEAPI_EXPORTED bool is_ready() const;

    /// Returns \c true if the asynchronous result associated with this Future has a stored value,
    ///         \c false otherwise.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    WXL_SOMEAPI_EXPORTED bool has_value() const;

    /// Returns \c true if the asynchronous result associated with this Future has a stored exception,
    ///         \c false otherwise.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    WXL_SOMEAPI_EXPORTED bool has_exception() const;

#ifdef WXL_SOMEAPI_CXX11

    /// Returns the stored exception.
    ///
    /// @throw std::logic_error if this instance does not refer to a shared state.
    /// @throw std::logic_error if the operation has *not* finished with an error.
    WXL_SOMEAPI_EXPORTED std::exception_ptr get_exception_ptr() const;

#endif

    enum { infinite_timeout = -1 };

    /// Waits for the result to become available during the timeout.
    ///
    /// \note Calling \c wait on the same Future from multiple threads is \e not safe;
    /// the intended use is for each thread that waits on the same shared state to have a \e copy of a Future.
    ///
    /// \throw std::logic_error if this instance does not refer to a shared state.
    WXL_SOMEAPI_EXPORTED future_status::enum_ wait(int timeout_in_ms) const;

protected:
    future_base()
        : state_() {}

    WXL_SOMEAPI_EXPORTED future_base(const future_base & other) WXL_SOMEAPI_NOEXCEPT;

    WXL_SOMEAPI_EXPORTED future_base & operator=(const future_base & other) WXL_SOMEAPI_NOEXCEPT;

#ifdef WXL_SOMEAPI_CXX11

    WXL_SOMEAPI_EXPORTED future_base(future_base && other) WXL_SOMEAPI_NOEXCEPT;

    WXL_SOMEAPI_EXPORTED future_base & operator=(future_base && other) WXL_SOMEAPI_NOEXCEPT;

#endif

    struct moving_init_t {};

    /// Initializes the instance with shared state.
    WXL_SOMEAPI_EXPORTED future_base(const future_shared_state * state) WXL_SOMEAPI_NOEXCEPT;

    /// Initializes the instance with shared state.
    WXL_SOMEAPI_EXPORTED future_base(const future_shared_state * state, moving_init_t) WXL_SOMEAPI_NOEXCEPT;

    /// Destroys a future object.
    ///
    /// If this is the last reference to the asynchronous result associated with *this (if any), then destroy that asynchronous result.
    WXL_SOMEAPI_EXPORTED ~future_base() WXL_SOMEAPI_NOEXCEPT;

    WXL_SOMEAPI_EXPORTED void swap(future_base & other) WXL_SOMEAPI_NOEXCEPT;

    WXL_SOMEAPI_EXPORTED const void * get_value_ptr() const;

    WXL_SOMEAPI_EXPORTED void get_void() const;

private:
    const future_shared_state * state_;

    friend class future_helper;
};

template<typename T>
struct future_get_return {
    typedef const T & type;
};

template<>
struct future_get_return<void> {
    typedef void type;
};

}

/// Represents a future result of an asynchronous operation - a result that will eventually appear in the Future after
/// the processing is complete.
///
/// The class template Future provides a mechanism to access the result of \e asynchronous operations :
/// \li An asynchronous operation (e.g. created via Promise) can provide a Future object to the creator of that asynchronous
/// operation (e.g. via Promise::get_future).
/// \li The creator of the asynchronous operation can then use a variety of methods to query, wait for, or extract a value from Future.
/// These methods may block if the asynchronous operation has not yet provided a value.
/// \li When the asynchronous operation is ready to send a result to the creator, it can do so by modifying <em> shared state </em>
/// (e.g. via Promise::set_value) that is linked to the creator's Future.
///
/// Future is the synchronization object constructed around the \e receiving end of the Promise channel. It allows for the separation
/// of the initiation of an operation and the act of waiting for its result.
///
/// Future is \e copyable and multiple SharedFuture objects may refer to the same shared state.
///
/// Unlike std::future, this implementation has an extended API, allowing you to query the state of SharedFuture.
/// The internal implementation is lock-free; therefore, setting and polling states do not lead to blocking of threads and/or switching
/// of an execution context.
///
/// \note Access to the same shared state from multiple threads is safe if each thread does it through its own copy of
/// a SharedFuture object.
///
/// \see http://en.cppreference.com/w/cpp/thread/shared_future .
template <typename T>
class shared_future : public implementation::future_base
{
public:
    shared_future() WXL_SOMEAPI_NOEXCEPT
        : future_base() {}

    /// Copy constructor.
    shared_future(const shared_future<T> & other) WXL_SOMEAPI_NOEXCEPT
        : future_base(other) {}

    /// Copy assignment.
    shared_future<T> & operator=(const shared_future<T> & other) WXL_SOMEAPI_NOEXCEPT {
        future_base::operator=(other);
        return *this;
    }

#ifdef WXL_SOMEAPI_CXX11

    shared_future(shared_future<T> && other) WXL_SOMEAPI_NOEXCEPT
        : future_base(std::move(other)) {}

    future_base & operator=(shared_future<T> && other) WXL_SOMEAPI_NOEXCEPT {
        future_base::operator=(std::move(other));
        return *this;
    }

#endif

    /// Returns the result. If the result is not ready, the method will block. When completes, it either returns a value or throws an exception.
    ///
    /// This method waits until the Future has a valid result and retrieves it. It effectively calls wait() in order to wait for the result.
    ///
    /// \exception If an exception was stored in the shared state referenced by this Future (e.g. via a call to Promise::set_exception()) then that exception will be thrown.
    typename implementation::future_get_return<T>::type get() const {
        return *reinterpret_cast<const T *>(get_value_ptr());
    }

    /// swaps two SharedFuture objects
    void swap(shared_future<T> & other) WXL_SOMEAPI_NOEXCEPT {
        future_base::swap(other);
    }

private:
    /// Initializes the instance with shared state.
    shared_future(const future_shared_state * state)
        : future_base(state) {}

    /// Initializes the instance with shared state.
    shared_future(const future_shared_state * state, moving_init_t t)
        : future_base(state, t) {}

    friend class implementation::future_helper;
};

template<>
inline void shared_future<void>::get() const {
    get_void();
}

}  // namespace some_handler::threading
// ============================================================================================================================

// ============================================================================================================================
// File SomeHandler/src/SharedFuture.cpp
namespace some_handler::threading::implementation {

using future_detail::future_accessor;

bool future_base::is_ready() const {
    return future_accessor::checked(state_)->ready();
}

bool future_base::has_value() const {
    return future_accessor::checked(state_)->has_value();
}

bool future_base::has_exception() const {
    return future_accessor::checked(state_)->has_exception();
}

#ifdef WXL_SOMEAPI_CXX11

std::exception_ptr future_base::get_exception_ptr() const {
    return future_accessor::checked(state_)->get_exception_ptr();
}

#endif

future_status::enum_ future_base::wait(int timeout_in_ms) const {
    const wxl::async::future_shared_state * state = future_accessor::checked(state_);

    // The foreign convention -- milliseconds, and negative for "no deadline" -- is read here,
    // at the boundary that speaks it, and becomes the two calls wxl has. Past this point
    // nothing carries "forever" as a value.
    if (const nullable<duration> timeout = timeout_from_ms(timeout_in_ms))
        return future_status::enum_(static_cast<int>(state->wait_for(*timeout)));

    state->wait();
    return future_status::ready;
}

future_base::future_base(const future_base & other) WXL_SOMEAPI_NOEXCEPT
    : state_(other.state_) {
    if(state_)
        intrusive_ptr_add_ref(state_);
}

future_base & future_base::operator=(const future_base & other) WXL_SOMEAPI_NOEXCEPT {
    if(state_ != other.state_) {
        const future_shared_state * old_state = state_;
        const future_shared_state * new_state = other.state_;
        state_ = new_state;

        if(new_state)
            intrusive_ptr_add_ref(new_state);

        if(old_state)
            intrusive_ptr_release(old_state);
    }

    return *this;
}

#ifdef WXL_SOMEAPI_CXX11

future_base::future_base(future_base && other) WXL_SOMEAPI_NOEXCEPT
    : state_(other.state_) {
    other.state_ = WXL_SOMEAPI_NULLPTR;
}

future_base & future_base::operator=(future_base && other) WXL_SOMEAPI_NOEXCEPT {
    if(state_ != other.state_) {
        const future_shared_state * old_state = state_;
        state_ = other.state_;
        other.state_ = WXL_SOMEAPI_NULLPTR;

        if(old_state)
            intrusive_ptr_release(old_state);
    }

    return *this;
}
#endif

future_base::future_base(const future_shared_state * state) WXL_SOMEAPI_NOEXCEPT
    : state_(state) {
    if(state_)
        intrusive_ptr_add_ref(state_);
}

future_base::future_base(const future_shared_state * state, moving_init_t) WXL_SOMEAPI_NOEXCEPT
    : state_(state) {}

future_base::~future_base() WXL_SOMEAPI_NOEXCEPT {
    if(state_)
        intrusive_ptr_release(state_);
}

void future_base::swap(future_base & other) WXL_SOMEAPI_NOEXCEPT {
    std::swap(state_, other.state_);
}

const void * future_base::get_value_ptr() const {
    const future_detail::state * state = future_accessor::checked(state_)->get();

    assert(dynamic_cast<const future_detail::resolved_state_base*>(state));
    return static_cast<const future_detail::resolved_state_base*>(state)->raw_data();
}

void future_base::get_void() const {
    const future_detail::state * state = future_accessor::checked(state_)->get();
    (void)state;
}

}  // namespace some_handler::threading::implementation
// EOF SomeHandler/src/SharedFuture.cpp
// ============================================================================================================================

// ============================================================================================================================
// File SomeHandler/src/FutureHelper.h
namespace some_handler::threading::implementation {

class future_helper
{
public:
    template <typename T>
    static shared_future<T> copy_to_public(const future<T> & f) {
        return shared_future<T>(future_accessor::state_of(f));
    }

#ifdef WXL_SOMEAPI_CXX11
    template <typename T>
    static shared_future<T> move_to_public(future<T> && f) {
        return shared_future<T>(future_accessor::detach_state(f), future_base::moving_init_t());
    }
#else
    template <typename T>
    static shared_future<T> move_to_public(future<T> f) {
        return shared_future<T>(future_accessor::detach_state(f), future_base::moving_init_t());
    }
#endif
};
}  // namespace some_handler::threading::implementation
// EOF SomeHandler/src/FutureHelper.h
// ============================================================================================================================

// Test stuff
// could be copied to target Handler's tests

typedef promise<void> test_promise;

namespace {
/// Returns \c true if the given Future object has the exception of the \e E type, \c false otherwise.
template <typename E, typename T>
bool has_exception(const some_handler::threading::shared_future<T> & f) {
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

std::exception_ptr specific_exception_instance = std::make_exception_ptr(specific_exception());

template<typename T>
struct test_value_holder {
    static T instance;
};

template<> int test_value_holder<int>::instance = 42;
template<> std::string test_value_holder<std::string>::instance = "Test string";
template<> std::exception_ptr test_value_holder<std::exception_ptr>::instance = specific_exception_instance;

template<typename T>
const T & test_value()
{
    return test_value_holder<T>::instance;
}

}

// --run_test=System/publicFutureTest
TEST(PublicFutureTest, public_future_test)
{
    using namespace some_handler::threading::implementation;
    typedef some_handler::threading::shared_future<void> test_future;

    {
        // Create and dispose

        test_future f;
        ASSERT_TRUE(!f.valid());

        {
            const test_promise p;
            f = future_helper::move_to_public(p.get_future());

            ASSERT_TRUE(f.valid());
            ASSERT_TRUE(!f.is_ready());

            f = test_future();
            ASSERT_TRUE(!f.valid());

            test_future f2 = future_helper::move_to_public(p.get_future());
            ASSERT_TRUE(f2.valid());

            f2 = f;
            ASSERT_TRUE(!f2.valid());
        }

        // SharedStateHolder does not refers to a shared state:
        ASSERT_THROW(f.get(), std::logic_error);
        ASSERT_THROW(f.is_ready(), std::logic_error);
        ASSERT_THROW(f.has_exception(), std::logic_error);
        ASSERT_THROW(f.has_value(), std::logic_error);

        {
            const test_promise p;

            f = future_helper::move_to_public(p.get_future());

            ASSERT_EQ(some_handler::threading::future_status::timeout, f.wait(1));
            ASSERT_TRUE(f.valid());
            ASSERT_TRUE(!f.is_ready());
        }

        // Here the Promise was destroyed before the corresponding Future had been set to the Ready state.
        ASSERT_THROW(f.get(), std::future_error);
        ASSERT_TRUE(f.is_ready());
        ASSERT_TRUE(f.has_exception());
        ASSERT_TRUE(!f.has_value());
    }

    {
        // Create, move and dispose

        test_future f1;

        {
            test_promise p1, p2;

            f1 = future_helper::move_to_public(p1.get_future());
            const test_future f2 = future_helper::move_to_public(p2.get_future());

            ASSERT_TRUE(p1.valid());
            ASSERT_TRUE(p2.valid());

            ASSERT_TRUE(!f1.is_ready());
            ASSERT_TRUE(!f2.is_ready());

            p2 = std::move(p1);

            ASSERT_TRUE(!p1.valid());
            ASSERT_TRUE(p2.valid());

            // Here the promise p2 was destroyed before the corresponding Future had been set to the Ready state.
            ASSERT_TRUE(has_exception<std::future_error>(f2));
            ASSERT_TRUE(!f1.is_ready());
        }

        // Here the promise p1 was destroyed before the corresponding Future had been set to the Ready state.
        ASSERT_TRUE(has_exception<std::future_error>(f1));
    }

    {
        // Create, copy, move and dispose

        test_promise p1;
        // ReSharper disable once CppInitializedValueIsAlwaysRewritten
        test_promise p2 = p1; // two owners over a single SharedState.

        const test_future f = future_helper::move_to_public(p1.get_future());

        ASSERT_EQ(some_handler::threading::future_status::timeout, f.wait(1));

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

    {
        // set value

        test_promise p;
        p.set_value();

        const test_future f = future_helper::move_to_public(p.get_future());

        ASSERT_TRUE(f.valid());
        f.get();
        ASSERT_TRUE(f.valid()); // still valid

        ASSERT_TRUE(f.is_ready());
        ASSERT_TRUE(f.has_value());
        ASSERT_TRUE(!f.has_exception());
    }

    {
        // set value from another thread

        test_promise p;
        const test_future f = future_helper::move_to_public(p.get_future());

        joining_thread thread(&test_promise::set_value, &p);

        f.get();

        ASSERT_TRUE(f.is_ready());
        ASSERT_TRUE(f.has_value());
        ASSERT_TRUE(!f.has_exception());
    }

    {
        // set exception

        test_promise p;
        const test_future f = future_helper::move_to_public(p.get_future());

        p.set_exception(specific_exception_instance);

        const std::exception_ptr second_exception = std::make_exception_ptr(std::logic_error("The second exception is ignored"));

        p.set_exception(second_exception);

        ASSERT_THROW(f.get(), specific_exception);

        ASSERT_TRUE(!f.has_value());
        ASSERT_TRUE(f.has_exception());

#ifdef WXL_SOMEAPI_CXX11
        ASSERT_EQ(specific_exception_instance, f.get_exception_ptr());
#endif
    }

    {
        // set exception from another thread

        test_promise p;
        const test_future f = future_helper::move_to_public(p.get_future());

        joining_thread thread(&test_promise::set_exception, &p, specific_exception_instance);

        ASSERT_THROW(f.get(), specific_exception);

#ifdef WXL_SOMEAPI_CXX11
        ASSERT_EQ(specific_exception_instance, f.get_exception_ptr());
#endif
    }
}

// Test case template for Future<T> specializations
// where T is int/string/ExceptionPtr
#define PUBLIC_FUTURE_T_TEST_CASE(test_name) \
    template<typename T> void WXL_TEST_NAME(test_name, body)(); \
    TEST(PublicFutureTest, test_name) \
    { \
        WXL_TEST_NAME(test_name, body)<int>(); \
        WXL_TEST_NAME(test_name, body)<std::string>(); \
        WXL_TEST_NAME(test_name, body)<std::exception_ptr>(); \
    } \
    template<typename T> void WXL_TEST_NAME(test_name, body)()

// --run_test=System/publicFutureTestT --log_level=test_suite
PUBLIC_FUTURE_T_TEST_CASE(public_future_test_t)
{
    using namespace some_handler::threading::implementation;
    using namespace some_handler::threading;

    {
        // set value

        promise<T> p;
        shared_future<T> f = future_helper::move_to_public(p.get_future());
        p.set_value(test_value<T>());

        ASSERT_EQ(f.get(), test_value<T>());
    }

    {
        // set value from another thread

        promise<T> p;
        shared_future<T> f = future_helper::move_to_public(p.get_future());

        joining_thread thread(&promise<T>::set_value, &p, test_value<T>());

        ASSERT_EQ(f.get(), test_value<T>());
        ASSERT_TRUE(f.has_value());
        ASSERT_TRUE(!f.has_exception());
    }

}
