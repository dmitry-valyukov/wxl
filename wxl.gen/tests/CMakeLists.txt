wxl_add_gtest(wxl.gen.tests
    SRC
        doc_language_test.cpp
        documentation_ids_test.cpp
        emit_test.cpp
        group_namespaces_test.cpp
        map_type_test.cpp
    DEPS
        wxl.gen
        # test_directory() for the tests that write the generator's output.
        wxl::test-support
)

# The type mapping is tested on the generator's real profiles and the metadata
# they name, which is where the generator's output comes from.
target_compile_definitions(wxl.gen.tests
    PRIVATE
        WXL_GEN_PROFILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../profiles"
)

# The arguments are parsed in winui-srcgen itself, not in the library, so the
# refusal of a language the reference has no folder for is tested on the
# executable: one test for the message, one for the failing exit code --
# PASS_REGULAR_EXPRESSION alone ignores the code, and WILL_FAIL would invert
# the match. The refusal comes before the profile is read.
set(_wxl_gen_doc_language
    winui-srcgen --profile "${CMAKE_CURRENT_SOURCE_DIR}/../profiles/base.json"
    --docs --doc-language)
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
