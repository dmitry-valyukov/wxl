# Визуальный редактор профилей генератора проекции (wxl.gen/profiles). Как
# любое приложение на wxl, стоит на wxl::ui; WIN32 делает точкой входа
# wWinMain самой wxl.

wxl_add_executable(wxl.gen.ui
    main.cpp
    Editor.h
    Editor.cpp
    VirtualTree.h
    VirtualTree.cpp
)

# Шрифты значков -- Fluent UI System Icons (MIT), контурный и заливочный, рядом с
# исполняемым файлом. Коды знаков -- в icons/*.json того же набора.
wxl_target_assets(wxl.gen.ui Assets)

# Профили и обход метаданных -- те же, что у генератора.
target_link_libraries(wxl.gen.ui PRIVATE wxl.gen.common)

# Откуда открывается профиль по умолчанию: из дерева исходников.
target_compile_definitions(wxl.gen.ui PRIVATE
    "WXL_GEN_PROFILES=\"${PROJECT_SOURCE_DIR}/wxl.gen/profiles\"")
