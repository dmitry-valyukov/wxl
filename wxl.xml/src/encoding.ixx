
// The one thing the reader has to say about UTF-8 that wxl.core does not: a
// broken document is an exception with an offset in it, not an answer to be
// looked at.
//
// Everything else about the encoding -- checking it, writing a code point,
// turning a path into UTF-8, knowing what a surrogate is -- belongs to
// wxl::core and is used from there.

export module wxl.xml:encoding;

import :core;

export namespace wxl::xml {

/// Checks that the text is well-formed UTF-8. Run once over the whole document
/// before it is parsed: the grammar walks bytes and would read a broken
/// sequence as content without noticing.
///
/// @throw exception, naming the offset the bad sequence starts at.
void validate_utf8(std::string_view text);

}  // namespace wxl::xml
