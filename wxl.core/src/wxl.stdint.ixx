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
/// The unsigned integer type of the result of `sizeof`; alias of `std::size_t`.
using size_t = std::size_t;
/// The signed integer type of the result of subtracting two pointers; alias of
/// `std::ptrdiff_t`.
using ptrdiff_t = std::ptrdiff_t;
/// The type of the `nullptr` literal; alias of `std::nullptr_t`.
using nullptr_t = std::nullptr_t;
/// The type whose alignment is at least as strict as that of every scalar type; alias of
/// `std::max_align_t`.
using max_align_t = std::max_align_t;

/// The signed counterpart of `size_t`, as POSIX defines it; alias of `std::ptrdiff_t`.
/// Standard C++ does not define this name, and MSVC provides it only as `SSIZE_T` in
/// `<basetsd.h>`, which `windows.h` includes.
using ssize_t = std::ptrdiff_t;

/* ---- <cstdint>: exact width ---- */
/// The signed integer type of exactly 8 bits; alias of `std::int8_t`.
using int8_t = std::int8_t;
/// The signed integer type of exactly 16 bits; alias of `std::int16_t`.
using int16_t = std::int16_t;
/// The signed integer type of exactly 32 bits; alias of `std::int32_t`.
using int32_t = std::int32_t;
/// The signed integer type of exactly 64 bits; alias of `std::int64_t`.
using int64_t = std::int64_t;

/// The unsigned integer type of exactly 8 bits; alias of `std::uint8_t`.
using uint8_t = std::uint8_t;
/// The unsigned integer type of exactly 16 bits; alias of `std::uint16_t`.
using uint16_t = std::uint16_t;
/// The unsigned integer type of exactly 32 bits; alias of `std::uint32_t`.
using uint32_t = std::uint32_t;
/// The unsigned integer type of exactly 64 bits; alias of `std::uint64_t`.
using uint64_t = std::uint64_t;

/* ---- <cstdint>: at least this wide ---- */
/// The smallest signed integer type of at least 8 bits; alias of `std::int_least8_t`.
using int_least8_t = std::int_least8_t;
/// The smallest signed integer type of at least 16 bits; alias of `std::int_least16_t`.
using int_least16_t = std::int_least16_t;
/// The smallest signed integer type of at least 32 bits; alias of `std::int_least32_t`.
using int_least32_t = std::int_least32_t;
/// The smallest signed integer type of at least 64 bits; alias of `std::int_least64_t`.
using int_least64_t = std::int_least64_t;

/// The smallest unsigned integer type of at least 8 bits; alias of `std::uint_least8_t`.
using uint_least8_t = std::uint_least8_t;
/// The smallest unsigned integer type of at least 16 bits; alias of `std::uint_least16_t`.
using uint_least16_t = std::uint_least16_t;
/// The smallest unsigned integer type of at least 32 bits; alias of `std::uint_least32_t`.
using uint_least32_t = std::uint_least32_t;
/// The smallest unsigned integer type of at least 64 bits; alias of `std::uint_least64_t`.
using uint_least64_t = std::uint_least64_t;

/* ---- <cstdint>: fastest of at least this width ---- */
/// The fastest signed integer type of at least 8 bits; alias of `std::int_fast8_t`.
using int_fast8_t = std::int_fast8_t;
/// The fastest signed integer type of at least 16 bits; alias of `std::int_fast16_t`.
using int_fast16_t = std::int_fast16_t;
/// The fastest signed integer type of at least 32 bits; alias of `std::int_fast32_t`.
using int_fast32_t = std::int_fast32_t;
/// The fastest signed integer type of at least 64 bits; alias of `std::int_fast64_t`.
using int_fast64_t = std::int_fast64_t;

/// The fastest unsigned integer type of at least 8 bits; alias of `std::uint_fast8_t`.
using uint_fast8_t = std::uint_fast8_t;
/// The fastest unsigned integer type of at least 16 bits; alias of `std::uint_fast16_t`.
using uint_fast16_t = std::uint_fast16_t;
/// The fastest unsigned integer type of at least 32 bits; alias of `std::uint_fast32_t`.
using uint_fast32_t = std::uint_fast32_t;
/// The fastest unsigned integer type of at least 64 bits; alias of `std::uint_fast64_t`.
using uint_fast64_t = std::uint_fast64_t;

/* ---- <cstdint>: pointer-sized and widest ---- */
/// The signed integer type to which a pointer to `void` converts and back without loss;
/// alias of `std::intptr_t`.
using intptr_t = std::intptr_t;
/// The unsigned integer type to which a pointer to `void` converts and back without loss;
/// alias of `std::uintptr_t`.
using uintptr_t = std::uintptr_t;
/// The widest signed integer type; alias of `std::intmax_t`.
using intmax_t = std::intmax_t;
/// The widest unsigned integer type; alias of `std::uintmax_t`.
using uintmax_t = std::uintmax_t;

}  // export namespace wxl
