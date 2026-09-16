# /cmake/wxl_add_module_library.cmake

include_guard(GLOBAL)

function(wxl_target_modules target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "\n[ERROR]: Target '${target}' does not exist!\n"
            "Make sure you call wxl_target_modules() strictly AFTER defining the target via add_executable() or add_library().\n"
        )
    endif()

    set(options "")
    set(multiValueArgs PUBLIC PRIVATE INTERFACE SRC HEADERS)
    cmake_parse_arguments(ARG "${options}" "" "${multiValueArgs}" ${ARGN})

    get_target_property(TARGET_BASE_DIR ${target} SOURCE_DIR)

    macro(_process_scope SCOPE SET_NAME FILES_LIST)
        if(${FILES_LIST})
            set(ABSOLUTE_FILES "")
            foreach(FILE IN LISTS ${FILES_LIST})
                if(IS_ABSOLUTE "${FILE}")
                    list(APPEND ABSOLUTE_FILES "${FILE}")
                else()
                    list(APPEND ABSOLUTE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
                endif()
            endforeach()

            target_sources(${target} ${SCOPE}
                FILE_SET ${SET_NAME} TYPE CXX_MODULES
                BASE_DIRS ${TARGET_BASE_DIR}
                FILES ${ABSOLUTE_FILES}
            )

            # Module units opt out of the precompiled header. CMake implements a
            # PCH by force-including it (/FI) into every source, which lands
            # ahead of a unit's `module;` / `export module` line; MSVC then
            # rejects the pair on a module interface (C2855: /Zi inconsistent
            # with precompiled header). A module unit gains nothing from the PCH
            # regardless -- it includes its handful of headers by hand in the
            # global module fragment. Marked here, where the module files are
            # already known, so no module library's CMakeLists has to remember
            # to (and WXL_ENABLE_PCH stays usable for the plain translation
            # units a target may still have).
            set_source_files_properties(${ABSOLUTE_FILES}
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        endif()
    endmacro()

    _process_scope(PRIVATE   "private_modules"   ARG_PRIVATE)
    _process_scope(PUBLIC    "modules"           ARG_PUBLIC)
    _process_scope(INTERFACE "interface_modules" ARG_INTERFACE)

    # Public headers: the ones a module interface unit still has to #include in
    # its global module fragment (macros and C types cannot travel through
    # `import`). They must be part of the target's interface, because whoever
    # consumes the target compiles those module interfaces in its own build tree
    # and needs the headers to be there. A HEADERS file set also carries the
    # include directory -- BASE_DIRS while building, the install destination
    # afterwards -- so consumers need no target_include_directories() of ours.
    if(ARG_HEADERS)
        set(ABSOLUTE_HEADERS "")
        foreach(FILE IN LISTS ARG_HEADERS)
            if(IS_ABSOLUTE "${FILE}")
                list(APPEND ABSOLUTE_HEADERS "${FILE}")
            else()
                list(APPEND ABSOLUTE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
            endif()
        endforeach()

        target_sources(${target} PUBLIC
            FILE_SET headers TYPE HEADERS
            BASE_DIRS ${TARGET_BASE_DIR}
            FILES ${ABSOLUTE_HEADERS}
        )
    endif()

    # Ordinary (non-module-interface) sources: implementation units, plain
    # translation units and private headers. Without this they were parsed out
    # of the argument list and then silently dropped, so nothing under SRC was
    # ever compiled into the target.
    if(ARG_SRC)
        target_sources(${target} PRIVATE ${ARG_SRC})

        # A module *implementation* unit (`module foo;`, after an optional
        # `module;` global module fragment) cannot take the force-included PCH
        # any more than an interface unit can -- MSVC raises the same C2855. But
        # SRC also carries plain translation units and private headers, which a
        # PCH does help, so they are told apart here by their module
        # declaration: only the module units opt out, and a plain .cpp keeps the
        # header. A partition's colon (`module foo:bar;`) is covered too.
        foreach(FILE IN LISTS ARG_SRC)
            if(IS_ABSOLUTE "${FILE}")
                set(_src "${FILE}")
            else()
                set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
            endif()
            if(EXISTS "${_src}")
                file(STRINGS "${_src}" _module_decl
                    REGEX "^[ \t]*(export[ \t]+)?module[ \t]+[A-Za-z_]" LIMIT_COUNT 1)
                if(_module_decl)
                    set_source_files_properties("${_src}"
                        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
                endif()
            endif()
        endforeach()
    endif()
endfunction()

function(wxl_add_module_library target)
    set(options STATIC SHARED MODULE)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    set(_lib_type "STATIC")
    if(ARG_SHARED)
        set(_lib_type "SHARED")
    elseif(ARG_MODULE)
        set(_lib_type "MODULE")
    endif()

    add_library(${target} ${_lib_type})

    set_target_properties(${target} PROPERTIES
		CXX_MODULE_STD ON

		# Debug and Release land in the same install prefix, so the archives must
		# not share a file name.
		DEBUG_POSTFIX d

		# The .cpp files are module implementation units (`module wxl.core;`), so
		# they have to be scanned for imports like the .ixx files are. Scanning
		# ordinary C++ sources is CMP0155-gated, and the project's
		# cmake_minimum_required(3.25) leaves that policy at OLD -- without this
		# they would compile with no module graph and fail to find `wxl.core`.
		CXX_SCAN_FOR_MODULES ON
	)

	target_compile_features(${target}
		PRIVATE	  cxx_std_23
		INTERFACE cxx_std_23
		PUBLIC    cxx_std_23
	)

	wxl_target_modules(${target} ${ARGN})
endfunction()
