wxl_add_module_library(wxl.core STATIC
    PUBLIC
        wxl.core.ixx
        wxl.stdint.ixx

        allocator.ixx
        atomic_trigger.ixx
        bit_vector.ixx
        checks.ixx
        compressed_optional.ixx
        directory.ixx
        environment.ixx
        event.ixx
        file.ixx
        function.ixx
        grapheme.ixx
        hevent.ixx
        intrusive_list.ixx
        intrusive_slist.ixx
        intrusive_ptr.ixx
        lightweight_semaphore.ixx
        lock_guard.ixx
        member_offset.ixx
        module_cleanup.ixx
        mutex.ixx
        not_null.ixx
        noncopyable.ixx
        numbers.ixx
        observable.ixx
        path.ixx
        pool_ptr.ixx
        refcounted.ixx
        semaphore.ixx
        spin_lock.ixx
        sta_allocator.ixx
        strings.ixx
        sync_root.ixx
        thread_guard.ixx
        thread_heap.ixx
        thread_id.ixx
        time.ixx
        timeout_timer.ixx
        traceable.ixx
        unicode.ixx
    HEADERS
        # Included from the global module fragment of the .ixx files, so they are
        # part of the public interface: a consumer compiles those interfaces
        # itself and would otherwise fail with "cannot open include file".
        abi.h
        platform.h
    SRC
        pch.h

        checks.cpp
        environment.cpp
        grapheme.cpp
        hevent.cpp
        lightweight_semaphore.cpp
        module_cleanup.cpp
        mutex.cpp
        semaphore.cpp
        spin_lock.cpp
        sta_allocator.cpp
        strings.cpp
        sync_root.cpp
        thread_heap.cpp
        time.cpp
        unicode.cpp
)

# The name consumers use, both in-tree (FetchContent / add_subdirectory) and
# from the installed package -- keeping them identical means a consumer never
# has to know which of the two it got.
add_library(wxl::core ALIAS wxl.core)

# `import std;` support (CMake's std module target). Requires C++23 and a
# toolchain that ships the std module sources (MSVC 17.10+).
set_target_properties(wxl.core PROPERTIES
    # Exported as wxl::core (the wxl:: namespace is added by install(EXPORT)).
    EXPORT_NAME core
)

# ---- Precompiled header (off by default; enable with -DWXL_ENABLE_PCH=ON) ----
# CMake force-includes the PCH (/FI) ahead of a unit's `module;` / `export
# module` line. MSVC 14.51 rejects that on a module unit -- interface and
# implementation alike -- with C2855 (/Zi inconsistent with precompiled header),
# so wxl_target_modules() opts every module unit out of the PCH (see
# cmake/wxl_add_module_library.cmake). The header is left to a target's plain
# translation units -- of which wxl.core has none, so here the PCH currently
# covers nothing; the call stays for targets that do carry plain sources, and
# against the day the toolchain accepts a force-include on module units again.
#
# PRIVATE, never PUBLIC/INTERFACE: pch.h is an internal header, consumers of
# wxl.core must not have it force-included (nor is it installed).
#
# Measured on this tree while the PCH still applied to everything (Ninja
# Multi-Config, MSVC 14.51): clean build ON 15.5s vs OFF 15.1s (no win),
# single-file rebuild ~12% off the edit/build loop for the ~15 of ~85 TUs that
# pull <windows.h>. A clean build is dominated by the serial `import std;` BMI
# (std.ifc alone is 7.5s Debug / 5.0s Release, and nothing else can start until
# it exists), which no PCH can shorten -- std.ifc is an *input* to compiling the
# PCH, and MSVC 14.51 in fact refuses to put `import std;` into a PCH at all
# (error C2129 on /Yc).
if(WXL_ENABLE_PCH)
    target_precompile_headers(wxl.core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
endif()

wxl_set_target_folders(wxl.core)

# ---- IntelliSense configuration refresh (see WXL_UPDATE_INTELLISENSE) ----
# Attached to wxl.core rather than run at configure time on purpose: the
# generator reads the *.obj.modmap files and the .ifc they point at, so it can
# only run once the build has produced them.
if(WXL_UPDATE_INTELLISENSE AND WIN32)
    find_program(WXL_POWERSHELL NAMES pwsh powershell)
    if(WXL_POWERSHELL)
        add_custom_command(TARGET wxl.core POST_BUILD
            COMMAND "${WXL_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                    -File "${PROJECT_SOURCE_DIR}/tools/gen-intellisense-config.ps1"
                    -IfIde
                    -Config $<CONFIG>
                    -BuildDir "${PROJECT_BINARY_DIR}"
                    -Log "${PROJECT_BINARY_DIR}/intellisense-config.log"
            COMMENT "Refreshing IntelliSense configuration ($<CONFIG>)"
            VERBATIM
        )
    else()
        message(WARNING
            "\n[WARNING]: PowerShell not found; .vscode/c_cpp_properties.json will\n"
            "not be refreshed after builds. Run tools/gen-intellisense-config.ps1 by hand.\n"
        )
    endif()
endif()

target_compile_definitions(wxl.core
    PUBLIC
        WXL_VERSION="${PROJECT_VERSION}"
)

# A consumer compiles wxl's module interfaces in its own build tree, so the
# language level has to travel with the target rather than rely on the
# consumer's global CMAKE_CXX_STANDARD.
target_compile_features(wxl.core PUBLIC cxx_std_23)

# /Gw gives every global a COMDAT of its own, the way /Gy does with functions,
# so the linker's /OPT:REF can drop the ones nothing references -- and wxl is
# made of per-class slots and per-interface tables an application never
# touches in full. PUBLIC for the same reason as the language level: a
# consumer building wxl's targets in its own tree has to get the flag with
# them, or its Release differs from the one this tree checks.
target_compile_options(wxl.core PUBLIC $<$<CONFIG:Release>:/Gw>)

if(WXL_INSTALL)
    # One export set per package component, so that a consumer can ask for just
    # the part it needs: find_package(wxl CONFIG REQUIRED COMPONENTS core).
    # The code generator wants core alone; an application wants winui, which
    # will pull core in transitively.
    install(TARGETS wxl.core
        EXPORT wxl-core-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core
        FILE_SET modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wxl COMPONENT core
        FILE_SET headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wxl COMPONENT core
    )
endif()

# wxl.core has no third-party dependencies: everything it needs comes from the `std`
# module. The last Boost use, boost::intrusive_ptr, was replaced by the :intrusive_ptr
# partition. The text builder over fmt is a module of its own, wxl.fmt, for exactly
# that reason.
