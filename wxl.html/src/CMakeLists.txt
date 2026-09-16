wxl_add_module_library(wxl.html STATIC
    PUBLIC
        wxl.html.ixx

        node.ixx
        builder.ixx
        parse.ixx
    SRC
        builder.cpp
        parse.cpp
)

add_library(wxl::html ALIAS wxl.html)

# PUBLIC because the interface re-exports wxl.core (the intrusive list the
# children hang on) and answers parse() with a tree of it.
target_link_libraries(wxl.html PUBLIC wxl::core)

# The arena the tree is carved from -- wxl.xml's own storage scheme, taken
# as a dependency rather than copied. PUBLIC since the :builder partition
# became the family door: tree_builder hands sibling tokenizers (wxl.bb,
# wxl.rsdn) the arena copies and the sta scratch types by name.
target_link_libraries(wxl.html PUBLIC wxl::xml)

# Allocation is the one thing parse() may report by throwing, and MSVC
# unwinds nothing without /EHsc; added only when a toolchain file replaced
# the default flags wholesale, same as in wxl.xml.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.html PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.html)
