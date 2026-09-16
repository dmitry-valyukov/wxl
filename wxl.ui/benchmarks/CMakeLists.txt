# The two halves of one measurement: the same scene of controls, built once
# through wxl's declarative syntax and once through the cppwinrt projection by
# hand. What they share -- the shape of the scene, the stopwatch and the table
# -- is in scene_shape.h and scene_report.h, so neither can quietly measure a
# different scene from the other.
#
# Console subsystem executables, unlike the samples: each opens its window and
# still has somewhere to print its table. A GUI subsystem process would have
# to be asked for its numbers some other way.
#
# Both need the Windows App Runtime bootstrapper beside them, for the same
# reason a sample does: unpackaged, no WinUI3 class activates until the
# framework package is resolved.

set(_wxl_scene_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

function(_wxl_scene_benchmark target)
    wxl_set_target_folders(${target})

    if(EXISTS "${_wxl_scene_bootstrap_dll}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_wxl_scene_bootstrap_dll}" "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying the Windows App Runtime bootstrapper beside ${target}")
    else()
        message(WARNING
            "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_scene_bootstrap_dll}; "
            "${target} will fail to start until it is beside the executable.")
    endif()
endfunction()

# ---- The declarative half ----
#
# An application as one written against wxl looks: it links wxl::ui and
# nothing else, and it never sees the cppwinrt projection.
add_executable(wxl.ui.scene-wxl-benchmark
    scene_shape.h
    scene_report.h
    scene_wxl.cpp
    app.manifest
)

target_link_libraries(wxl.ui.scene-wxl-benchmark PRIVATE wxl::ui)

_wxl_scene_benchmark(wxl.ui.scene-wxl-benchmark)

# ---- The hand-written half ----
#
# Not one line of wxl is linked here: what is being measured is the projection
# on its own, so this target wires up the same runtime dependencies wxl would
# have wired up for it -- the projection headers, the bootstrapper, and the
# WinRT runtime object library.
add_executable(wxl.ui.scene-winrt-benchmark
    scene_shape.h
    scene_report.h
    scene_winrt.cpp
    app.manifest
)

target_compile_features(wxl.ui.scene-winrt-benchmark PRIVATE cxx_std_23)

target_include_directories(wxl.ui.scene-winrt-benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_compile_definitions(wxl.ui.scene-winrt-benchmark PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(wxl.ui.scene-winrt-benchmark PRIVATE
    Microsoft::CppWinRT
    runtimeobject.lib
)

if(EXISTS "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")
    target_link_libraries(wxl.ui.scene-winrt-benchmark PRIVATE
        "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")
endif()

# It imports nothing, so there is nothing to scan for -- and scanning would
# hand it the std module it never asks for.
set_target_properties(wxl.ui.scene-winrt-benchmark PROPERTIES
    CXX_MODULE_STD OFF
    CXX_SCAN_FOR_MODULES OFF)

_wxl_scene_benchmark(wxl.ui.scene-winrt-benchmark)
