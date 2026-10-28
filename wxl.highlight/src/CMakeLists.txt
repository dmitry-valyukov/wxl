wxl_add_module_library(wxl.highlight STATIC
    PUBLIC
        wxl.highlight.ixx
    SRC
        highlight.cpp
        languages.cpp
)

add_library(wxl::highlight ALIAS wxl.highlight)

# Nothing here throws, but the standard library module is built with unwinding,
# and one set of options for the whole tree is cheaper, as in the other modules.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.highlight PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.highlight)
