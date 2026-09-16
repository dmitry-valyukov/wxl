wxl_add_module_library(wxl.logging STATIC
    PUBLIC
        wxl.logging.ixx

        severity.ixx
        entry.ixx
        facility.ixx
        output.ixx
        console_output.ixx
        file_output.ixx
        logger.ixx
        async_logger.ixx
    SRC
        entry.cpp
        output.cpp
        console_output.cpp
        file_output.cpp
        logger.cpp
        async_logger.cpp
)

add_library(wxl::logging ALIAS wxl.logging)

set_target_properties(wxl.logging PROPERTIES
    EXPORT_NAME logging
)

# All PUBLIC: the module interfaces name types from every one of these, and the
# calls that matter -- logger::info() and its siblings -- are templates compiled
# in the consumer's own translation units.
#
# The asynchronous half is what brings wxl.async in, and it brings it in for
# everyone importing wxl.logging: partitions are not separately importable. A
# consumer that wants the log without the component model would need a module
# of its own, which is written down in TODO.txt and not needed by anybody yet.
target_link_libraries(wxl.logging PUBLIC
    wxl::core
    wxl::fmt
    wxl::async
)

# Writing to an output can throw -- a full disk, a file that went away -- and
# multicast_output catches it to drop that output rather than the program. MSVC
# unwinds nothing without /EHsc, so the destructors between the throw and that
# catch would simply be skipped. Added only when the flags do not already carry
# it, the way wxl.xml does.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.logging PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.logging)
