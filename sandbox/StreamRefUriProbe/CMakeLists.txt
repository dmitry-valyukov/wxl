# Проба: читается ли RandomAccessStreamReference, сделанный из адреса со схемой
# file, и что с такой ссылкой делает буфер обмена. Проекция cppwinrt напрямую,
# без обёрток wxl и без wxl.ui -- вопрос в том, что отвечает сама Windows;
# обёртки проверяются другой пробой теми же шагами. Консольная программа со
# своим main и простым окном (Clipboard в классическом приложении работает у
# окна переднего плана); от wxl нужен только platform.h -- единственный путь к
# windows.h.
add_executable(sandbox.stream-ref-uri-probe
    main.cpp
)

target_include_directories(sandbox.stream-ref-uri-probe PRIVATE ${PROJECT_SOURCE_DIR}/wxl.core/src)
target_link_libraries(sandbox.stream-ref-uri-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)
wxl_set_target_folders(sandbox.stream-ref-uri-probe)
