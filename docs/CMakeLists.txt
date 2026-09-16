# ---- The wxl documentation site ----
#
#   cmake --build --preset x64-debug --target docs
#   build/x64/docs/wxl/html/index.html
#
# One site for the whole tree: the prose articles in docs/ and in each module's
# doc/ next to the API pulled out of the sources, because that is the pairing
# that makes either half worth reading. Doxygen links them by itself -- a
# mention of wxl::async::future in a markdown article becomes a link to its
# page, dozens of them in a single article -- and that link is what a
# separate site generator for the prose would cost.
#
# The site says nothing about Bukvitsa, deliberately: inside the code this is
# one job, but wxl is presented as a library that stands on its own. The
# traffic goes the other way, through the tag file Bukvitsa reads.

wxl_add_doxygen_site(docs
    PROJECT_NAME "wxl"
    BRIEF        "Winui Xaml-Less: декларативный интерфейс на C++23"
    LANGUAGE     Russian
    STRIP_FROM   "${PROJECT_SOURCE_DIR}"
    MAINPAGE     "${PROJECT_SOURCE_DIR}/docs/index.md"
    OUTPUT       "${CMAKE_BINARY_DIR}/docs/wxl"
    TAGFILE      "${CMAKE_BINARY_DIR}/docs/wxl.tag"
    INPUT
        "${PROJECT_SOURCE_DIR}/docs"
        "${PROJECT_SOURCE_DIR}/wxl.core/src"
        "${PROJECT_SOURCE_DIR}/wxl.core/readme.md"
        "${PROJECT_SOURCE_DIR}/wxl.fmt/src"
        "${PROJECT_SOURCE_DIR}/wxl.fmt/doc"
        "${PROJECT_SOURCE_DIR}/wxl.fmt/README.md"
        "${PROJECT_SOURCE_DIR}/wxl.async/src"
        "${PROJECT_SOURCE_DIR}/wxl.logging/src"
        "${PROJECT_SOURCE_DIR}/wxl.xml/src"
        # The other reader, and the arena's second consumer.
        "${PROJECT_SOURCE_DIR}/wxl.json/src"
        # The markup family: the HTML subset and the sibling
        # tokenizers over the shared tree builder.
        "${PROJECT_SOURCE_DIR}/wxl.html/src"
        "${PROJECT_SOURCE_DIR}/wxl.bb/src"
        "${PROJECT_SOURCE_DIR}/wxl.rsdn/src"
        "${PROJECT_SOURCE_DIR}/wxl.highlight/src"
        # The generated WinUI3 projection is part of the surface, not an
        # implementation detail: wxl::Button and wxl::UIElement are what a
        # consumer writes, and without them Bukvitsa's tag file resolves
        # nothing.
        "${PROJECT_SOURCE_DIR}/wxl.ui/src"
    EXCLUDE
        # CLAUDE.md files are instructions for working on the tree, not pages
        # about it; next-session-prompt.md is a working note of the same kind.
        "*/CLAUDE.md"
        "*/next-session-prompt.md"
        "*/tests/*"
        "*/test/*"
        "*/bench/*"
        "*/benchmarks/*"
        "*/packaging/*"
        "*/tools/*"
)

if(BUILD_TESTING)
    wxl_add_doxygen_test(docs)
endif()
