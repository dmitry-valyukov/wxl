# Проба на обёртках wxl: те же шаги, что у sandbox/StreamRefUriProbe (голый
# cppwinrt), но через wxl::RandomAccessStreamReference, wxl::Operation,
# wxl::DataPackage и wxl::Clipboard -- так, как их зовёт пример Gallery. Точка
# входа -- wWinMain самой wxl, приложение -- wxl_launched() в main.cpp. Проекция
# cppwinrt видна только server.cpp: картинка от системного кодировщика, сервер
# на петле и разбор исключения -- то, что вокруг предмета пробы, а не он.
wxl_add_executable(sandbox.stream-ref-uri-wxl-probe
    main.cpp
    server.cpp
    server.h
    pch.h
)

target_include_directories(sandbox.stream-ref-uri-wxl-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.stream-ref-uri-wxl-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)
