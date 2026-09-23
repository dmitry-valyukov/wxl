# Визуальный редактор профилей генератора проекции (wxl.gen/profiles). Как
# любое приложение на wxl, стоит на wxl::ui; WIN32 делает точкой входа
# wWinMain самой wxl.

wxl_add_executable(wxl.gen.ui
    main.cpp
)

# Профили и обход метаданных -- те же, что у генератора.
target_link_libraries(wxl.gen.ui PRIVATE wxl.gen.common)
