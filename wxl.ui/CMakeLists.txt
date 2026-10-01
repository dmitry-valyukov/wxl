# wxl.ui -- the WinUI3 backend of wxl: the hand-written, given-from-above
# levels (wxl::Object / wxl::DependencyObject and the impl:: plumbing they
# rest on) plus everything winui-srcgen generates from the profiles.
#
# Buildable both standalone (its own preset, see CMakePresets.json) and as a
# subdirectory of a larger build. Standalone is the normal case, because
# consuming wxl.core means consuming a C++20 module: that needs a Ninja
# generator and CMake's experimental `import std;` gate opened before
# project() -- exactly what wxl.core's own gate file below checks.
#
# The generated tree brings its own CMakeLists.txt, which only calls
# target_sources() on this target -- hence add_subdirectory(src/generated)
# at the *end* of this file, after the target exists.
cmake_minimum_required(VERSION 3.28)

# Every CMake script in this tree is shared, one level up.
set(WXL_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(${WXL_CMAKE_SCRIPT_DIR}/vcpkg_bootstrap.cmake)
    include(${WXL_CMAKE_SCRIPT_DIR}/wxl_import_std_gate.cmake)
    project(wxl-ui VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# ---- wxl.core: refcounted, intrusive_ptr, the STA allocator ----
# A top-level project of its own, consumed either as an installed package
# (point CMAKE_PREFIX_PATH at the install prefix) or as a target a larger
# build already created.
if(NOT TARGET wxl::core)
    find_package(wxl CONFIG REQUIRED COMPONENTS core)
endif()

# ---- cppwinrt: the WinUI3 projection headers ----
# vcpkg's cppwinrt package covers the OS namespaces (winrt/base.h,
# winrt/Windows.*.h) only; the Microsoft.UI.Xaml.* projection is generated
# from the WindowsAppSDK metadata and lives in the repo.
find_package(cppwinrt CONFIG REQUIRED)

# Where the NuGet packages are, and at which versions: cmake/wxl_packages.cmake.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/wxl_packages.cmake")

# Relative to this directory, not CMAKE_SOURCE_DIR: wxl.ui builds both
# standalone and as a subdirectory, and the projection lives in the repo
# next to both.
set(WXL_CPPWINRT_PROJECTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../generated"
    CACHE PATH "Directory holding the generated WinUI3 cppwinrt projection (winrt/Microsoft.UI.Xaml.h, ...)")

if(NOT EXISTS "${WXL_CPPWINRT_PROJECTION_DIR}/winrt/Microsoft.UI.Xaml.h")
    message(FATAL_ERROR
        "No WinUI3 cppwinrt projection at ${WXL_CPPWINRT_PROJECTION_DIR}. "
        "Point WXL_CPPWINRT_PROJECTION_DIR at the directory produced by "
        "cppwinrt.exe for the WindowsAppSDK metadata.")
endif()

# The Windows App Runtime bootstrapper -- wxl owns its own runtime
# dependencies, so this is wired here
# rather than by every consuming application. Declared before the target,
# since both the include path and the import library below use it.
wxl_nuget_package_dir(_wxl_foundation_dir Microsoft.WindowsAppSDK.Foundation)
set(WXL_WINDOWSAPPSDK_FOUNDATION_DIR
    "${_wxl_foundation_dir}"
    CACHE PATH "WindowsAppSDK.Foundation NuGet package (Bootstrap headers/lib/dll)")

# WinUI3's native interop headers: microsoft.ui.xaml.window.h, which is how
# a Window hands out its HWND, and microsoft.ui.xaml.media.dxinterop.h, which
# is how a SwapChainPanel is given a swap chain. Plain MIDL output, self
# contained, and nothing here is generated from metadata.
#
# Not the InteractiveExperiences package's Microsoft.UI.Interop.h, which
# would answer the same question through a WindowId: it includes the ABI
# header Microsoft.UI.h, which that package does not carry.
wxl_nuget_package_dir(_wxl_winui_dir Microsoft.WindowsAppSDK.WinUI)
set(WXL_WINDOWSAPPSDK_WINUI_DIR
    "${_wxl_winui_dir}"
    CACHE PATH "WindowsAppSDK.WinUI NuGet package (native interop headers)")

# Win2D (Microsoft.Graphics.Canvas) -- CompositionWindow grabs textures with it
# (TextureCache: covers, backdrops) asynchronously into video memory. The native
# DLL activates unpackaged and, like the bootstrapper above, must sit next to the
# consumer's executable -- or the window's constructor faults building the shared
# CanvasDevice. wxl declares the path; the consumer copies the DLL.
wxl_nuget_package_dir(_wxl_win2d_dir Microsoft.Graphics.Win2D)
set(WXL_WIN2D_DIR
    "${_wxl_win2d_dir}"
    CACHE PATH "Win2D (Microsoft.Graphics.Canvas) NuGet package (native runtime DLL)")

add_library(wxl.ui STATIC
    # Given from above -- never generated.
    src/core.h
    src/Object.h
    src/DateTime.h
    src/Object.impl.h
    src/Object.cpp
    src/events.h
    src/event_awaitable.h
    src/BevelEffect.h
    src/BevelEffect.cpp
    src/Button3DEffect.h
    src/Button3DEffect.cpp
    src/HaloEffect.h
    src/HaloEffect.cpp
    src/GaussianBlurEffect.h
    src/GaussianBlurEffect.cpp
    src/GlassEffect.h
    src/GlassEffect.cpp
    src/impl/scene.h
    src/impl/effect_layer.h
    src/impl/effect_layer.cpp
    src/MagnifyEffect.h
    src/MagnifyEffect.cpp
    src/schema.h
    src/impl/event_waits.h
    src/aliases.h
    src/ui.h
    src/Collection.h
    src/Collection.impl.h
    src/Color.h
    src/Relief.h
    src/Thickness.h
    src/TaggedValue.h
    src/Uri.h
    src/FontFamily.h
    src/ImageSource.h
    src/ImageSource.cpp
    src/StringList.h
    src/StringList.cpp
    src/launch.h
    src/launch.cpp

    # Hand-written plumbing the generated code leans on.
    src/event_token.h
    src/impl/activation_factory.h
    src/impl/statics.h
    src/impl/statics.cpp
    src/impl/runtime_class_name.h
    src/impl/com_ptr.h
    src/impl/release_only_ptr.h
    src/impl/event_token.h
    src/impl/abi_layout.cpp
    src/impl/hresult.h
    src/impl/hresult.cpp
    src/impl/member.h
    src/impl/member.cpp
    src/geometry.h
    src/impl/app_icon.h
    src/impl/app_icon.cpp
    src/impl/bootstrap.h
    src/impl/bootstrap.cpp
    src/impl/window_constraints.h
    src/impl/window_constraints.cpp
    src/impl/fluent_symbol.h
    src/impl/fluent_symbol.cpp
    src/impl/cursor.h
    src/impl/cursor.cpp
    src/ApplicationFolder.h
    src/impl/application_folder.h
    src/impl/application_folder.cpp
    src/FileDrop.h
    src/FileDrop.cpp
    src/WindowHandle.h
    src/WindowPlacement.h
    src/impl/window_placement.h
    src/impl/window_frame.cpp
    src/impl/window_frame.h
    src/WindowPlacement.cpp
    src/WindowBackdrop.h
    src/WindowBackdrop.cpp
    src/TrayIcon.h
    src/TrayIcon.cpp
    src/style_lookup.h
    src/brush_lookup.h
    src/resource_lookup.cpp
    src/Bind.h
    src/Card.h
    src/Card.cpp
    src/Panels.h
    src/Panels.cpp
    src/SplitPanel.h
    src/SplitPanel.cpp
    src/CustomLayout.h
    src/CustomLayout.cpp
    src/AppZoom.h
    src/AppZoom.cpp
    src/ZoomEffect.h
    src/ZoomEffect.cpp
    src/impl/binding.h
    src/binding.cpp
    src/WindowShade.h
    src/WindowShade.cpp
    src/WindowFit.h
    src/WindowFit.cpp
    src/ShowDialog.h
    src/ShowDialog.cpp
    src/ThemeBrush.h
    src/ThemeBrush.cpp
    src/WindowModal.h
    src/WindowModal.cpp
    src/DrawingSurface.h
    src/impl/composition_interop.h
    src/DrawingSurface.cpp
    src/CompositionWindow.h
    src/CompositionWindow.cpp
    src/TextureCache.h
    src/TextureCache.cpp
    src/UiThread.h
    src/UiThread.cpp
    src/FormattedBlock.h
    src/FormattedBlock.impl.h
    src/FormattedBlock.cpp
    src/MarkupBlock.h
    src/MarkupBlock.impl.h
    src/markup_build.h
    src/MarkupBlock.cpp
    src/HtmlBlock.h
    src/HtmlBlock.impl.h
    src/HtmlBlock.cpp
    src/BbBlock.h
    src/BbBlock.impl.h
    src/BbBlock.cpp
    src/RsdnBlock.h
    src/RsdnBlock.impl.h
    src/RsdnBlock.cpp
    src/impl/grid_definitions.h
    src/impl/grid_definitions.cpp
    src/impl/tool_tip.h
    src/impl/tool_tip.cpp
    src/impl/resource.h
    src/impl/resource.cpp
    src/impl/title_bar_drag.h
    src/impl/title_bar_drag.cpp
    src/Resource.h
    src/impl/geometry_from_text.h
    src/impl/geometry_from_text.cpp
    src/impl/points_from_text.h
    src/impl/points_from_text.cpp
    src/impl/scroll_controller.h
    src/impl/scroll_controller.cpp
    src/impl/swipe_items_mode.h
    src/impl/swipe_items_mode.cpp
    src/impl/wheel_to_parent.h
    src/impl/wheel_to_parent.cpp
    src/impl/automation_name.h
    src/impl/automation_name.cpp
    src/impl/value_box.h
    src/impl/value_box.cpp
    src/impl/activation_factory.cpp
)
add_library(wxl::ui ALIAS wxl.ui)

target_compile_features(wxl.ui PUBLIC cxx_std_23)

# wxl owns the entry point, and it lives in this library rather than in the
# application. The linker infers which CRT startup to use by looking for
# main/wWinMain among the objects it was *given*, and an object pulled out of
# a library is not among them -- so it would settle on the narrow WinMain and
# never find one. Naming the startup routine is what tells it otherwise, and
# it is INTERFACE because it is the executable that needs it.
target_link_options(wxl.ui INTERFACE "/ENTRY:wWinMainCRTStartup")

# src/ so the generated tree can reach "../Object.h"; the generated tree
# adds itself to the include path from its own CMakeLists.txt.
target_include_directories(wxl.ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# The projection is PRIVATE on purpose: consuming code must never need a
# winrt header -- that is the whole compile-time argument.
target_include_directories(wxl.ui PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
    ${WXL_WINDOWSAPPSDK_WINUI_DIR}/include
)

# The keys every windows.h of the tree gets live beside it, in wxl.core's
# platform.h. NOGDI is this library's own: nothing here draws through GDI --
# the pixels come from Direct2D through the compositor -- and the few files
# that paint a GDI surface lift it for themselves.
target_compile_definitions(wxl.ui PRIVATE NOGDI)

target_link_libraries(wxl.ui
    PUBLIC
        wxl::core
        # Public because a consumer writes coroutines of its own the moment it
        # awaits an event, and async::detached_task is what one of those returns. An
        # application still links wxl::ui and nothing else.
        wxl::async
    PRIVATE
        # The markup parsers behind the family of blocks. Private like the
        # projection: the DOM never crosses a control's public interface --
        # markup goes in as a string, primitives come out.
        wxl::html
        wxl::bb
        wxl::rsdn
        Microsoft::CppWinRT
        runtimeobject.lib
        # The graphics stack under a composition drawing surface: a D3D11
        # device, the DXGI device inside it and the D2D device on top --
        # see DrawingSurface.cpp. PRIVATE like the projection: an
        # application draws through the ID2D1DeviceContext it is handed and
        # links none of this itself.
        d3d11.lib
        d2d1.lib
        dxgi.lib
        # WIC, decoding the backdrop image -- see WindowBackdrop.cpp.
        windowscodecs.lib
        # The shell, twice over: files dropped on a window (DragAcceptFiles,
        # DragQueryFileW in FileDrop.cpp, with the window subclass that
        # catches the message they arrive in) and the notification area
        # (Shell_NotifyIconW in TrayIcon.cpp).
        shell32.lib
        comctl32.lib
        # Painting the redirection surface behind the island -- the one
        # spot where GDI is the right tool; see WindowBackdrop.cpp.
        gdi32.lib
        # CreateDispatcherQueueController, for the system compositor a
        # WindowShade hosts on its own overlay window -- see WindowShade.cpp.
        coremessaging.lib
        # DwmSetWindowAttribute: the dark frame and the caption colour of a
        # CompositionWindow follow the application's theme.
        dwmapi.lib
)

# The bootstrapper as a target, not as a path to its import library: then
# CMake knows the DLL behind it, and $<TARGET_RUNTIME_DLLS> of an application
# names it without the application knowing where the SDK lies. GLOBAL, so the
# name reaches the directories where applications are declared; PUBLIC,
# because the DLL has to reach their folders and not only wxl's link line.
if(EXISTS "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")
    add_library(wxl.winappsdk.bootstrap SHARED IMPORTED GLOBAL)
    set_target_properties(wxl.winappsdk.bootstrap PROPERTIES
        IMPORTED_LOCATION
            "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll"
        IMPORTED_IMPLIB
            "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")

    target_link_libraries(wxl.ui PUBLIC wxl.winappsdk.bootstrap)
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.lib at ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}; "
        "applications built on wxl will fail to start.")
endif()

# Win2D activates by name at run time and carries no import library, so
# nothing links it and CMake cannot learn about it on its own. wxl names it
# here, and wxl_target_runtime() puts it beside whoever links wxl.
if(EXISTS "${WXL_WIN2D_DIR}/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")
    set_property(TARGET wxl.ui APPEND PROPERTY WXL_RUNTIME_FILES
        "${WXL_WIN2D_DIR}/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")
else()
    message(WARNING
        "No Microsoft.Graphics.Canvas.dll at ${WXL_WIN2D_DIR}; a CompositionWindow "
        "will fault while building its shared CanvasDevice.")
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" REGULAR_EXPRESSION "src/.*")

# ---- The declarative surface, compiled the way an application sees it ----
# Deliberately linked against wxl.ui only, so it inherits the PUBLIC
# include path and *not* the cppwinrt projection: a winrt:: name leaking into
# a public header breaks this target instead of quietly costing every
# consumer the projection's parse time.
#
# Declared *before* the generated directory because the generated schema's
# own test belongs to this target, and a target has to exist before another
# directory can add sources to it.
add_library(wxl.ui.surface-test OBJECT
    test/dsl_surface.cpp
)
target_link_libraries(wxl.ui.surface-test PRIVATE wxl.ui)

# Last: the generated sources add themselves to the two targets above.
add_subdirectory(src/generated)

# After that subdirectory and not before it: this reads the targets' SOURCES to
# lay them out in the IDE, and until the generated sources are in, half of what
# the library is made of would be missing from the tree.
wxl_set_target_folders(wxl.ui)
wxl_set_target_folders(wxl.ui.surface-test PREFIX "tests")

# Off by default, like wxl.core's and wxl.async's: the benchmarks are
# measurement tools, run deliberately. These two are whole WinUI3
# applications, so they also cost a build the other targets do not.
option(WXL_BUILD_BENCHMARKS "Build the wxl.ui benchmark executables." OFF)

if(WXL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_TESTING)
    # The layout facts hstring_param crossing into WinRT rests on -- one
    # pointer wide on both sides, the handle lent as it is, a fast-pass header
    # cppwinrt reads as a string of its own -- proven where words about them
    # would otherwise have to be believed. Needs only cppwinrt's base header,
    # no WinUI runtime. What hstring_param takes and refuses is in wxl.core's
    # tests; the surface test never runs, so what is about values goes here.
    wxl_add_gtest(wxl.ui.tests
        SRC
            test/hstring_param_test.cpp
        DEPS
            Microsoft::CppWinRT
            wxl::core
    )
    target_include_directories(wxl.ui.tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    # Teardown is header-only over core::function, so its test needs only the
    # header and wxl.core -- not wxl.ui, whose INTERFACE
    # /ENTRY:wWinMainCRTStartup would take a main() away from it. It is a
    # plain executable rather than a gtest because launch.h reaches wxl.core
    # through core.h (include-then-import), which gtest's own standard headers
    # cannot share a translation unit with; it reports through its exit code.
    # CXX_MODULE_STD / scanning as wxl_add_gtest sets, since it consumes a
    # module built with `import std;`.
    add_executable(wxl.ui.teardown-test test/teardown_test.cpp)
    target_link_libraries(wxl.ui.teardown-test PRIVATE wxl::core)
    target_include_directories(wxl.ui.teardown-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(wxl.ui.teardown-test PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON)
    wxl_set_target_folders(wxl.ui.teardown-test PREFIX "tests")
    add_test(NAME wxl.ui.teardown COMMAND wxl.ui.teardown-test)

    # AppZoom -- the steps and the nearest one -- is plain code over
    # wxl.core's observable, so its test builds the one source with wxl.core,
    # the way teardown's does, and reports through the exit code for the same
    # reason: AppZoom.h reaches wxl.core through core.h.
    add_executable(wxl.ui.app-zoom-test test/app_zoom_test.cpp src/AppZoom.cpp)
    target_link_libraries(wxl.ui.app-zoom-test PRIVATE wxl::core)
    target_include_directories(wxl.ui.app-zoom-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(wxl.ui.app-zoom-test PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON)
    wxl_set_target_folders(wxl.ui.app-zoom-test PREFIX "tests")
    add_test(NAME wxl.ui.app-zoom COMMAND wxl.ui.app-zoom-test)
endif()
