# The policies of this directory are those of CMake 3.28, and not the root's:
# the later ones change what its targets compile (CMP0204, for one, defines
# _MBCS in each target created under it).
cmake_minimum_required(VERSION 3.28)

configure_file(config.h.in config.h @ONLY)

wxl_add_gtest(wxl.xml.tests
    SRC
        byte_order_mark_test.cpp
        carriage_return_test.cpp
        character_references_test.cpp
        diagnostics_test.cpp
        doctype_test.cpp
        encoding_test.cpp
        file_name_test.cpp
        mixed_content_test.cpp
        namespaces_test.cpp
        ownership_test.cpp
        parser_test.cpp
        traverse_test.cpp
        well_formedness_test.cpp
        xml_declaration_test.cpp
        config.h.in
    DEPS
        wxl::core
)

# config.h is generated next to the build, not next to the sources.
target_include_directories(wxl.xml.tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
