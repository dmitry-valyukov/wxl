add_executable(wxl.core.tests
    basic_text_tests.cpp
    bit_vector_tests.cpp
    compressed_optional_tests.cpp
    directory_tests.cpp
    event_tests.cpp
    file_tests.cpp
    function_tests.cpp
    grapheme_tests.cpp
    intrusive_list_tests.cpp
    intrusive_slist_tests.cpp
    module_cleanup_tests.cpp
    mutex_tests.cpp
    not_null_tests.cpp
    numbers_tests.cpp
    observable_tests.cpp
    path_tests.cpp
    refcounted_tests.cpp
    spin_lock_tests.cpp
    sta_allocator_tests.cpp
    sta_stl_tests.cpp
    strings_tests.cpp
    sync_root_tests.cpp
    thread_heap_tests.cpp
    time_tests.cpp
    timeout_timer_tests.cpp
    unicode_tests.cpp
)

target_link_libraries(wxl.core.tests
    PRIVATE
        wxl::core
        wxl::test-support
)

# GraphemeBreakTest.txt of the Unicode version the grapheme table was
# generated from; gen-grapheme-table.py writes both.
target_compile_definitions(wxl.core.tests
    PRIVATE
        WXL_CORE_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

wxl_set_target_folders(wxl.core.tests)

include(GoogleTest)

gtest_discover_tests(
    wxl.core.tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
