# Проба RandomAccessStreamReference::CreateFromUri с адресом file:// на голой
# проекции cppwinrt: читается ли ссылка напрямую и после прохода через буфер
# обмена (в своём процессе и в чужом), какие написания адреса платформа принимает,
# как из адреса получить путь, что спрашивает у ссылки буфер. Что делает каждая
# часть -- в начале main.cpp. Журнал -- stream-reference-uri-probe.log рядом с
# исполняемым файлом.
wxl_add_executable(sandbox.stream-reference-uri-probe
    main.cpp
)

target_include_directories(sandbox.stream-reference-uri-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.stream-reference-uri-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib shcore.lib shlwapi.lib)
