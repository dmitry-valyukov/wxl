# Проба: доступная ширина контейнера как источник привязки. LayoutPanel с CustomLayout пишет ширину из функции измерения в
# поле модели до измерения ребёнка. Режимы -- переменная окружения PROBE_OPTS, см. начало main.cpp.
wxl_add_executable(sandbox.available-width-probe
    main.cpp
)
