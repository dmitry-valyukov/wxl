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

# Allocation is the one thing parse() may report by throwing, and MSVC
# unwinds nothing without /EHsc; added only when a toolchain file replaced
# the default flags wholesale, same as in wxl.core.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.html PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.html)
