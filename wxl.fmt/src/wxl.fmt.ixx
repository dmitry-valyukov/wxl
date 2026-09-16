// wxl.fmt -- the buffer text is assembled in, and fmt pointed at it.
//
// Its own module and not a partition of wxl.core for one reason: fmt. The
// buffer is fmt::basic_memory_buffer and the builder's format() is a template
// compiled in the consumer, so whoever imports this compiles against fmt's
// headers. wxl.core stays on the std module alone, and only the modules that
// assemble text -- the log, the applications writing their settings -- take
// the dependency.
//
// The names live in wxl::core all the same: text_builder and text_buffer are
// spelled wxl::core::text_builder<sta_allocator> wherever they are used, and
// moving the type between modules was not a reason to rename every call. The
// module is what one imports; the namespace is what one writes.

export module wxl.fmt;

export import :text_buffer;
export import :text_builder;
