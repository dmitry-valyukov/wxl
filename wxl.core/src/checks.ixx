export module wxl.core:checks;

import std;

export namespace wxl::core {

/**
 * Reports a failed `ensure()` check: prints `cond_str` together with its
 * source location to stderr and aborts. Called by the release-build
 * expansion of `ensure()`; not intended to be called directly.
 */
[[noreturn]] void fail(std::string_view cond_str,
                       std::source_location loc = std::source_location::current());

[[noreturn]] void abort(std::string_view err) noexcept;

}  // export namespace wxl::core
