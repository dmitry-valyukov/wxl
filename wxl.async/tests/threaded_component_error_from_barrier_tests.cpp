#include <gtest/gtest.h>


#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

class error_from_barrier_exception : public std::logic_error {
public:
    error_from_barrier_exception() : logic_error("Error from barrier") {}
};

}  // namespace

// --run_test=System/ThreadedComponent/errorFromBarrier --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, errorFromBarrier)
{
    threaded_test_component component(tracer.name());
    task_tree_handle barrier = task_tree::create();

    const ticket start_ticket = component.start_async(barrier);
    start_ticket.get();
    future<void> stop_ticket = component.stop_ticket();

    ASSERT_TRUE(!component.was_called(component_method::run));
    ASSERT_TRUE(!stop_ticket.is_ready());

    barrier.set_failed(std::make_exception_ptr(error_from_barrier_exception()));
    ASSERT_TRUE(stop_ticket.wait_for(big_timeout) == future_status::ready);

    ASSERT_TRUE(*component.stop_reason_ptr());
    ASSERT_TRUE(exception_is<error_from_barrier_exception>(*component.stop_reason_ptr()));

    ASSERT_TRUE(component.was_called(component_method::on_started));
    ASSERT_TRUE(!component.was_called(component_method::run));
    ASSERT_TRUE(component.was_called(component_method::on_stopped));

    ASSERT_THROW(stop_ticket.get(), error_from_barrier_exception);
}
