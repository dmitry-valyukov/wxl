#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

threaded_test_component::threaded_test_component(std::string_view name, thread_group * manager, bool call_dispose) noexcept
    : test_component_impl<threaded_component>(name, manager)
    , wake_up_(0)
    , call_dispose_(call_dispose)
{}

threaded_test_component::threaded_test_component(std::string_view name, thread_group * manager, not_null<wxl::core::sync_root> sync_root_arg, bool call_dispose)
    : test_component_impl<threaded_component>(name, manager, sync_root_arg)
    , wake_up_(0)
    , call_dispose_(call_dispose)
{}

threaded_test_component::~threaded_test_component()
{
    if (call_dispose_)
        dispose();
    else
        assert(is_disposed());
}

void threaded_test_component::run()
{
    on_before(component_method::run);

    if (self_stop_)
        stop_async();

    if (!stop_requested())
        wake_up_.acquire();

    on_after(component_method::run);
}

void threaded_test_component::on_stopping()
{
    test_component_impl<threaded_component>::on_stopping();
    wake_up_.release();
}

threaded_test_component::exception_matrix_helper::~exception_matrix_helper() = default;

threaded_test_component::exception_matrix_helper::component_interfaces
threaded_test_component::exception_matrix_helper::create_component(std::string_view suffix)
{
    manager_->join_all();
    component_.reset(new threaded_test_component(name() + ": " + std::string(suffix), manager_));
    return component_interfaces{component_.get(), component_.get()};
}

}  // namespace wxl::async
