#pragma once



#include "exception_matrix_helper.h"
#include "test_component.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

// The test components live in wxl::async and reach for wxl.core names (traceable,
// sync_root) as freely as the library itself does.
using namespace wxl::core;

class threaded_test_component : public test_component_impl<threaded_component>
{
public:
    explicit threaded_test_component(std::string_view name = "threaded component", thread_group * manager = nullptr, bool call_dispose = true) noexcept;

    threaded_test_component(std::string_view name, thread_group * manager, not_null<wxl::core::sync_root> sync_root_arg, bool call_dispose = true);

    ~threaded_test_component() override;

    class exception_matrix_helper : public wxl::async::exception_matrix_helper
    {
    public:
        ~exception_matrix_helper() override;

    protected:
        component_interfaces create_component(std::string_view suffix) override;

    private:
        thread_scope manager_;
        std::unique_ptr<threaded_test_component> component_;
    };

protected:
    void run() override;
    void on_stopping() override;

private:
    semaphore wake_up_;
    bool const call_dispose_;
};

}  // namespace wxl::async
