# wxl.gen -- the generator's code, an ordinary library: reads WinUI3 .winmd
# metadata and writes wxl's wrappers. The profiles and the metadata walk it
# shares with the profile editor come from wxl.gen.common through md.h; what
# is here is the output side:
#   generate.h  -- orchestration of the output, with one writer per artefact
#                  under gen/
#   run.h       -- a whole run behind one call
#
# The sources import std and wxl.core after their headers, hence the
# module-library helper: it scans them for imports.
wxl_add_module_library(wxl.gen STATIC
    HEADERS
        wxl.gen.h
        md.h
        generate.h
        run.h

        gen/emit.h
        gen/types.h
        gen/metadata.h
        gen/projection.h
        gen/members.h
        gen/writers.h
    SRC
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
)

source_group("gen" REGULAR_EXPRESSION "gen/.*")

target_link_libraries(wxl.gen
    PUBLIC
        wxl.gen.common
    PRIVATE
        # join(), ascii_lower() and the rest of the text handling this tool
        # used to keep copies of.
        wxl::core
)

wxl_set_target_folders(wxl.gen)

# winui-srcgen -- the executable: main.cpp over the library.
add_executable(winui-srcgen main.cpp)

set_target_properties(winui-srcgen PROPERTIES
    CXX_MODULE_STD ON

    # main.cpp imports std after its headers; scanning ordinary sources is
    # CMP0155-gated and the project leaves that policy at OLD. Same reason as
    # in wxl_add_module_library().
    CXX_SCAN_FOR_MODULES ON
)

target_compile_features(winui-srcgen PRIVATE cxx_std_23)

target_link_libraries(winui-srcgen PRIVATE wxl.gen)

# Before the profiles below, not after: this lays the sources out by directory
# with source_group(TREE), and the explicit grouping of the profiles has to be
# the one that wins.
wxl_set_target_folders(winui-srcgen)

# Input profiles -- not compiled, listed so they show up in the IDE tree.
file(GLOB _wxl_gen_profiles CONFIGURE_DEPENDS "profiles/*.json")
target_sources(winui-srcgen PRIVATE ${_wxl_gen_profiles})
source_group("profiles" FILES ${_wxl_gen_profiles})
set_source_files_properties(${_wxl_gen_profiles} PROPERTIES HEADER_FILE_ONLY ON)
