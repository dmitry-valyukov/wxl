wxl_add_gtest(wxl.gen.tests
    SRC
        emit_test.cpp
        group_namespaces_test.cpp
        map_type_test.cpp
    DEPS
        wxl.gen
)

# The type mapping is tested on the generator's real profiles and the metadata
# they name, which is where the generator's output comes from.
target_compile_definitions(wxl.gen.tests
    PRIVATE
        WXL_GEN_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../profiles"
)
