#pragma once

#include <gtest/gtest.h>

import std;
import wxl.core;



namespace wxl::core {

#define WXL_TEST_NAME(a, b) a##_##b

#define STRESS_TEST_BODY(suite_name, test_name) \
    TEST(suite_name, test_name) { \
        run_stress_test(WXL_TEST_NAME(suite_name, test_name)); \
    }

// Defines a stress test body with the signature: static void test_name(unit_test_tracer& tracer)
#define STRESS_TEST_CASE(suite_name, test_name) \
    static void WXL_TEST_NAME(suite_name, test_name)(unit_test_tracer &); \
    STRESS_TEST_BODY(suite_name, test_name) \
    static void WXL_TEST_NAME(suite_name, test_name)(unit_test_tracer & tracer)

// Defines a stress test body with the signature: static void test_name(unit_test_tracer& tracer, size_t number_of_elements)
#define STRESS_TEST_CASE_EX(suite_name, test_name) \
    static void WXL_TEST_NAME(suite_name, test_name)(unit_test_tracer &, size_t); \
    STRESS_TEST_BODY(suite_name, test_name) \
    static void WXL_TEST_NAME(suite_name, test_name)(unit_test_tracer & tracer, size_t number_of_elements)

inline constexpr duration big_timeout = duration::from_ms(5000);

/// Tracer handed to every stress test body; skipped_ lets a body abort the remaining iterations.
struct unit_test_tracer {
    bool skipped_ = false;

    /// Name of the gtest test currently running (used to name test components).
    static std::string name() {
        const ::testing::TestInfo * info = ::testing::UnitTest::GetInstance()->current_test_info();
        return info ? std::string(info->test_suite_name()) + "." + info->name() : "unnamed";
    }

    /// Narrates a checkpoint within the test body; purely diagnostic, no assertion value.
    void trace(std::string_view ) const noexcept {}
};

/// Skips the remaining stress-test iterations for the current test body -- used for
/// scenarios that are known-broken upstream and tracked by a ticket, matching the
/// original code's use of this exact escape hatch.
#define WXL_SKIP_THIS_TEST(ticket) \
    do { \
        tracer.skipped_ = true; \
        GTEST_SKIP() << "Skipped, tracked by " << (ticket); \
    } while (false)

/// \return true if the given std::exception_ptr, when rethrown, is (or derives from) T.
template<typename T>
inline bool exception_is(const std::exception_ptr & ep) {
    if (!ep)
        return false;

    try {
        std::rethrow_exception(ep);
    }
    catch (const T &) {
        return true;
    }
    catch (...) {
    }

    return false;
}

typedef void test_body(unit_test_tracer & tracer);

inline void run_stress_test(test_body test_body, size_t number_of_iterations) {
    unit_test_tracer tracer;

    for (size_t i = 0; i < number_of_iterations; ++i) {
        test_body(tracer);

        if (tracer.skipped_)
            break;
    }
}

inline void run_stress_test(test_body test_body) {
#ifdef NDEBUG
    constexpr size_t default_number_of_iterations = 1024;
#else
    constexpr size_t default_number_of_iterations = 17;
#endif

    run_stress_test(test_body, default_number_of_iterations);
}

typedef void test_body_ex(unit_test_tracer & tracer, size_t number_of_elements);

inline void run_stress_test(test_body_ex test_body, size_t number_of_iterations, size_t number_of_elements) {
    unit_test_tracer tracer;

    for (size_t i = 0; i < number_of_iterations; ++i) {
        test_body(tracer, number_of_elements);

        if (tracer.skipped_)
            break;
    }
}

inline void run_stress_test(test_body_ex test_body) {
#ifdef NDEBUG
    constexpr size_t max_number_of_iterations = 512;
#else
    constexpr size_t max_number_of_iterations = 30;
#endif

    run_stress_test(test_body, max_number_of_iterations, 1);
    run_stress_test(test_body, max_number_of_iterations, 3);

#ifdef NDEBUG
    run_stress_test(test_body, max_number_of_iterations, 5);
    run_stress_test(test_body, max_number_of_iterations, 9);
#endif
}

/// User-defined exception used to check that exceptions are moved between threads correctly.
class specific_exception : public std::logic_error {
public:
    specific_exception() noexcept
        : logic_error("Specific ExceptionPtr")
    {}
};

/// Polls predicate until it becomes true or the timeout elapses; always checks once more after the timeout.
template<typename t_predicate>
inline bool become_true(t_predicate predicate, duration timeout) {
    if (predicate())
        return true;

    const timeout_timer timer(timeout);

    while (timer.remaining()) {
        if (predicate())
            return true;

        std::this_thread::yield();
    }

    return predicate();
}

template<typename t_predicate>
inline bool become_true(t_predicate predicate, int timeout_in_ms) {
    assert(timeout_in_ms > 0);
    return become_true(predicate, duration::from_ms(timeout_in_ms));
}

#define WXL_REQUIRE_BECOME_TRUE(P, T) \
    ASSERT_TRUE(become_true((P), (T))) << "Predicate did not become true during timeout"

#define WXL_CHECK_BECOME_TRUE(P, T) \
    EXPECT_TRUE(become_true((P), (T))) << "Predicate did not become true during timeout"

}  // namespace wxl::core
