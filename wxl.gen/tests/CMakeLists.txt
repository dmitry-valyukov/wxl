wxl_add_gtest(wxl.gen.tests
    SRC
        doc_language_test.cpp
        documentation_ids_test.cpp
        emit_test.cpp
        generate_test.cpp
        group_namespaces_test.cpp
        map_type_test.cpp
        parts_test.cpp
    DEPS
        wxl.gen
        # test_directory() for the tests that write the generator's output.
        wxl::test.gtest
)

# The type mapping is tested on the generator's real profiles and the metadata
# they name, which is where the generator's output comes from: the packages and
# the Windows SDK of this build, as its configure lists them.
wxl_restored_packages_file(_wxl_gen_restored_packages)
target_compile_definitions(wxl.gen.tests
    PRIVATE
        WXL_GEN_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../profiles"
        WXL_RESTORED_PACKAGES="${_wxl_gen_restored_packages}"
)

# The arguments are parsed in winui-srcgen itself, not in the library, so the
# refusal of a language the reference has no folder for is tested on the
# executable: one test for the message, one for the failing exit code --
# PASS_REGULAR_EXPRESSION alone ignores the code, and WILL_FAIL would invert
# the match. The refusal comes before the profile is read.
set(_wxl_gen_doc_language
    winui-srcgen --profile "${CMAKE_CURRENT_SOURCE_DIR}/../profiles/base.winxl-profile"
    --packages "${_wxl_gen_restored_packages}" --docs --doc-language)
add_test(NAME winui-srcgen.unknown-doc-language.message COMMAND ${_wxl_gen_doc_language} pt)
set_tests_properties(winui-srcgen.unknown-doc-language.message PROPERTIES
    PASS_REGULAR_EXPRESSION "--doc-language pt is not a language of the reference")
add_test(NAME winui-srcgen.unknown-doc-language.fails COMMAND ${_wxl_gen_doc_language} pt)
set_tests_properties(winui-srcgen.unknown-doc-language.fails PROPERTIES WILL_FAIL TRUE)

# An empty code is refused by the same message, which leaves two spaces where
# the code stands. It is a quoted argument after the list: an empty element
# inside a list is dropped when the list is expanded.
add_test(NAME winui-srcgen.empty-doc-language.message COMMAND ${_wxl_gen_doc_language} "")
set_tests_properties(winui-srcgen.empty-doc-language.message PROPERTIES
    PASS_REGULAR_EXPRESSION "--doc-language  is not a language of the reference")
add_test(NAME winui-srcgen.empty-doc-language.fails COMMAND ${_wxl_gen_doc_language} "")
set_tests_properties(winui-srcgen.empty-doc-language.fails PROPERTIES WILL_FAIL TRUE)

# A run without the list of the restored packages stops before it reads a
# profile, and says where the list comes from.
set(_wxl_gen_no_packages
    winui-srcgen --profile "${CMAKE_CURRENT_SOURCE_DIR}/../profiles/base.winxl-profile")
add_test(NAME winui-srcgen.no-packages.message COMMAND ${_wxl_gen_no_packages})
set_tests_properties(winui-srcgen.no-packages.message PROPERTIES
    PASS_REGULAR_EXPRESSION "--packages <build>/wxl.packages/restored-packages.json")
add_test(NAME winui-srcgen.no-packages.fails COMMAND ${_wxl_gen_no_packages})
set_tests_properties(winui-srcgen.no-packages.fails PROPERTIES WILL_FAIL TRUE)

# A number of parts the names cannot carry -- they have two digits -- or no number
# at all is refused before the profile is read, as the language is.
set(_wxl_gen_parts
    winui-srcgen --profile "${CMAKE_CURRENT_SOURCE_DIR}/../profiles/base.winxl-profile"
    --packages "${_wxl_gen_restored_packages}" --out "${CMAKE_CURRENT_BINARY_DIR}/parts-refused"
    --parts)
foreach(_wxl_gen_count IN ITEMS 0 101 eight)
    add_test(NAME winui-srcgen.parts-${_wxl_gen_count}.message
        COMMAND ${_wxl_gen_parts} ${_wxl_gen_count})
    set_tests_properties(winui-srcgen.parts-${_wxl_gen_count}.message PROPERTIES
        PASS_REGULAR_EXPRESSION "--parts ${_wxl_gen_count}: the number of parts is from 1 to 100")
    add_test(NAME winui-srcgen.parts-${_wxl_gen_count}.fails
        COMMAND ${_wxl_gen_parts} ${_wxl_gen_count})
    set_tests_properties(winui-srcgen.parts-${_wxl_gen_count}.fails PROPERTIES WILL_FAIL TRUE)
endforeach()
