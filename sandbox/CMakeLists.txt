# main.cpp illustrates the target wxl API shape against a full WinUI3 app
# scaffold (pch.h, properties.h) that doesn't exist yet, so it isn't built.
# props.cpp is plain C++ and prototypes the Property/Event DSL mechanism
# standalone, so it's built and run as a sandbox executable.
add_executable(wxl-props-sandbox
    props.cpp
)
target_include_directories(wxl-props-sandbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wxl-props-sandbox PRIVATE Boost::headers)

# The Windows App Runtime bootstrapper (MddBootstrap.h / .lib / .dll) --
# used by wxl/core/bootstrap.cpp to resolve the WinUI3 framework package at
# process startup, internally, so consuming apps never write bootstrap
# boilerplate themselves. Ships in the WindowsAppSDK.Foundation NuGet package,
# not the top-level metapackage.
# 2.3.5 is the Foundation version actually cached alongside the 2.3.2 WinUI
# metadata wxl already targets (no exact-matching top-level WindowsAppSDK
# release is cached; Foundation's own versioning is close enough -- the
# bootstrapper only cares about the major release number, see
# wxl/core/bootstrap.cpp).
set(WXL_WINDOWSAPPSDK_FOUNDATION_DIR
    "D:/Users/Dima/.nuget/packages/microsoft.windowsappsdk.foundation/2.3.5"
    CACHE PATH "WindowsAppSDK.Foundation NuGet package (Bootstrap headers/lib/dll)"
)

# Copies the Windows App Runtime bootstrapper DLL next to `target`'s output
# so MddBootstrapInitialize (called from inside wxl-impl) can actually load
# it at process startup -- wxl-impl is a static library, so this can't be
# handled once at the library level, every final executable needs its own
# copy. A real wxl package/install step would do this automatically as part
# of consuming wxl; this sandbox calls it by hand per executable for now.
function(wxl_copy_runtime_dependencies target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll"
            "$<TARGET_FILE_DIR:${target}>"
    )
endfunction()

# wxl-core: refcounted/sta_allocator and their small dependencies -- see
# wxl-core/CMakeLists.txt.
add_subdirectory(wxl-core)

# wxl-impl: the impl:: types with out-of-line members (wxl::impl::UIElement's
# constructor and its nested UIElementOverrides shim -- see
# wxl/impl/UIElement.cpp), plus the Windows App Runtime bootstrap glue
# (wxl/core/bootstrap.cpp). A real winui-srcgen build would produce many such
# translation units, one compiled library. No winrt/cppwinrt dependency; just the
# Windows SDK's own raw COM headers (<unknwn.h>/<inspectable.h>) plus the
# WindowsAppSDK bootstrapper's own header (also raw COM/ABI, no cppwinrt).
add_library(wxl-impl STATIC
    wxl/impl/UIElement.cpp
    wxl/impl/FrameworkElement.cpp
    wxl/core/bootstrap.cpp
)
target_include_directories(wxl-impl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated
)
target_include_directories(wxl-impl PRIVATE
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)
target_link_libraries(wxl-impl PUBLIC wxl-core)
target_link_libraries(wxl-impl PRIVATE
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib
)
target_compile_definitions(wxl-impl PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)

# uielement_demo.cpp: wxl::UIElement / wxl::impl::UIElement compiled against
# a hand-generated *bare* WinRT ABI interface (wxl/abi/Microsoft.UI.Xaml.h)
# and the real generated PropertyKey/EventKey (../generated). Never constructs
# a live UIElement -- see the comment at the top of uielement_demo.cpp.
add_executable(wxl-uielement-sandbox
    uielement_demo.cpp
)
target_link_libraries(wxl-uielement-sandbox PRIVATE wxl-impl)

# overrides_demo.cpp: Overrides authoring -- the hook lives directly on
# wxl::impl::UIElement (protected virtual), the ABI-facing shim is a private
# nested class created once as a UIElement member. Plain inheritance +
# override, no CRTP/ControlT<D>, non-interlocked ref-counting, StaObject
# allocation. Actually run (not just compiled), since the whole exercised
# path is wxl's own code plus the test-only mocks defined in the file.
add_executable(wxl-overrides-sandbox
    overrides_demo.cpp
)
target_link_libraries(wxl-overrides-sandbox PRIVATE wxl-impl)

# framework_element_activation.cpp: cheap probe for whether the WinUI3 runtime
# allows activating a
# real Microsoft.UI.Xaml.FrameworkElement, now going through wxl-impl's own
# Windows App Runtime bootstrap (wxl::impl::ensure_windows_app_runtime_initialized)
# instead of raw RoGetActivationFactory with no bootstrap at all. Otherwise
# still raw WinRT COM, no cppwinrt.
add_executable(wxl-framework-element-activation-sandbox
    framework_element_activation.cpp
)
target_link_libraries(wxl-framework-element-activation-sandbox PRIVATE wxl-impl runtimeobject.lib CoreMessaging.lib)
wxl_copy_runtime_dependencies(wxl-framework-element-activation-sandbox)

# live_app_activation.cpp: the framework_element_activation.cpp probe
# hit RPC_E_WRONG_THREAD even with a real outer identity and a manually
# created DispatcherQueueController -- modeled after the working
# M:\source\WxlApp1\WxlApp1\main.cpp example, this runs the same
# CreateInstance + blind-aggregation identity probe from inside a genuine
# Application::Start/OnLaunched context instead. Real cppwinrt for the
# App/OnLaunched scaffolding only (test/host code, not wxl itself); needs both
# vcpkg's cppwinrt base runtime headers
# (winrt/base.h, via Microsoft::CppWinRT) and this repo's own generated
# WinUI3 projection headers (../generated/cppwinrt, which has
# winrt/Microsoft.UI.Xaml.h etc. -- vcpkg's cppwinrt package only covers OS
# namespaces).
add_executable(wxl-live-app-activation-sandbox
    live_app_activation.cpp
)
target_include_directories(wxl-live-app-activation-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cppwinrt
)
target_compile_definitions(wxl-live-app-activation-sandbox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wxl-live-app-activation-sandbox PRIVATE
    wxl-impl
    Microsoft::CppWinRT
    runtimeobject.lib
)
wxl_copy_runtime_dependencies(wxl-live-app-activation-sandbox)

# activation_factory_demo.cpp: validates wxl::impl::ActivationFactory<Obj, I>
# (wxl/core/activation_factory.h) -- wxl's own cached, self-patching
# activation-factory lookup, not cppwinrt's winrt::make<T>() -- against a
# real, publicly (non-aggregating) activatable class, Grid. Same
# live-Application bootstrap pattern as live_app_activation.cpp.
add_executable(wxl-activation-factory-sandbox
    activation_factory_demo.cpp
)
target_include_directories(wxl-activation-factory-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cppwinrt
)
target_compile_definitions(wxl-activation-factory-sandbox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wxl-activation-factory-sandbox PRIVATE
    wxl-impl
    Microsoft::CppWinRT
    runtimeobject.lib
)
wxl_copy_runtime_dependencies(wxl-activation-factory-sandbox)

# aggregated_activation_demo.cpp: validates two-phase construction
# against a real, *aggregating* factory -- FrameworkElement's
# IFrameworkElementFactory::CreateInstance(outer, &inner, &value) -- with
# our own real wxl::impl::UIElement (default-constructed, then
# bind_inspectable()'d) as the outer identity, not a throwaway TestOuter.
# Same live-Application bootstrap pattern as live_app_activation.cpp.
add_executable(wxl-aggregated-activation-sandbox
    aggregated_activation_demo.cpp
)
target_include_directories(wxl-aggregated-activation-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cppwinrt
)
target_compile_definitions(wxl-aggregated-activation-sandbox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wxl-aggregated-activation-sandbox PRIVATE
    wxl-impl
    Microsoft::CppWinRT
    runtimeobject.lib
)
wxl_copy_runtime_dependencies(wxl-aggregated-activation-sandbox)

# framework_element_aggregated_demo.cpp: validates the two-phase
# construction / on_bind_inspectable() chain-of-responsibility across two
# real links (wxl::impl::FrameworkElement on top of wxl::impl::UIElement),
# not just one.
add_executable(wxl-framework-element-aggregated-sandbox
    framework_element_aggregated_demo.cpp
)
target_include_directories(wxl-framework-element-aggregated-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cppwinrt
)
target_compile_definitions(wxl-framework-element-aggregated-sandbox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wxl-framework-element-aggregated-sandbox PRIVATE
    wxl-impl
    Microsoft::CppWinRT
    runtimeobject.lib
)
wxl_copy_runtime_dependencies(wxl-framework-element-aggregated-sandbox)

# interface_cache_demo.cpp / wxl/impl/interface_cache.cpp: validates the design
# where Impl leans on the real cppwinrt projection internally --
# wxl::Object/ContentControl/ButtonBase/Button
# form one ordinary inheritance chain, each level's Impl holding real
# cppwinrt-projected *interface* smart pointers (IContentControl/
# IButtonBase/IButton), lazily QueryInterfaced and cached per object.
# interface_cache.cpp genuinely depends on the heavy cppwinrt projection
# headers (unlike UIElement.cpp/FrameworkElement.cpp's bare-ABI approach,
# kept as-is/unmodified reference) -- built directly into this demo
# executable rather than folded into wxl-impl, since wxl-impl's own
# "no cppwinrt dependency" shape isn't being changed by this prototype.
add_executable(wxl-interface-cache-sandbox
    interface_cache_demo.cpp
    wxl/impl/interface_cache.cpp
)
target_include_directories(wxl-interface-cache-sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../generated/cppwinrt
)
target_compile_definitions(wxl-interface-cache-sandbox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wxl-interface-cache-sandbox PRIVATE
    wxl-impl
    Microsoft::CppWinRT
    runtimeobject.lib
)
wxl_copy_runtime_dependencies(wxl-interface-cache-sandbox)
