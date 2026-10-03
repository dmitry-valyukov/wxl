#pragma once

// The loop a suite runs on, for the suites built into two binaries: one on the sleeping
// loop, and one on a loop with a dispatcher queue under it, where what goes by a name
// is carried out on the thread pool. wait_until() is how either of them waits.
#ifdef WXL_ASYNC_TESTS_DISPATCHED

#include "dispatched_environment.h"

#else

#include "sta_pool.h"

namespace wxl::async {

inline void wait_until(auto done) { sta_loop::run_until(done); }

}  // namespace wxl::async

#endif
