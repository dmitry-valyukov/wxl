wxl_add_module_library(wxl.fmt STATIC
    PUBLIC
        wxl.fmt.ixx

        text_buffer.ixx
        text_builder.ixx
        format.ixx
    SRC
        format.cpp
)

add_library(wxl::fmt ALIAS wxl.fmt)

set_target_properties(wxl.fmt PROPERTIES
    EXPORT_NAME fmt
)

# fmt is taken as a C++20 module, `import fmt;`, built here from its own
# sources rather than from the vcpkg port, which builds the library alone. A
# module is the one shape that sits beside `import std;` without trouble: fmt
# headers in a global module fragment merge with the std module only as far
# as MSVC's extern "C++" workaround reaches, and fmt/std.h (thread,
# filesystem) is past it. With FMT_IMPORT_STD fmt imports std itself and its
# global module fragment holds C headers alone.
#
# Fetched here and not only at the root of the tree: this directory is added
# on its own by consumers outside it -- Bukvitsa and Besedka add wxl.fmt/src
# straight into their builds. SOURCE_SUBDIR names a directory fmt does not
# have, so its own CMakeLists.txt stays out of the build: fmt makes its
# module target for the plain Ninja generator only, with flags of its own.
if(NOT TARGET fmt::module)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 1be298e1bd68957e4cd352e1f676f00e07dcfb57 # 12.2.0
        SOURCE_SUBDIR sources-only
    )
    FetchContent_MakeAvailable(fmt)

    add_library(fmt.module STATIC)
    add_library(fmt::module ALIAS fmt.module)

    # fmt.cc includes format.cc after the headers when it finds it beside
    # itself, and that puts the definitions of dragonbox and the locale
    # helpers into the module. An importer then instantiates them from the
    # module instead of taking the instances the module holds, and MSVC 14.51
    # cannot: it loses the template parameter of a definition read back from
    # the module ("'T': undeclared identifier" in format-inl.h). So the module
    # unit is compiled from a copy with no format.cc beside it, and format.cc
    # is an ordinary translation unit of the same library. For the two to
    # link, the module attaches its declarations to the global module
    # (FMT_ATTACH_TO_GLOBAL_MODULE: extern "C++" around the headers), which is
    # what a plain translation unit defines. FMT_OS=0: the file and pipe
    # wrappers of fmt/os.h have no caller here.
    set(_fmt_module_dir "${CMAKE_CURRENT_BINARY_DIR}/fmt-module")
    configure_file("${fmt_SOURCE_DIR}/src/fmt.cc" "${_fmt_module_dir}/fmt.cc" COPYONLY)
    target_sources(fmt.module
        PUBLIC
            FILE_SET fmt TYPE CXX_MODULES
            BASE_DIRS "${_fmt_module_dir}"
            FILES "${_fmt_module_dir}/fmt.cc"
        PRIVATE
            "${fmt_SOURCE_DIR}/src/format.cc"
    )
    set_source_files_properties("${_fmt_module_dir}/fmt.cc" PROPERTIES
        COMPILE_DEFINITIONS "FMT_IMPORT_STD;FMT_ATTACH_TO_GLOBAL_MODULE"
    )
    target_include_directories(fmt.module PRIVATE ${fmt_SOURCE_DIR}/include)
    target_compile_definitions(fmt.module PRIVATE FMT_OS=0)
    target_compile_features(fmt.module PUBLIC cxx_std_23)
    set_target_properties(fmt.module PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )
    wxl_set_target_folders(fmt.module)
endif()

# PUBLIC: text_buffer is fmt::basic_memory_buffer and text_builder::format() is
# a template, so their bodies are compiled in the consumer, which needs the
# module for that. Why fmt rather than std::format is in doc/fmt.md.
target_link_libraries(wxl.fmt PUBLIC fmt::module)

# format() answers with u16_text, so the interface names a type from wxl.core.
target_link_libraries(wxl.fmt PUBLIC wxl::core)

wxl_set_target_folders(wxl.fmt)
