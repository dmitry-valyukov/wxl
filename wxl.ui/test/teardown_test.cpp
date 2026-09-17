// wxl::Teardown -- what an application returns from wxl_launched, and the exit
// code it names.
//
// A plain executable rather than a gtest: Teardown lives in launch.h, which
// reaches wxl.core through core.h -- the include-then-import shim an ordinary
// translation unit uses. gtest's own <ostream>/<coroutine> and that import
// cannot share one TU (a standard header after `import std` is rejected), so
// this is built like a sample instead: the standard headers first, launch.h
// last, and the result reported through the exit code.
//
// Three facts: a handler that returns nothing names no code, a handler that
// returns a number names it (the unsigned one GetExitCodeProcess hands out
// included), and an empty Teardown is false.

#include <cstdio>
#include <utility>

#include "launch.h"

namespace {

int failures = 0;

void check(bool ok, char const* what) {
    if (!ok) {
        std::fprintf(stderr, "teardown_test: FAILED -- %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    {
        wxl::Teardown const empty;
        check(!empty, "an empty Teardown is false");
    }

    {
        int calls = 0;
        auto seen = wxl::TeardownReason::Closed;
        wxl::Teardown const teardown = [&](wxl::TeardownReason reason) {
            ++calls;
            seen = reason;
        };
        check(static_cast<bool>(teardown), "a set Teardown is true");
        check(!(*teardown)(wxl::TeardownReason::Error), "a handler returning nothing names no code");
        check(calls == 1, "the handler ran once");
        check(seen == wxl::TeardownReason::Error, "the handler saw its reason");
    }

    {
        wxl::Teardown const teardown = [](wxl::TeardownReason) { return 42; };
        check((*teardown)(wxl::TeardownReason::Closed) == 42, "a handler names the exit code");
    }

    {
        // The shape Trayed is in: GetExitCodeProcess hands back a DWORD.
        wxl::Teardown const teardown = [](wxl::TeardownReason) {
            return static_cast<unsigned long>(3);
        };
        check((*teardown)(wxl::TeardownReason::Closed) == 3, "an unsigned code is taken too");
    }

    {
        wxl::Teardown source = [](wxl::TeardownReason) { return 7; };
        wxl::Teardown target = std::move(source);
        check(static_cast<bool>(target), "the move target holds the handler");
        check(!source, "the moved-from source is empty");
        check((*target)(wxl::TeardownReason::Closed) == 7, "the moved handler still names its code");
        target = {};
        check(!target, "an assigned-away Teardown is empty");
    }

    if (failures == 0) std::puts("teardown_test: all checks passed");
    return failures;
}
