#pragma once

// What a string is at the surface of wxl.ui: `hstring` when a function gives
// one, `hstring_param` when it takes one, `zstring_view` when the text is
// only read. All three live in wxl.core -- an HSTRING is an ABI and needs no
// WinRT header -- and are named here so that the declarative syntax and every
// generated header spell them without a qualifier.
//
// A string is UTF-16 in char16_t, the unit the standard fixes at sixteen bits
// everywhere and the unit an HSTRING is made of; wchar_t is sixteen bits on
// MSVC and thirty-two on gcc and clang, so text written in it is UTF-16 by a
// coincidence of platform. Code moves from wchar_t to char16_t for that reason,
// and the ordinary way of writing a literal here is u"...". A wchar_t literal
// or string is still taken -- the seam is in zstring_view, and a crossing
// between the two is a renaming, not a conversion -- but nothing new is
// written in it.
//
// The contract hstring_param carries: the character after the text is a zero.
// A string handed to WinRT is handed over as a fast-pass string -- a header
// on the stack over these very characters, no copy -- and that is only legal
// with the terminator. The types that hold it (a literal, a std::basic_string,
// checked text that owns its buffer, an hstring) convert on their own; a plain
// view does not, because it promises nothing about the unit after its end, and
// goes through core::assume_terminated() where the promise is written out.
//
// What a property returns is an hstring, a reference to the very HSTRING the
// control holds: reading a Text is a count, not a copy of the text.

#include "core.h"

namespace wxl {

using core::hstring;
using core::hstring_param;
using core::zstring_view;

}  // namespace wxl
