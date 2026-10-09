export module wxl.core:checks;

import std;

export namespace wxl::core {

/**
 * Reports a failed `ensure()` check the way `abort()` reports its reason:
 * `cond_str` and its source location. Called by the release-build expansion
 * of `ensure()`; not intended to be called directly.
 */
[[noreturn]] void fail(std::string_view cond_str,
                       std::source_location loc = std::source_location::current());

/**
 * Ends the process over a broken rule there is no way back from. The reason
 * goes, as `file(line): why`, to stderr and to the debugger's output; with a
 * debugger attached the process stops right there, as at a failed assert, and
 * otherwise it aborts. The place is the caller's own line unless one is given:
 * a check that knows the line of the code that broke the rule passes that one.
 * Nothing is allocated on the way out, so it serves a pool that has run dry.
 */
[[noreturn]] void abort(std::string_view why,
                        std::source_location where = std::source_location::current()) noexcept;

}  // export namespace wxl::core
