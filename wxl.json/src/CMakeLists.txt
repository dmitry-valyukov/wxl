wxl_add_module_library(wxl.json STATIC
    PUBLIC
        wxl.json.ixx

        exception.ixx
        value.ixx
        document.ixx
    SRC
        exception.cpp
        document.cpp
)

add_library(wxl::json ALIAS wxl.json)

# PUBLIC: the primary interface re-exports wxl.core, and a consumer compiles
# that re-export in its own translation units.
target_link_libraries(wxl.json PUBLIC wxl::core)

# The arena the tree is allocated from is the storage scheme of wxl.xml, taken
# as a dependency rather than copied. PUBLIC because document holds the arena
# in a field, and the module interface is visible to the consumer.
target_link_libraries(wxl.json PUBLIC wxl::xml)

# An invalid document is reported by an exception, and without /EHsc MSVC does
# not unwind the stack: destructors of the objects between throw and catch are
# not called. The flag is added only when a toolchain file has replaced the
# flags wholesale, as in wxl.xml.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.json PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.json)
