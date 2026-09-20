# The eyeball viewer for wxl.html: loads the sample markup from
# wxl.html/sample into an HtmlBlock, so the parser's output is judged by
# looking at it -- the golden tests check the tree, not the picture.

wxl_add_executable(sample.htmlview
    main.cpp
)

# The sample markup rides along as assets, straight from wxl.html/sample --
# one source of truth for what the viewer shows and what the module's own
# documentation names. Not wxl_target_assets(): these folders come from
# elsewhere in the tree and land under names of their own.
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
