# /cmake/all.cmake

# ---- Most common infrastructure ----
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ---- Preset configuration types if not provided via CLI ----
if(WIN32 AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release)
endif()

if(WIN32)
    add_compile_options(/permissive- /utf-8)
endif()

set(_old_module_path ${CMAKE_MODULE_PATH})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# --- includes begin ----
# Pure helpers (wxl_add_module_library, wxl_set_target_folders, wxl_add_gtest).
# The same file is installed with the package, so keep it free of global state --
# everything above this line is what makes config.cmake project-local.
include("${CMAKE_CURRENT_LIST_DIR}/wxl_helpers.cmake")
# --- includes end ----

set(CMAKE_MODULE_PATH ${_old_module_path})
unset(_old_module_path)
