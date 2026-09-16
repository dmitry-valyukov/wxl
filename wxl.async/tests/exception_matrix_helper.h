#pragma once



#include "test_component.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

// The test components live in wxl::async and reach for wxl.core names (traceable,
// sync_root) as freely as the library itself does.
using namespace wxl::core;

/// Runs a component through "throw at method X, before/after" fault-injection scenarios
/// and asserts on the expected sequence of lifecycle-method calls and ticket outcomes.
class exception_matrix_helper : public traceable
{
    typedef traceable base;

protected:
    exception_matrix_helper() noexcept
        : base(unit_test_tracer::name())
    {}

public:
    virtual ~exception_matrix_helper() = default;

    exception_matrix_helper & run_and_throw(method_throw_order::enum_ throw_order, component_method::enum_ method_where_throw);

    exception_matrix_helper & should_be_called(std::initializer_list<component_method::enum_> methods);

    exception_matrix_helper & should_not_be_called(std::initializer_list<component_method::enum_> methods);

    exception_matrix_helper & on_start(expected::enum_ expectation);

    exception_matrix_helper & start_ticket(expected::enum_ expectation);

    exception_matrix_helper & stop_ticket(expected::enum_ expectation);

protected:
    struct component_interfaces {
        test_component * test;
        component * comp;
    };

    virtual void set_throw_method(method_throw_order::enum_ throw_order, component_method::enum_ method_where_throw);

    virtual component_interfaces create_component(std::string_view suffix) = 0;

private:
    void should_be_called_impl(component_method::enum_ method);

    void should_not_be_called_impl(component_method::enum_ method);

    component_method::enum_ last_run_throw_method_ = component_method::not_set;
    method_throw_order::enum_ last_run_throw_order_ = method_throw_order::before;

    ticket start_ticket_;
    ticket stop_ticket_;
    std::exception_ptr start_exception_;

    component_interfaces component_{};
};

}  // namespace wxl::async
