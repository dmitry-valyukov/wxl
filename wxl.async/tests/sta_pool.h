#pragma once

#include <gtest/gtest.h>

import wxl.core;
import wxl.async;

namespace wxl::async {

namespace impl {

/// The loop, standing for the whole binary.
///
/// The loop has to be here rather than in each fixture, and not for tidiness:
/// it starts once per process and cannot be restarted. Its channels live as long
/// as the process, `spsc_queue` takes a single reader for the whole of its life,
/// and the turnstile that guards sending latches shut for good -- so a test that
/// stopped the loop after itself would leave every later test without one.
///
/// The sleeping shape, because a test has no message loop of its own: what the
/// tests wait in is `sta_loop::run_until`. The shape a real application uses --
/// a callback into its dispatcher -- is `start_driven()`, and it is `sta_signal`
/// that carries the difference.
class sta_environment : public ::testing::Environment
{
public:
    void SetUp() override {
        sta_loop::start("wxl tests: I/O");
    }

    void TearDown() override { sta_loop::stop(); }
};

/// Registered before main(), where a global environment is registered.
inline ::testing::Environment* const sta_env =
    ::testing::AddGlobalTestEnvironment(new sta_environment);

}  // namespace impl

}  // namespace wxl::async
