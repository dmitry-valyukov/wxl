# winui-srcgen -- reads WinUI3 .winmd metadata and generates wxl's wrappers.
#
# One module, wxl.gen, with a partition per stage:
#   :profile   -- input: the JSON profiles saying what to generate
#   :crawl     -- the metadata walk producing the type closure
#   :generate  -- orchestration of the output, with one writer per artefact
#                 under gen/
#   :winmd     -- the sole include of winmd_reader.h, re-exported as `md`
#
# main.cpp is an ordinary translation unit: main() may not be attached to a
# named module, so it consumes wxl.gen the way any application would.
add_executable(winui-srcgen main.cpp)

set_target_properties(winui-srcgen PROPERTIES
    CXX_MODULE_STD ON

    # The .cpp files are module implementation units (`module wxl.gen;`), so
    # they have to be scanned for imports like the .ixx files are; scanning
    # ordinary sources is CMP0155-gated and the project leaves that policy at
    # OLD. Same reason as in wxl_add_module_library().
    CXX_SCAN_FOR_MODULES ON
)

target_compile_features(winui-srcgen PRIVATE cxx_std_23)

wxl_target_modules(winui-srcgen
    PRIVATE
        wxl.gen.ixx

        winmd.ixx
        md.ixx
        profile.ixx
        crawl.ixx
        generate.ixx
        run.ixx

        gen/emit.ixx
        gen/types.ixx
        gen/metadata.ixx
        gen/projection.ixx
        gen/members.ixx
        gen/writers.ixx
    SRC
        profile.cpp
        crawl.cpp
        generate.cpp
        run.cpp

        gen/emit.cpp
        gen/types.cpp
        gen/metadata.cpp
        gen/projection.cpp
        gen/members.cpp

        gen/collections.cpp
        gen/styles.cpp
        gen/symbols.cpp
        gen/keys.cpp
        gen/tags.cpp
        gen/enums_structs.cpp
        gen/event_args.cpp
        gen/interfaces.cpp
        gen/classes.cpp
        gen/dsl.cpp
        gen/schema.cpp
        gen/cmake.cpp

        xml_input.h
        xml_input.cpp
)

source_group("gen" REGULAR_EXPRESSION "gen/.*")

target_link_libraries(winui-srcgen
    PRIVATE
        winmd_reader
        # The profiles are read with wxl's own JSON reader -- comments and
        # all, which is the shape our settings files are written in.
        wxl::json
        # The nuspec of a WindowsAppSDK release is read with wxl's own XML
        # reader; it brings wxl::core with it, which is where the STA pool
        # main() builds comes from.
        wxl::xml
        # join(), ascii_lower() and the rest of the text handling this tool
        # used to keep copies of.
        wxl::core
)

# Before the profiles below, not after: this lays the sources out by directory
# with source_group(TREE), and the explicit grouping of the profiles has to be
# the one that wins.
wxl_set_target_folders(winui-srcgen)

# Input profiles -- not compiled, listed so they show up in the IDE tree.
file(GLOB _wxl_gen_profiles CONFIGURE_DEPENDS "profiles/*.json")
target_sources(winui-srcgen PRIVATE ${_wxl_gen_profiles})
source_group("profiles" FILES ${_wxl_gen_profiles})
set_source_files_properties(${_wxl_gen_profiles} PROPERTIES HEADER_FILE_ONLY ON)
