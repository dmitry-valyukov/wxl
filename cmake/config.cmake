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
    # For the whole directory rather than as a usage requirement of wxl::core:
    # CMake compares a consumer's compile options with those of the `std`
    # module target, and a consumer whose options differ gets a synthesized
    # copy of the module. wxl::core also carries /Gw in Release only; with this
    # option on the target too, its consumers would differ from the module in
    # both configurations, and differently in each. CMake 4.4.1 to 4.4.4 under
    # Ninja Multi-Config makes the Debug and the Release copy depend on each
    # other and stops the generate step on the cycle. Set here, the option
    # reaches the module target as well: Debug consumers use the module as it
    # is, and only Release ones get a copy.
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
