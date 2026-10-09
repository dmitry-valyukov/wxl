#pragma once

// A check of how a coroutine is used, for the global module fragment of a wxl.async
// interface unit: a macro does not travel through import. What the build does with it
// is asked of the partition wxl.async:coroutine_checks, which the unit imports as well.
//
// coro_check(cond, why, where): `why` is a string literal, `where` a coro_detail::site.
// Without STRICT_CORO it is assert: a Debug build stops there, naming the condition with
// the reason, and a Release build has nothing of it -- neither the condition nor the
// place is evaluated. With STRICT_CORO the condition is checked in every build, and a
// broken one ends the process through wxl::core::abort with the reason and `where`: the
// line of the code that broke the rule where the check is given it, else the check's own.
#include <cassert>

#define coro_check(cond, why, where)                                  \
    do {                                                              \
        if constexpr (::wxl::async::coro_detail::strict) {            \
            if (!(cond)) [[unlikely]]                                 \
                ::wxl::async::coro_detail::broken(why, where);        \
        } else {                                                      \
            assert((cond) && why);                                    \
        }                                                             \
    } while (false)
