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

# The target is x64 with AVX2 (Haswell, 2013, and later). Under __AVX2__ the STL's
# std::countl_zero is lzcnt alone, with no run-time check of __isa_available, so
# nothing stands between a constant size and a constant size-class index in
# sta_memory_pool. Set for the whole directory and every configuration rather than
# on one target: an inline function of the STL takes another body under __AVX2__,
# and translation units compiled both ways would put two definitions of one
# function into a program. Not every target here links wxl::core (fmt.module, the
# cppwinrt-only probes), so a usage requirement on it would not reach them all.
if(MSVC)
    add_compile_options(/arch:AVX2)
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
