// wxl::html -- the HTML subset behind HtmlBlock.
//
// A tolerant parser for foreign, often broken markup -- forum posts, chat
// messages -- producing a lightweight DOM for one walk. The contract, in
// full, is wxl.html/design.md; the two rules everything here serves:
//
//  - the text is sacred, the tags are not: no input ever throws, an unknown
//    tag disappears and leaves its content standing, a mismatched close
//    closes and reopens the way browsers do;
//  - every parse is complete in itself: the tree closes all its blocks, and
//    neither the source nor any parser state survives the call -- content
//    may arrive in independent chunks.
//
// A wxl::core::sta_memory_pool has to exist before the first parse and
// outlive the last document: the tree is carved from an arena over that
// pool, the storage scheme wxl.xml proved out.

export module wxl.html;

export import :node;
export import :builder;
export import :parse;
