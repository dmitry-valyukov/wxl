# Проба своего окна на композиторе -- открывает wxl::CompositionWindow. Как
# любое приложение на wxl: линкует только wxl::ui, проекции cppwinrt не видит,
# точкой входа служит wWinMain самой wxl. Загрузчик Windows App Runtime и
# Win2D кладёт рядом с exe wxl_add_executable() -- приложение о них не знает.
wxl_add_executable(sandbox.composition-probe
    main.cpp
)
