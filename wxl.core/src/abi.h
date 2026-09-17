#pragma once

// The only header a module interface partition needs in its global module
// fragment for the language-level plumbing that cannot travel through
// `import`: preprocessor macros (assert/assume/ensure) and the C fixed-width
// and size types the code spells unqualified.
#include <cassert>
#include <stddef.h>
#include <stdint.h>

#if (defined(__SIZE_WIDTH__) && __SIZE_WIDTH__ == 64) ||                          \
    (defined(__INTPTR_WIDTH__) && __INTPTR_WIDTH__ == 64) || defined(_M_AMD64) || \
    defined(_M_ARM64)
#define PLATFORM_64BIT
#else
#define PLATFORM_32BIT
#endif

// POSIX signed counterpart of size_t; MSVC only provides the uppercase SSIZE_T
// (via <basetsd.h>, pulled in by windows.h). Spelled via ptrdiff_t here so that
// partitions which never touch the Windows API don't have to include it.
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
using ssize_t = ptrdiff_t;
#endif

/**
 * Platform-specific implementation of the optimizer hint used by `assume()`
 * in release builds. Telling the compiler that `cond` always holds lets it
 * prune the branches that would otherwise handle the false case.
 */
// The C++23 spelling `[[assume(cond)]]` would replace all four branches at
// once, and MSVC 19.51 does answer `__has_cpp_attribute(assume)` and compile
// it without a word -- but its optimizer ignores it. Measured: after
// `[[assume(x == 42)]]` the compiler still returns x, while after
// `__assume(x == 42)` it returns the constant. Switching would silently turn
// every release-build assume() into a no-op, so the intrinsic stays.
#if defined(__clang__)
#define WXL_ASSUME_IMPL(cond) __builtin_assume(cond)
#elif defined(_MSC_VER)
#define WXL_ASSUME_IMPL(cond) __assume(cond)
#elif defined(__GNUC__)
#define WXL_ASSUME_IMPL(cond)                 \
    do {                                      \
        if (!(cond)) __builtin_unreachable(); \
    } while (0)
#else
#define WXL_ASSUME_IMPL(cond) ((void)0)
#endif

/**
 * Declares an invariant that is checked in debug builds (as `assert`) and
 * assumed true by the optimizer in release builds. Violating `cond` in a
 * release build is undefined behavior, so only use it for conditions that
 * are truly guaranteed by the surrounding code, never for validating
 * external input.
 */
#ifdef NDEBUG
#define assume(cond) WXL_ASSUME_IMPL(cond)
#else
#define assume(cond) assert(cond)
#endif

/**
 * Checks a precondition that must hold in both debug and release builds. In
 * debug builds this expands to `assert(cond)`; in release builds a failure
 * reports `cond` and its call site via `wxl::core::fail` and aborts, instead
 * of being assumed true the way `assume()` is. Prefer `ensure()` over
 * `assume()` for conditions that can be violated by bad input rather than
 * only by an internal bug.
 *
 * \note The release-build expansion names `wxl::core::fail`, so a translation
 *       unit using `ensure()` must also `import :checks;`.
 */
#ifdef NDEBUG
#define ensure(cond)                \
    do {                            \
        if (!(cond)) [[unlikely]]   \
            wxl::core::fail(#cond); \
    } while (0)
#else
#define ensure(cond) assert(cond)
#endif
