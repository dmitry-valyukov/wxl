add_executable(wxl.core.tests
    basic_text_tests.cpp
    binding_scope_tests.cpp
    bit_vector_tests.cpp
    change_watch_tests.cpp
    checks_tests.cpp
    compressed_optional_tests.cpp
    directory_tests.cpp
    environment_tests.cpp
    event_tests.cpp
    file_tests.cpp
    function_tests.cpp
    grapheme_tests.cpp
    hstring_tests.cpp
    intrusive_list_tests.cpp
    intrusive_slist_tests.cpp
    module_cleanup_tests.cpp
    mutex_tests.cpp
    not_null_tests.cpp
    numbers_tests.cpp
    observable_list_tests.cpp
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
    weak_link_tests.cpp
    zstring_view_tests.cpp
)

target_link_libraries(wxl.core.tests
    PRIVATE
        GTest::gtest_main
        wxl::core
        wxl::test.gtest
        wxl::test
        runtimeobject
)

# GraphemeBreakTest.txt of the Unicode version the grapheme table was
# generated from; gen-grapheme-table.py writes both.
target_compile_definitions(wxl.core.tests
    PRIVATE
        WXL_CORE_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

wxl_set_target_folders(wxl.core.tests)

include(GoogleTest)

# Listed when ctest starts, from the binary of the configuration it tests, as
# wxl_add_gtest does: a listing taken after the link is shared by every
# configuration of a multi-config tree (CMake before 4.4), and the Debug and
# Release programs differ here: the probes that read the debug CRT heap are
# built in Debug only.
gtest_discover_tests(
    wxl.core.tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DISCOVERY_MODE PRE_TEST
)

# The tests of the areas of the module, one directory each.
add_subdirectory(fmt)
add_subdirectory(async)
add_subdirectory(logging)
add_subdirectory(xml)
add_subdirectory(json)
