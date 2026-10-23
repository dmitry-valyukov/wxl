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

    # /Zc:throwingNew tells the compiler that allocation functions follow the
    # standard: one that is not noexcept never returns null. The pool never
    # does -- it stops the process instead. As of MSVC 14.53.36911 the compiler
    # drops its null test only after the global operator new; after a
    # class-specific one, the pool's included, the test still stands (checked in
    # the listing). The option is kept to state the intent, and a compiler that
    # honours it for class-specific allocation functions gets it for free.
    # For the whole directory rather than as a usage requirement of wxl::core: a
    # target-level option is part of what CMake fingerprints when it synthesizes
    # the `std` module target, and two fingerprints in one tree give the module
    # two targets -- and, under CMake 4.4.4, a cycle between them.
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Zc:throwingNew>)
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
