wxl_add_module_library(wxl.core STATIC
    PUBLIC
        wxl.core.ixx
        wxl.stdint.ixx

        allocator.ixx
        atomic_trigger.ixx
        binding_scope.ixx
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
        hstring.ixx
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
        observable_list.ixx
        path.ixx
        pool_ptr.ixx
        refcounted.ixx
        semaphore.ixx
        spin_lock.ixx
        sta_allocator.ixx
        sta_stl.ixx
        strings.ixx
        sync_root.ixx
        thread_guard.ixx
        thread_heap.ixx
        thread_id.ixx
        time.ixx
        timeout_timer.ixx
        traceable.ixx
        unicode.ixx
        weak_link.ixx
        zstring_view.ixx

        # fmt: the text buffer, the builder over it and format(), in wxl::core.
        fmt/text_buffer.ixx
        fmt/text_builder.ixx
        fmt/format.ixx

        # async: the future family, task trees, the coroutines and the loop they run
        # on, threads, and the lock-free primitives underneath them.
        async/async_directory.ixx
        async/async_file.ixx
        async/async_mutex.ixx
        async/async_op.ixx
        async/async_semaphore.ixx
        async/barrier.ixx
        async/bump_buffer.ixx
        async/cancellation.ixx
        async/completion_counter.ixx
        async/coroutine_checks.ixx
        async/countdown_event.ixx
        async/drain_stack.ixx
        async/future.ixx
        async/future_shared_state.ixx
        async/overlapped_op.ixx
        async/io_port.ixx
        async/mpsc_channel.ixx
        async/mpsc_queue.ixx
        async/one_shot_event.ixx
        async/pool.ixx
        async/safe_pool.ixx
        async/spsc_channel.ixx
        async/spsc_queue.ixx
        async/spsc_queue_reference_implementation.ixx
        async/sta_loop.ixx
        async/sta_thread.ixx
        async/task.ixx
        async/task_tree.ixx
        async/thread.ixx
        async/thread_group.ixx
        async/threaded_allocator.ixx
        async/turnstile.ixx

        # What is left of the component model: the base, and threaded_component, the one
        # derivative with callers of its own. The containers, the hosted half and the
        # apartments are kept outside this tree.
        # last_error stays because async_file and async_directory throw it.
        async/component.ixx
        async/last_error.ixx
        async/stop_reason.ixx
        async/threaded_component.ixx

        # logging: a log built around one call per message.
        logging/severity.ixx
        logging/entry.ixx
        logging/facility.ixx
        logging/output.ixx
        logging/console_output.ixx
        logging/file_output.ixx
        logging/logger.ixx
        logging/async_logger.ixx

        # xml: the XML reader -- the tree is views into the buffer it takes over.
        xml/core.ixx
        xml/exception.ixx
        xml/encoding.ixx
        xml/node.ixx
        xml/document.ixx
        xml/parser.ixx

        # The arena is the storage scheme itself -- blocks from the STA pool,
        # objects carved by a cursor, the lot released whole -- and the html area
        # takes it up for its own tree rather than keeping a second copy of it.
        xml/arena.ixx

        # Internal -- only the arena asks it for pages. Listed all the same:
        # a partition belongs to the module whether or not the primary
        # interface re-exports it.
        xml/arena_pool.ixx

        # json: the JSON reader, of the same make as the XML one and on its arena.
        json/exception.ixx
        json/value.ixx
        json/document.ixx

        # html: the HTML subset behind HtmlBlock, a tolerant parser whose tree is
        # carved from the arena of the XML reader. :html.builder is the family
        # door: tree_builder hands the sibling tokenizers (wxl.bb, wxl.rsdn) the
        # arena copies and the STA scratch types by name.
        html/node.ixx
        html/builder.ixx
        html/parse.ixx
    HEADERS
        # Included from the global module fragment of the .ixx files, so they are
        # part of the public interface: a consumer compiles those interfaces
        # itself and would otherwise fail with "cannot open include file".
        abi.h
        platform.h
        # The coroutine check itself, a macro -- it is assert when the checks are
        # Debug's -- for the global module fragment of the interfaces that check.
        async/coroutine_checks.h
    SRC
        pch.h

        checks.cpp
        environment.cpp
        grapheme.cpp
        hevent.cpp
        hstring.cpp
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

        fmt/format.cpp

        async/async_directory.cpp
        async/async_file.cpp
        async/async_op.cpp
        async/future.cpp
        async/future_shared_state.cpp
        async/overlapped_op.cpp
        async/io_port.cpp
        async/one_shot_event.cpp
        async/sta_loop.cpp
        async/sta_queue.cpp
        async/task_tree.cpp
        async/thread.cpp
        async/thread_group.cpp
        async/threaded_allocator.cpp

        # The private half of component: its state, shared by every component .cpp
        # that touches it, and included from inside the module purview.
        async/component.impl.h

        async/component.cpp
        async/last_error.cpp
        async/stop_reason.cpp
        async/threaded_component.cpp

        logging/entry.cpp
        logging/output.cpp
        logging/console_output.cpp
        logging/file_output.cpp
        logging/logger.cpp
        logging/async_logger.cpp

        xml/arena.cpp
        xml/arena_pool.cpp
        xml/encoding.cpp
        xml/exception.cpp
        xml/node.cpp
        xml/document.cpp
        xml/parser.cpp

        json/exception.cpp
        json/document.cpp

        html/builder.cpp
        html/parse.cpp
)

# The name consumers use, both in-tree (FetchContent / add_subdirectory) and
# from the installed package -- keeping them identical means a consumer never
# has to know which of the two it got.
add_library(wxl::core ALIAS wxl.core)

# The one system library beyond the defaults: the shell, for the command line
# split into arguments (CommandLineToArgvW in environment.cpp). PUBLIC because
# wxl.core is static, so the consumer's link is where the symbol is resolved.
target_link_libraries(wxl.core PUBLIC shell32.lib)

# fmt is taken as a C++20 module, `import fmt;`, built here from its own
# sources rather than from the vcpkg port, which builds the library alone. A
# module is the one shape that sits beside `import std;` without trouble: fmt
# headers in a global module fragment merge with the std module only as far
# as MSVC's extern "C++" workaround reaches, and fmt/std.h (thread,
# filesystem) is past it. With FMT_IMPORT_STD fmt imports std itself and its
# global module fragment holds C headers alone.
#
# Fetched unless a target fmt::module already exists. SOURCE_SUBDIR names a
# directory fmt does not have, so its own CMakeLists.txt stays out of the
# build: fmt makes its module target for the plain Ninja generator only, with
# flags of its own.
if(NOT TARGET fmt::module)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 1be298e1bd68957e4cd352e1f676f00e07dcfb57 # 12.2.0
        SOURCE_SUBDIR sources-only
    )
    FetchContent_MakeAvailable(fmt)

    add_library(fmt.module STATIC)
    add_library(fmt::module ALIAS fmt.module)

    # fmt.cc includes format.cc after the headers when it finds it beside
    # itself, and that puts the definitions of dragonbox and the locale
    # helpers into the module. An importer then instantiates them from the
    # module instead of taking the instances the module holds, and MSVC 14.51
    # cannot: it loses the template parameter of a definition read back from
    # the module ("'T': undeclared identifier" in format-inl.h). So the module
    # unit is compiled from a copy with no format.cc beside it, and format.cc
    # is an ordinary translation unit of the same library. For the two to
    # link, the module attaches its declarations to the global module
    # (FMT_ATTACH_TO_GLOBAL_MODULE: extern "C++" around the headers), which is
    # what a plain translation unit defines. FMT_OS=0: the file and pipe
    # wrappers of fmt/os.h have no caller here.
    set(_fmt_module_dir "${CMAKE_CURRENT_BINARY_DIR}/fmt-module")
    configure_file("${fmt_SOURCE_DIR}/src/fmt.cc" "${_fmt_module_dir}/fmt.cc" COPYONLY)
    target_sources(fmt.module
        PUBLIC
            FILE_SET fmt TYPE CXX_MODULES
            BASE_DIRS "${_fmt_module_dir}"
            FILES "${_fmt_module_dir}/fmt.cc"
        PRIVATE
            "${fmt_SOURCE_DIR}/src/format.cc"
    )
    set_source_files_properties("${_fmt_module_dir}/fmt.cc" PROPERTIES
        COMPILE_DEFINITIONS "FMT_IMPORT_STD;FMT_ATTACH_TO_GLOBAL_MODULE"
    )
    target_include_directories(fmt.module PRIVATE ${fmt_SOURCE_DIR}/include)
    target_compile_definitions(fmt.module PRIVATE FMT_OS=0)
    target_compile_features(fmt.module PUBLIC cxx_std_23)
    set_target_properties(fmt.module PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )
    wxl_set_target_folders(fmt.module)
endif()

# PUBLIC: text_buffer is fmt::basic_memory_buffer and text_builder::format() is
# a template, so their bodies are compiled in the consumer, which needs the
# module for that. Why fmt rather than std::format is in docs/fmt.md.
target_link_libraries(wxl.core PUBLIC fmt::module)

# PUBLIC: the module interfaces of the async area read it, and whoever compiles
# them -- this target, a consumer that builds them in its own tree, the tests
# that ask what a build checks -- has to get the same answer.
if(WXL_STRICT_CORO)
    target_compile_definitions(wxl.core PUBLIC STRICT_CORO)
endif()

# PRIVATE: every worker thread brings COM up as MTA (async/thread_group.cpp), so
# a worker body can make WinRT calls off the UI thread -- Win2D, say.
# CoInitializeEx/CoUninitialize live in ole32.
target_link_libraries(wxl.core PRIVATE ole32)

# PRIVATE: the thread's dispatcher queue, which start_dispatched() finds or
# makes -- CreateDispatcherQueueController is coremessaging, the activation
# factory and the string behind its name are runtimeobject.
target_link_libraries(wxl.core PRIVATE coremessaging runtimeobject)

# Errors are reported by throwing: a broken document is an exception of the
# XML and JSON readers, allocation is the one thing the HTML parser may throw,
# and writing to a log output can fail -- a full disk, a file that went away --
# which multicast_output catches to drop that output rather than the program.
# MSVC unwinds nothing without /EHsc: the destructors of everything on the
# stack between the throw and the catch are simply skipped. CMake puts the flag
# in CMAKE_CXX_FLAGS itself, but a toolchain file is free to replace those
# flags wholesale, so it is added here only when it is missing, rather than
# duplicated on every source file.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.core PUBLIC /EHsc)
endif()

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

    # wxl.core links fmt::module PUBLIC, so an export of wxl.core has to carry
    # it: the fmt.module made above travels in the same export set, its module
    # source beside wxl's. A fmt::module the project brought itself is the
    # project's to export.
    if(TARGET fmt.module)
        install(TARGETS fmt.module
            EXPORT wxl-core-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core
            FILE_SET fmt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wxl/fmt COMPONENT core
        )
    endif()
endif()

# Beyond the `std` module, wxl.core depends on fmt alone, taken as a module above
# for the text buffer, the builder and format(). The last Boost use,
# boost::intrusive_ptr, was replaced by the :intrusive_ptr partition.
