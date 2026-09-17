module;

// fmt first, and it has to stay first: wxl.core's abi.h defines assume() as a
// macro, and fmt has a function of that name -- included the other way round,
// the macro rewrites fmt's own definition of it and the header stops parsing.
#include <fmt/format.h>

// The buffer text is assembled in.
//
// It is fmt's, not the standard library's, and not one of our own either. The
// standard library was ruled out on the way it is built: std::format has no
// public way to be handed a buffer -- format_to() to a back_inserter breaks
// the output into a push_back per character, format_to_n() to a raw pointer
// formats into a stack buffer and copies out of it, and formatted_size() is a
// second full pass through every formatter. fmt's buffer is the extension
// point the standard left out.
//
// A buffer of our own was ruled out by measurement. The pool underneath wxl
// keeps a bump cursor, so a block that is the last one handed out could grow
// where it stands, and a buffer built on that would never copy itself. It
// turns out not to matter: a
// pool that has been used has free lists, a block from a free list has a
// neighbour behind it, and only the first buffer in a program's life is on
// top of the bump. The run that measures it is
// wxl.fmt/benchmarks/text_builder_benchmark.cpp.
//
// Where the text goes is one question. How the format string is read is
// another, independent of it: FMT_COMPILE parses the string at compile time
// and adds to whatever buffer is underneath. That one is the caller's, made
// one format string at a time -- see :text_builder.

export module wxl.fmt:text_buffer;

import std;

export namespace wxl::core {

/// How much room a buffer carries inside itself, before it allocates
/// anything at all. A line of text with a number or two in it fits, which is
/// what most of them are.
inline constexpr std::size_t buffered_capacity = 256;

/// The buffer text_builder assembles into, and what a caller reaches for to
/// format into a buffer of its own.
///
/// The allocator is named as a template, not as a type made for char: the
/// buffer is always char, so the element type would only ever be repeated.
/// text_buffer<wxl::core::sta_allocator> is a buffer on the STA pool, which
/// is worth having wherever the pool is already the program's answer -- not
/// for growing, which measurement says is a wash, but because that is where
/// the memory of that thread lives; on the GUI thread it is the one to write.
/// The default is the ordinary heap, for the threads that own no pool --
/// wxl.io's and wxl.logging's.
template <template <typename> class Allocator = std::allocator>
using text_buffer = fmt::basic_memory_buffer<char, buffered_capacity, Allocator<char>>;

/// What has been written into a buffer so far, as a view into it: valid until
/// the next append and no longer. There is no terminating zero -- nothing
/// writes one -- so a caller handing this to a C API appends one itself.
template <typename Allocator>
constexpr std::string_view view_of(
    const fmt::basic_memory_buffer<char, buffered_capacity, Allocator>& buffer) noexcept {
    return {buffer.data(), buffer.size()};
}

}  // export namespace wxl::core
