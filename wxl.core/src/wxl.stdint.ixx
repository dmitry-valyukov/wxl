export module wxl.stdint;

import std;

/**
 * The C type names, in namespace `wxl`.
 *
 * `size_t`, `int32_t` and the rest are not names that `import std;` brings: from a module
 * they are `std::size_t`, `std::int32_t`, and the unqualified spellings arrive only with
 * `<stddef.h>` / `<stdint.h>`. Including those in a module interface for the sake of two
 * type names is a poor trade, and the alternative -- writing `std::` on every `size_t` --
 * is noise on names that have been spelled plainly in C and C++ for thirty years.
 *
 * So they are declared once here, and once is enough for the whole library: they sit in
 * `wxl`, the namespace enclosing `wxl::core` and `wxl::async`,
 * and unqualified lookup from any of those finds them without an import of its own beyond
 * the `wxl.core` everything imports anyway.
 *
 * A separate module rather than a partition of `wxl.core`, so that something needing only
 * the type names can take only them.
 */
export namespace wxl {

/* ---- <cstddef> ---- */
using size_t = std::size_t;
using ptrdiff_t = std::ptrdiff_t;
using nullptr_t = std::nullptr_t;
using max_align_t = std::max_align_t;

/// POSIX's signed counterpart of size_t. Not a standard C++ name at all -- MSVC spells it
/// SSIZE_T and only inside <basetsd.h>, which comes with windows.h -- so it is defined
/// here in terms of the one that means the same thing.
using ssize_t = std::ptrdiff_t;

/* ---- <cstdint>: exact width ---- */
using int8_t = std::int8_t;
using int16_t = std::int16_t;
using int32_t = std::int32_t;
using int64_t = std::int64_t;

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using uint64_t = std::uint64_t;

/* ---- <cstdint>: at least this wide ---- */
using int_least8_t = std::int_least8_t;
using int_least16_t = std::int_least16_t;
using int_least32_t = std::int_least32_t;
using int_least64_t = std::int_least64_t;

using uint_least8_t = std::uint_least8_t;
using uint_least16_t = std::uint_least16_t;
using uint_least32_t = std::uint_least32_t;
using uint_least64_t = std::uint_least64_t;

/* ---- <cstdint>: fastest of at least this width ---- */
using int_fast8_t = std::int_fast8_t;
using int_fast16_t = std::int_fast16_t;
using int_fast32_t = std::int_fast32_t;
using int_fast64_t = std::int_fast64_t;

using uint_fast8_t = std::uint_fast8_t;
using uint_fast16_t = std::uint_fast16_t;
using uint_fast32_t = std::uint_fast32_t;
using uint_fast64_t = std::uint_fast64_t;

/* ---- <cstdint>: pointer-sized and widest ---- */
using intptr_t = std::intptr_t;
using uintptr_t = std::uintptr_t;
using intmax_t = std::intmax_t;
using uintmax_t = std::uintmax_t;

}  // export namespace wxl
