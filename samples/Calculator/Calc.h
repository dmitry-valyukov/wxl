#pragma once

#include "pch.h"
#include <string>

using namespace wxl::core;

namespace calculator {

inline constexpr std::size_t MaxDigits = 32;

bool isNumeric(char16_t key);  // Цифры или десятичная точка (состав числа)
bool isTerminal(char16_t key); // Команды завершения ввода ('C' или '=')

class Calc : public wxl::core::sta_refcounted
{
public:
    static intrusive_ptr<Calc> create();

    bool press(char16_t key);

    // Вспомогательное табло сверху: текущее выражение в процессе (например, "12 +")
    observable<std::u16string> expression;

    // Главное табло: текущее вводимое число или результат вычислений
    observable<std::u16string> display{u"0"};

private:
    Calc() {}

    void digit(char16_t key);
    void show(double result);
    void updateExpression();
    double value() const;
    double apply(double right) const;

    double left_{0.0};
    char16_t operator_{u'\0'};
    bool typing_{false};
};

} // namespace calculator
