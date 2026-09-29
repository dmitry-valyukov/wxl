# ---- CMakeList.txt : Top-level CMake project file ----
#
# One common project over the whole tree: the generator (wxl.gen), the core
# runtime library (wxl.core) and the WinUI3 backend (wxl.ui) configure and
# build together, so wxl.ui consumes wxl::core as an in-tree target instead
# of an installed package.
#
# Every CMake script the tree uses lives in cmake/ next to this file and is
# shared by all three projects.
cmake_minimum_required(VERSION 4.3)

# Resolves VCPKG_ROOT (from the vcpkg-managed path file, or the environment)
# and sets the toolchain file itself -- which is why no preset here has to
# name CMAKE_TOOLCHAIN_FILE. It has to run before project().
include(cmake/vcpkg_bootstrap.cmake)

# Opens CMake's experimental `import std;` gate, which wxl.core's module build
# needs. Only the top-level project can open it, and only before project().
# CMAKE_CXX_MODULE_STD below is the request; this is the permission, and CMake
# refuses the request without it ("requires toolchain support, but it was not
# provided").
include(cmake/wxl_import_std_gate.cmake)

project(
    wxl-project VERSION 0.1.0
    DESCRIPTION "Winui Xaml-Less library (wxl)"
    #HOMEPAGE_URL ""
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Set global compiler options ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# ---- No link-time optimization ----
# Measured on this tree, and the measurement went the other way from what one
# expects. The STA pool gets *faster* without /GL -- 28.4 against 31.7 ns per
# allocation on the deep scenario, 38.8 against 48.9 on the small one -- and so
# does the UTF-8 validation, by 18%, because a tight eight-byte-at-a-time loop
# in wxl.core is otherwise inlined into its caller and loses the register
# allocation it had standing alone. The XML parse does not move at all: the
# grammar sits in one translation unit and there is nothing across a boundary
# to inline. Executables come out within a percent either way.
#
# What it costs is plain: a /GL object carries an intermediate representation
# instead of machine code, so the build tree grows by an order of magnitude --
# the static libraries of this tree go from 42 MB to 1.7 GB, wxl.ui.lib
# alone from 26 MB to 1.67 GB -- and every Release link recompiles the whole
# program.

# Off by default, and the presets say nothing about it: building the library
# is what a tree is configured for, and GTest is asked of vcpkg only by
# someone who is going to run the tests. It used to be OFF here and ON in the
# preset -- which meant the declared default was never what anyone got, and
# the requirement list said "optional" about a package without which even the
# configure step failed. One place says it now, and it is this one.
#
#     cmake --preset x64 -DBUILD_TESTING=ON      (sticks in CMakeCache.txt)
#     tools\build.ps1 -Define BUILD_TESTING=ON
option(BUILD_TESTING "Build the testing tree." OFF)

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED CONFIG)
    include(GoogleTest)
endif()

find_package(cppwinrt CONFIG REQUIRED)

# winmd::reader is a header-only WinMD metadata parsing library; not packaged
# by vcpkg, so it's fetched directly from its upstream repo.
include(FetchContent)
FetchContent_Declare(
    winmd
    GIT_REPOSITORY https://github.com/microsoft/winmd.git
    GIT_TAG a363c851a65c4e15743d6589acc1723e9e21b143 # master, 2026-08
)
FetchContent_MakeAvailable(winmd)

# Shared infrastructure plus the wxl_* helper functions (wxl_add_module_library,
# wxl_add_gtest, wxl_set_target_folders) every subproject uses.
include(cmake/config.cmake)

option(WXL_BACKEND "Backend to be used for WXL." "WinUI3")

if(NOT TARGET winmd_reader)
    add_library(winmd_reader INTERFACE)
    target_include_directories(winmd_reader INTERFACE ${winmd_SOURCE_DIR}/src)
endif()

# wxl.core first: wxl.ui links wxl::core, and the generated sources it
# compiles import that module.
add_subdirectory(wxl.core)

# Shared test infrastructure (the gtest fixture, the stress-test harness),
# used by every subproject's test executable.
if(BUILD_TESTING)
    add_subdirectory(test-support)
endif()

# The text buffer and builder over fmt. Beside wxl.core and not inside it, so
# that the core keeps depending on the std module alone; the names stay in
# wxl::core, only the import differs.
add_subdirectory(wxl.fmt)

# The layer above wxl.core: the future family, the threading machinery, and what
# is left of the component model built on them -- component and threaded_component.
# The containers, the hosted half and the apartments had no caller and are kept
# outside this tree.
add_subdirectory(wxl.async)

# The log: wxl.core's text_builder formats a line, wxl.async supplies the
# thread the asynchronous half writes it from.
add_subdirectory(wxl.logging)

add_subdirectory(wxl.xml)

# Читатель JSON -- брат wxl.xml с той же схемой хранения: дерево из видов в
# присвоенный буфер, вырезанное из его же арены.
add_subdirectory(wxl.json)

# The HTML subset behind HtmlBlock: a tolerant parser for foreign markup,
# its tree carved from wxl.xml's arena. Specification: wxl.html/design.md.
add_subdirectory(wxl.html)

# Подсветка синтаксиса: таблицы языков и сканер без регулярных выражений;
# ею пользуется wxl.rsdn, а зависит она только от std.
add_subdirectory(wxl.highlight)

# Братья wxl.html: свои токенизаторы (BB и RSDN-разметка форумов) над общим
# tree_builder семейства и тем же деревом.
add_subdirectory(wxl.bb)
add_subdirectory(wxl.rsdn)

# Профили и обход метаданных -- общее у генератора и редактора профилей.
add_subdirectory(wxl.gen.common)
add_subdirectory(wxl.gen)
add_subdirectory(wxl.ui)

# Визуальный редактор профилей wxl.gen -- приложение на wxl.ui.
add_subdirectory(wxl.gen.ui)

add_subdirectory(samples)

# Песочница: пробы механизмов WinUI, каждая -- отдельное приложение. Собирается
# вместе со всем: рецепт, который никто не компилирует, тихо отстаёт от дерева
# -- эта папка так и простояла закомментированной, пока её цели не рассыпались.
add_subdirectory(sandbox)

# The documentation site. An option because configuring it reaches the network
# once per build tree -- for the theme, and for Doxygen itself when the host has
# nothing recent enough -- and a tree configured to build a library should not
# have to wait for that.
option(WXL_BUILD_DOCS "Build the Doxygen documentation site." ON)
if(WXL_BUILD_DOCS)
    include(cmake/wxl_doxygen.cmake)
    add_subdirectory(docs)
endif()

# The shared CMake scripts, shown in the IDE tree without being built.
file(GLOB_RECURSE _wxl_cmake_scripts CONFIGURE_DEPENDS "cmake/*.*")
add_custom_target(cmake_files SOURCES ${_wxl_cmake_scripts})
source_group("cmake" FILES ${_wxl_cmake_scripts})
