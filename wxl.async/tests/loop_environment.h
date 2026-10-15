#pragma once

// The loop a suite runs on, for the suites built into two binaries: one on the sleeping
// loop, and one on a loop with a dispatcher queue under it, where what goes by a name
// is carried out on the thread pool.
#ifdef WXL_ASYNC_TESTS_DISPATCHED

#include "dispatched_environment.h"

#else

#include "sta_pool.h"

#endif

namespace wxl::async {

/// How either binary waits for a condition of the test's own: the loop's wait, in its
/// shape -- asleep in run_one(), or turning the message loop under the queue. A task
/// is waited for with run_until(task) itself.
inline void wait_until(auto done) { sta_loop::run_until(done); }

}  // namespace wxl::async
