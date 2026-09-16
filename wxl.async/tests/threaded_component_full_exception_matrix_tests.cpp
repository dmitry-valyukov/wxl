#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/fullExceptionMatrix --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, fullExceptionMatrix)
{
    threaded_test_component::exception_matrix_helper()

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::before, component_method::on_starting)
    .on_start(expected::fail)
    .should_be_called({
        component_method::on_starting,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .should_not_be_called({
        component_method::on_started,
        component_method::run,
    })

    //-------------------------------------------------------------------------------------
    // Бросок после on_starting: остановка отсюда уходит своим ходом
    // (component::start_async), и к возврату из него её колбэки ещё не вызваны --
    // сценарий проверяет только несостоявшийся старт.
    .run_and_throw(method_throw_order::after, component_method::on_starting)
    .on_start(expected::fail)
    .should_be_called({
        component_method::on_starting,
    })
    .should_not_be_called({
        component_method::on_started,
        component_method::run,
    })

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::before, component_method::on_started)
    .on_start(expected::fail)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .should_not_be_called({
        component_method::run,
    })
    .start_ticket(expected::fail)
    .stop_ticket(expected::fail)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::after, component_method::on_started)
    .on_start(expected::fail)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .should_not_be_called({
        component_method::run,
    })
    .start_ticket(expected::fail)
    .stop_ticket(expected::fail)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::before, component_method::run)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::fail)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::after, component_method::run)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::ok)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::before, component_method::on_stopping)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::ok)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::after, component_method::on_stopping)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::ok)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::before, component_method::on_stopped)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::ok)

    //-------------------------------------------------------------------------------------
    .run_and_throw(method_throw_order::after, component_method::on_stopped)
    .on_start(expected::ok)
    .should_be_called({
        component_method::on_starting,
        component_method::on_started,
        component_method::run,
        component_method::on_stopping,
        component_method::on_stopped,
    })
    .start_ticket(expected::ok)
    .stop_ticket(expected::ok);
}
