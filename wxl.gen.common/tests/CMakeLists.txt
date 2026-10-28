wxl_add_gtest(wxl.gen.common.tests
    SRC
        crawl_test.cpp
        documentation_test.cpp
        member_filter_test.cpp
        nuget_version_test.cpp
        profile_test.cpp
        xml_input_test.cpp
    DEPS
        wxl.gen.common
)

# The walk is tested on the generator's real profiles and the metadata they
# name, which is where the generator's output comes from: the packages and the
# Windows SDK of this build, as its configure lists them.
wxl_restored_packages_file(_wxl_gen_common_restored_packages)
target_compile_definitions(wxl.gen.common.tests
    PRIVATE
        WXL_GEN_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../wxl.gen/profiles"
        WXL_RESTORED_PACKAGES="${_wxl_gen_common_restored_packages}"
)
