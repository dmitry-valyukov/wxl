# wxl.gen.common -- what the generator (winui-srcgen) and the profile editor
# (wxl.gen.ui) share: the profiles and the metadata walk they describe.
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
        xml_input.h
    SRC
        profile.cpp
        crawl.cpp

        xml_input.cpp
)

target_link_libraries(wxl.gen.common
    PUBLIC
        # crawl.h includes the reader.
        winmd_reader
    PRIVATE
        # The profiles are read with wxl's own JSON reader -- comments and
        # all, which is the shape our settings files are written in.
        wxl::json
        # The nuspec of a WindowsAppSDK release and the XAML dictionaries are
        # read with wxl's own XML reader.
        wxl::xml
        wxl::core
)

wxl_set_target_folders(wxl.gen.common)
