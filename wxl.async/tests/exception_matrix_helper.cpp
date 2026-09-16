#include "exception_matrix_helper.h"

import std;
import wxl.core;
import wxl.async;


namespace wxl::async {

exception_matrix_helper & exception_matrix_helper::run_and_throw(method_throw_order::enum_ throw_order, component_method::enum_ method_where_throw)
{
    std::ostringstream oss;
    oss << "Throw " << throw_order << " " << method_where_throw;
    const std::string scenario = oss.str();

    start_exception_ = std::exception_ptr();
    start_ticket_ = ticket();
    stop_ticket_ = ticket();

    component_ = create_component(scenario);

    last_run_throw_method_ = method_where_throw;
    last_run_throw_order_ = throw_order;
    set_throw_method(throw_order, method_where_throw);

    component_.test->self_stop_ = true;

    try {
        start_ticket_ = component_.comp->start_async();
        stop_ticket_ = component_.comp->stop_ticket();

        try {
            if (start_ticket_.wait_for(big_timeout) == future_status::timeout)
                throw operation_canceled_exception("Component cannot start in scenario: " + scenario);

            start_ticket_.get();
        }
        catch (const specific_exception & ex) {
            start_exception_ = std::make_exception_ptr(ex);
        }

        EXPECT_TRUE(stop_ticket_.wait_for(big_timeout) == future_status::ready) << ("Component cannot stop in scenario: " + scenario);
    }
    catch (const specific_exception & ex) {
        start_exception_ = std::make_exception_ptr(ex);
    }

    return *this;
}

exception_matrix_helper & exception_matrix_helper::should_be_called(std::initializer_list<component_method::enum_> methods)
{
    for (component_method::enum_ method : methods)
        should_be_called_impl(method);

    return *this;
}

exception_matrix_helper & exception_matrix_helper::should_not_be_called(std::initializer_list<component_method::enum_> methods)
{
    for (component_method::enum_ method : methods)
        should_not_be_called_impl(method);

    return *this;
}

exception_matrix_helper & exception_matrix_helper::on_start(expected::enum_ expectation)
{
    std::ostringstream oss;
    oss << "current scenario: Throw " << last_run_throw_order_ << " " << last_run_throw_method_;

    if (expectation == expected::ok)
        EXPECT_TRUE(!start_exception_) << ("start should not throw, " + oss.str());
    else
        EXPECT_TRUE(exception_is<specific_exception>(start_exception_)) << ("start should throw specific_exception, " + oss.str());

    return *this;
}

exception_matrix_helper & exception_matrix_helper::start_ticket(expected::enum_ expectation)
{
    if (expectation == expected::ok)
        EXPECT_TRUE(start_ticket_.has_value()) << ("start ticket should be resolved successfully");
    else
        EXPECT_TRUE(start_ticket_.has_exception()) << ("start ticket should be rejected");

    return *this;
}

exception_matrix_helper & exception_matrix_helper::stop_ticket(expected::enum_ expectation)
{
    if (expectation == expected::ok)
        EXPECT_TRUE(stop_ticket_.has_value()) << ("stop ticket should be resolved successfully");
    else
        EXPECT_TRUE(stop_ticket_.has_exception()) << ("stop ticket should be rejected");

    return *this;
}

void exception_matrix_helper::set_throw_method(method_throw_order::enum_ throw_order, component_method::enum_ method_where_throw)
{
    if (throw_order == method_throw_order::before)
        component_.test->throw_before_method_ = method_where_throw;
    else
        component_.test->throw_after_method_ = method_where_throw;
}

void exception_matrix_helper::should_be_called_impl(component_method::enum_ method)
{
    std::ostringstream oss;
    oss << "Method \"" << method << "\" should be called. Current scenario: Throw " << last_run_throw_order_ << " " << last_run_throw_method_;

    EXPECT_TRUE(component_.test->was_called(method)) << (oss.str());
}

void exception_matrix_helper::should_not_be_called_impl(component_method::enum_ method)
{
    std::ostringstream oss;
    oss << "Method \"" << method << "\" should not be called. Current scenario: Throw " << last_run_throw_order_ << " " << last_run_throw_method_;

    EXPECT_TRUE(!component_.test->was_called(method)) << (oss.str());
}

}  // namespace wxl::async
