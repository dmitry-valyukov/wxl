# Смотрелка для wxl.html: загружает разметку примера из wxl.html/sample в
# HtmlBlock, чтобы результат разбора можно было оценить глазами -- эталонные
# тесты проверяют дерево, а не картинку.

wxl_add_executable(sample.htmlview
    main.cpp
)

# Разметка примера едет рядом как ресурсы, прямо из wxl.html/sample -- один
# источник истины для того, что показывает смотрелка и что называет
# документация самого модуля. Не wxl_target_assets(): эти папки лежат в
# другом месте дерева и ложатся рядом под своими именами.
add_custom_command(TARGET sample.htmlview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/wxl.html/sample" "$<TARGET_FILE_DIR:sample.htmlview>/Assets"
    COMMENT "Copying the wxl.html samples beside sample.htmlview")

# Смайлики RSDN едут туда же, в Assets/smiles: разборщик выдаёт
# <img src="smiles/...">, а этот путь разрешается от той же базовой папки.
add_custom_command(TARGET sample.htmlview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/wxl.rsdn/smiles" "$<TARGET_FILE_DIR:sample.htmlview>/Assets/smiles"
    COMMENT "Copying the RSDN smiles beside sample.htmlview")
