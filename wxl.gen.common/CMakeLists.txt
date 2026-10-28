# wxl.gen.common -- what the generator (winui-srcgen) and the profile editor
# (wxl.gen.ui) share: the profiles, the metadata walk they describe and the
# documentation of what the walk finds.
#
# Headers, not a module. The walk hands back winmd's own types, and each
# consumer includes the reader directly, through crawl.h. Imported from
# another module instead, winmd's TypeDef stays incomplete for MSVC 14.51 and
# 14.52 (C2079 on EnumDefinition::m_typedef).
#
# The .cpp files are ordinary translation units that still import std and
# wxl's modules, hence the module-library helper: it scans them for imports.
wxl_add_module_library(wxl.gen.common STATIC
    HEADERS
        profile.h
        crawl.h
        documentation.h
        xml_input.h
    SRC
        profile.cpp
        crawl.cpp
        documentation.cpp

        xml_input.cpp
)

target_link_libraries(wxl.gen.common
    PUBLIC
        # crawl.h includes the reader, and platform.h ahead of it.
        winmd_reader
        # And wxl's own readers: the XAML dictionaries and the documentation
        # files are read with its XML reader; the profiles, the tables beside
        # them and the list of the restored packages with its JSON reader --
        # comments and all, which is the shape our settings files are written
        # in.
        wxl::core
)

wxl_set_target_folders(wxl.gen.common)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()

# Built whenever the tree is the project; in a project that adds wxl, only with
# the other benchmarks (WXL_BUILD_BENCHMARKS).
if(PROJECT_IS_TOP_LEVEL OR WXL_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
