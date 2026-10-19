#pragma once

#include <gtest/gtest.h>

#include "platform.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

namespace impl {

/// The loop on a dispatcher queue, standing for the whole binary: the mode an
/// application with a window runs. The thread that runs the tests is the one with the
/// queue, and it waits the way such a thread does -- in its message loop.
class dispatched_environment : public ::testing::Environment
{
public:
    void SetUp() override { sta_loop::start_dispatched("wxl dispatched tests: I/O"); }

    void TearDown() override { sta_loop::stop(); }
};

inline ::testing::Environment* const dispatched_env =
    ::testing::AddGlobalTestEnvironment(new dispatched_environment);

}  // namespace impl

}  // namespace wxl::async
