export module wxl.async:coroutine_checks;

import wxl.core;
import std;

/// What a build does about the checks of how coroutines are used -- a task awaited by
/// two at once, a value read twice, a task dropped while it runs or while somebody
/// joins it, a task used after a move. Code that keeps state only the checks look
/// at asks here; the check itself is `coro_check` in coroutine_checks.h, a macro, since
/// without STRICT_CORO it is assert.
export namespace wxl::async::coro_detail {

/// Built with STRICT_CORO (CMake: WXL_STRICT_CORO): the checks stay in every build,
/// Release included, and a broken one ends the process through core::abort, naming the
/// reason and the place.
inline constexpr bool strict =
#ifdef STRICT_CORO
    true;
#else
    false;
#endif

/// Whether this build checks at all: a strict one, and any Debug one, where the check
/// is assert. Without it a broken rule is undefined behaviour, and a build keeps
/// neither a byte nor an instruction for it.
inline constexpr bool checked =
#if defined(STRICT_CORO) || !defined(NDEBUG)
    true;
#else
    false;
#endif

/// The place of a build that reports none. constexpr rather than consteval: it is the
/// default argument of hooks the compiler calls in every build that checks, and an
/// immediate function in that place is one compilers have rejected (g++ 13, for
/// initial_suspend).
struct no_site {
    static constexpr no_site current() noexcept { return {}; }
};

/// The place of the code that broke a rule, as a defaulted parameter gives it: the line
/// of the call -- or of the co_await, when the compiler calls the awaiter. Only a strict
/// build reports it; any other passes nothing.
using site = std::conditional_t<strict, std::source_location, no_site>;

/// Where a strict build's broken check goes.
[[noreturn]] inline void broken(std::string_view why, std::source_location where) noexcept {
    core::abort(why, where);
}

/// Never called: a build without a place is not strict, and its check is assert.
[[noreturn]] inline void broken(std::string_view why, no_site) noexcept {
    core::abort(why);
}

}  // export namespace wxl::async::coro_detail
