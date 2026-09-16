#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

#include "Calc.h"

namespace calculator {

bool isNumeric(char16_t key)  { return (key >= u'0' && key <= u'9') || key == u'.'; }
bool isTerminal(char16_t key) { return key == u'C' || key == u'='; }

namespace {
    constexpr std::size_t digits_room = 64;

    // Прямая трансляция ASCII-буфера в u16string без лишних проходов
    std::u16string widen(char const* first, char const* last) {
        std::u16string text;
        text.reserve(static_cast<std::size_t>(last - first));
        for (; first != last; ++first) {
            text.push_back(static_cast<char16_t>(*first));
        }
        return text;
    }

    // Преобразование числа с защитой от NaN и исключением региональных локалей
    std::u16string number_text(double value) {
        if (std::isnan(value)) {
            return u"Error";
        }

        char narrow[digits_room];
        auto const [end, error] = std::to_chars(narrow, narrow + sizeof(narrow), value, std::chars_format::general, 10);

        return error == std::errc{} ? widen(narrow, end) : std::u16string{u"0"};
    }

    constexpr char16_t translate(char16_t key) {
        switch (key) {
            case u'*': return u'×';
            case u'/': return u'÷';
            default: return key;
        }
    }
}  // namespace

intrusive_ptr<Calc> Calc::create()
{
    return { new Calc{}, /*add_ref=*/ false };
}

double Calc::value() const {
    std::u16string const& text = display.get();
    if (text.empty() || text.size() >= digits_room) {
        return 0.0;
    }

    char narrow[digits_room];
    std::transform(text.begin(), text.end(), narrow, [](char16_t unit) {
        return static_cast<char>(unit);
    });

    double result = 0.0;
    auto const [end, error] = std::from_chars(narrow, narrow + text.size(), result);
    return error == std::errc{} ? result : 0.0;
}

void Calc::show(double result) {
    display.set(number_text(result));
    typing_ = false;
}

void Calc::updateExpression() {
    if (!operator_) {
        expression.set({});
        return;
    }

    std::u16string text = number_text(left_);
    text.reserve(text.size() + 3); // Защита от реалокаций при конкатенации
    text += u' ';
    text += operator_;
    expression.set(std::move(text));
}

void Calc::digit(char16_t key) {
    std::u16string text = typing_ ? display.get() : std::u16string{};
    typing_ = true;

    if (text.size() >= MaxDigits) {
        return;
    }

    if (key == u'.' && text.find(u'.') != std::u16string::npos) {
        return;
    }

    // Юзабилити-фикс: заменяем дефолтный ноль первой вводимой цифрой
    if (text == u"0" && key != u'.') {
        text = key;
    } else {
        text += key;
    }

    display.set(std::move(text));
}

double Calc::apply(double right) const {
    switch (operator_) {
        case u'+': return left_ + right;
        case u'-': return left_ - right;
        case u'*':
        case u'×': return left_ * right;
        case u'/':
        case u'÷': return right == 0.0 ? std::numeric_limits<double>::quiet_NaN() : left_ / right;
        default: return right;
    }
}

bool Calc::press(char16_t key) {
    key = translate(key);

    switch (key) {
        case u'C':
            display.set(u"0");
            left_ = 0.0;
            operator_ = 0;
            typing_ = false;
            break;

        case u'+':
        case u'-':
        case u'×':
        case u'÷':
            left_ = operator_ ? apply(value()) : value();
            operator_ = key;
            show(left_);
            break;

        case u'=':
            if (operator_) {
                show(apply(value()));
                operator_ = 0;
            }
            break;

        case u'±':
            show(-value());
            break;

        case u'%':
            show(value() / 100.0);
            break;

        case u'√':
            show(value() < 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::sqrt(value()));
            break;

        default:
            if (!isNumeric(key)) {
                return false;
            }
            digit(key);
            break;
    }

    updateExpression();
    return true;
}

}  // namespace calculator
