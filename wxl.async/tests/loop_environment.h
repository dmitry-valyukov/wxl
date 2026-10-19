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
/// mode -- asleep in run_one(), or turning the message loop under the queue. A task
/// is waited for with run_until(task) itself.
inline void wait_until(auto done) { sta_loop::run_until(done); }

/// Opens the gate an orphan of the test stands at, and turns the loop until nothing is out,
/// on every way out of the test -- an ASSERT that returns early as well: the orphan holds the
/// test's locals by reference. Made after them, so that it goes first.
class orphans_drained_on_exit
{
public:
    /// For an orphan that waits for nothing the test holds shut.
    orphans_drained_on_exit() noexcept = default;

    explicit orphans_drained_on_exit(core::hevent& gate) noexcept : gate_(&gate) {}

    orphans_drained_on_exit(const orphans_drained_on_exit&) = delete;
    orphans_drained_on_exit& operator=(const orphans_drained_on_exit&) = delete;

    ~orphans_drained_on_exit() {
        if (gate_) gate_->set();

        wait_until([] { return sta_loop::outstanding() == 0; });
    }

private:
    core::hevent* gate_ = nullptr;
};

}  // namespace wxl::async
