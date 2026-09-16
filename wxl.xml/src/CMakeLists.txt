wxl_add_module_library(wxl.xml STATIC
    PUBLIC
        wxl.xml.ixx

        core.ixx
        exception.ixx
        encoding.ixx
        node.ixx
        document.ixx
        parser.ixx

        # Exported since wxl.html took it up for its own tree: the arena is
        # the storage scheme itself -- blocks from the STA pool, objects
        # carved by a cursor, the lot released whole -- and a second copy of
        # it next door would be the one thing worse than a wider interface.
        arena.ixx

        # Internal -- only the arena asks it for pages. Listed all the same:
        # a partition belongs to the module whether or not the primary
        # interface re-exports it.
        arena_pool.ixx
    SRC
        arena.cpp
        arena_pool.cpp
        encoding.cpp
        exception.cpp
        node.cpp
        document.cpp
        parser.cpp
)

add_library(wxl::xml ALIAS wxl.xml)

# PUBLIC because the primary interface re-exports wxl.core: a consumer that
# imports wxl.xml compiles that re-export in its own translation units.
target_link_libraries(wxl.xml PUBLIC wxl::core)

# A broken document is reported by throwing, and MSVC unwinds nothing without
# /EHsc: the destructors of everything on the stack between the throw and the
# catch are simply skipped. CMake puts the flag in CMAKE_CXX_FLAGS itself, but
# a toolchain file is free to replace those flags wholesale -- vcpkg's does
# when this directory is configured as its own project -- so it is added here
# only when it is missing, rather than duplicated on every source file.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.xml PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.xml)
