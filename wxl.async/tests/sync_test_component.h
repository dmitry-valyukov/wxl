#pragma once

#include "sync_component.h"
#include "test_component.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

// The test components live in wxl::async and reach for wxl.core names (traceable,
// sync_root) as freely as the library itself does.
using namespace wxl::core;

/// Test infrastructure for a synchronously started component.
class sync_test_component : public test_component_impl<sync_component>
{
public:
    explicit sync_test_component(std::string_view name = "sync component", thread_group * = nullptr) noexcept
        : test_component_impl<sync_component>(name)
    {}

    ~sync_test_component() override {
        dispose();
    }
};

}  // namespace wxl::async
