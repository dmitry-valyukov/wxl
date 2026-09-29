wxl_add_gtest(wxl.gen.common.tests
    SRC
        crawl_test.cpp
    DEPS
        wxl.gen.common
)

# The walk is tested on the generator's real profiles and the metadata they
# name, which is where the generator's output comes from.
target_compile_definitions(wxl.gen.common.tests
    PRIVATE
        WXL_GEN_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../wxl.gen/profiles"
)
