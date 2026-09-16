# The eyeball viewer for wxl.html: loads the sample markup from
# wxl.html/sample into an HtmlBlock, so the parser's output is judged by
# looking at it -- the golden tests check the tree, not the picture.

add_executable(sample.htmlview WIN32
    main.cpp
    app.manifest
)

wxl_set_target_folders(sample.htmlview)

target_link_libraries(sample.htmlview PRIVATE wxl::ui)

# The sample markup rides along as assets, straight from wxl.html/sample --
# one source of truth for what the viewer shows and what the module's own
# documentation names.
add_custom_command(TARGET sample.htmlview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/wxl.html/sample" "$<TARGET_FILE_DIR:sample.htmlview>/Assets"
    COMMENT "Copying the wxl.html samples beside sample.htmlview")

# The RSDN smiles ride along too, under Assets/smiles: the tokenizer emits
# <img src="smiles/...">, resolved against the same base directory.
add_custom_command(TARGET sample.htmlview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/wxl.rsdn/smiles" "$<TARGET_FILE_DIR:sample.htmlview>/Assets/smiles"
    COMMENT "Copying the RSDN smiles beside sample.htmlview")

set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.htmlview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.htmlview>"
        COMMENT "Copying the Windows App Runtime bootstrapper beside sample.htmlview")
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_bootstrap_dll}; "
        "sample.htmlview will fail to start until it is beside the executable.")
endif()
