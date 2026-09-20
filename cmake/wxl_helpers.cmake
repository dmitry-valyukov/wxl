# /cmake/wxl_helpers.cmake
#
# The portable half of wxl's CMake helpers: pure functions, no global side
# effects. This is the file that travels with the package -- wxl-config.cmake
# includes it, so a project doing find_package(wxl) gets wxl_add_module_library()
# and friends for its own targets.
#
# Consequently nothing here may touch project-wide state (compile options,
# configuration types, USE_FOLDERS, testing, ...): a package must not silently
# reconfigure its consumer. That kind of setup belongs in config.cmake, which is
# used by wxl's own build and is not installed.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/wxl_add_module_library.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wxl_target_folders.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wxl_add_gtest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wxl_app_icon.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wxl_target_assets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wxl_add_executable.cmake")
