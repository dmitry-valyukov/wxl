// The one thing the reader deliberately does not do: put the pieces of an
// element's text back together. A consumer writes this for itself, in the
// string type it needs and tuned to what it knows -- so a test, which is the
// consumer standing in for all of them, writes it once here instead of a loop
// at every assertion.
//
// Included after `import wxl.xml;` rather than before it, and holding no
// standard header of its own: MSVC does not take a standard header included
// after a translation unit has seen the std module.

#pragma once

/// The whole text under a node, in document order, with nothing between the
/// pieces.
inline std::string joined_text(const wxl::xml::node& el) {
    std::string out;

    for (const wxl::core::u8_view piece : el.text_pieces())
        out.append(piece.chars());

    return out;
}
