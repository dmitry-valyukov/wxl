wxl_add_module_library(wxl.fmt STATIC
    PUBLIC
        wxl.fmt.ixx

        text_buffer.ixx
        text_builder.ixx
)

add_library(wxl::fmt ALIAS wxl.fmt)

set_target_properties(wxl.fmt PROPERTIES
    EXPORT_NAME fmt
)

# fmt is looked for here and not only at the root of the tree: this directory
# is added on its own by consumers outside it -- Bukvitsa and Besedka add
# wxl.fmt/src straight into their builds -- and there nobody has looked for fmt
# yet. The same shape wxl.ui uses for cppwinrt.
if(NOT TARGET fmt::fmt)
    find_package(fmt CONFIG REQUIRED)
endif()

# PUBLIC: text_buffer is fmt::basic_memory_buffer and text_builder::format() is
# a template, so their bodies -- fmt::format_to and the appender -- are
# compiled in the consumer. Why fmt rather than std::format is in doc/fmt.md.
target_link_libraries(wxl.fmt PUBLIC fmt::fmt)

wxl_set_target_folders(wxl.fmt)
