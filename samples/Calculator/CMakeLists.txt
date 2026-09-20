# Калькулятор на wxl:
# - стилизация кнопок и дисплея;
# - трюк с итерацией описания массива кнопок.
wxl_add_executable(sample.calc
    Calc.h
    Calc.cpp
    main.cpp
)

wxl_target_icon(sample.calc app.ico)
wxl_target_assets(sample.calc Assets)
