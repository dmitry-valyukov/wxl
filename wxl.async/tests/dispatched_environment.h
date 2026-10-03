#pragma once

#include <gtest/gtest.h>

#include "platform.h"

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

/// Manual-reset, set just before the loop is stopped: for a body a test leaves out on
/// purpose, to stand on until then.
inline core::hevent loop_stopping{true};

namespace impl {

/// The loop on a dispatcher queue, standing for the whole binary: the shape an
/// application with a window runs. The thread that runs the tests is the one with the
/// queue, and it waits the way such a thread does -- in its message loop.
class dispatched_environment : public ::testing::Environment
{
public:
    void SetUp() override { sta_loop::start_dispatched("wxl dispatched tests: I/O"); }

    void TearDown() override {
        loop_stopping.set();
        sta_loop::stop();
    }
};

inline ::testing::Environment* const dispatched_env =
    ::testing::AddGlobalTestEnvironment(new dispatched_environment);

}  // namespace impl

/// Turns the message loop until the condition holds. What the queue is posted arrives
/// as a message, so every handover and every operation coming back wakes it.
inline void wait_until(auto done) {
    MSG message{};

    while (!done() && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
}

}  // namespace wxl::async
