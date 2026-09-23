// Квадратное уравнение: три коэффициента на входе, дискриминант и корни на выходе.
// Модель — поля с оповещением, интерфейс — одно выражение, привязанное к ним.

#include "Bind.h"
#include "Card.h"
#include "CompositionWindow.h"
#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::core;
using namespace wxl::dsl;

namespace {

u16_text to_string(double value) {
    return to_u16(value, std::chars_format::general, 9);
}

u16_text to_string(double real, double imaginary) {
    u16_text text = to_string(real);
    if (imaginary < 0)
        text += u" − j·";
    else
        text += u" + j·";
    text += to_string(std::abs(imaginary));
    return text;
}

struct Answer {
    u16_text D, x1, x2;
};

// Пустое поле NumberBox — это NaN: коэффициент без числа ответа не даёт.
constexpr double blank = std::numeric_limits<double>::quiet_NaN();

// Корни через q = −(b + sign(b)·√D)/2 как q/a и c/q: без вычитания близких чисел,
// которое съедает точность меньшего по модулю корня. При a = 0 уравнение линейное.
static Answer solve(double a, double b, double c) {
    u16_text const none{u"—"};

    if (std::isnan(a) || std::isnan(b) || std::isnan(c))
        return {none, none, none};

    if (a == 0) {
        if (b != 0)
            return {none, to_string(-c / b), none};

        if (c == 0)
            return {none, u16_text{u"ℝ"}, none};

        return {none, u16_text{u"∅"}, none};
    }

    double const d = b * b - 4 * a * c;

    if (d < 0) {
        // Комплексные корни
        double const real = -b / (2 * a);
        double const imaginary = std::sqrt(-d) / (2 * std::abs(a));
        return {to_string(d), to_string(real, -imaginary), to_string(real, imaginary)};
    }

    // Формула Мюллера для сохранения точности
    double const q = -0.5 * (b + std::copysign(std::sqrt(d), b));
    double const first = q / a;
    // Теорема Виета
    double const second = q == 0 ? first : c / q;
    return {to_string(d), to_string(std::min(first, second)), to_string(std::max(first, second))};
}

// Коэффициенты пишут поля ввода, ответы показывают поля вывода. Любая правка
// коэффициента пересчитывает ответы, пока все три — числа.
class Equation : public noncopyable
{
    observable<Answer> answer;

public:
    observable<double> a{blank}, b{blank}, c{blank};
    observable<u16_text> D, x1, x2;

    Equation() {
        answer                      // три числа → ответ
            .follow(a, b, c, solve)
            .on_change([this](Answer const& answer) noexcept {
                D.set(answer.D);
                x1.set(answer.x1);  // ответ → три текста
                x2.set(answer.x2);
            });
    }
};

const Preset common {
    fontSize = 18,
    hAlign.center,
    vAlign.center,
};

const Preset txt {
    common, // Пресеты могут вкладываться друг в друга
    FontWeight {600},
    foreground = colors.blue,
};

const Preset input {
    common,
    width = 200,
};

const Preset output {
    common,
    width = 300,
    isReadOnly = true,
};

}  // namespace

wxl::Teardown wxl_launched() {
    // Время жизни модели требуется продлить
    auto const model = std::make_shared<Equation>();
    Equation & eq = *model;

    auto window = CompositionWindow {
        title = u"WXL Quadratic",
        minSize = {910, 390},
        background = BackgroundImage {u"Assets/bk2.jpg", BackgroundFill::UniformToFill},
        Card {
            hAlign.center,
            vAlign.center,
            StackPanel {
                spacing = 32,
                TextBlock {
                    u"Решение квадратного уравнения",
                    styles.TextBlock.Subtitle,
                    hAlign.center,
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    NumberBox {
                        input,
                        placeholderText = u"a",
                        value = BindInput {eq.a},
                        onLoaded = [](NumberBox const& control) {
                            control.focus(FocusState::Programmatic);
                        },
                    },
                    TextBlock {txt, u"· x² +"},
                    NumberBox {
                        input,
                        placeholderText = u"b",
                        value = BindInput {eq.b},
                    },
                    TextBlock {txt, u"· x +"},
                    NumberBox {
                        input,
                        placeholderText = u"c",
                        value = BindInput {eq.c},
                    },
                    TextBlock {txt, u"= 0"},
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {txt, u"D ="},
                    TextBox {
                        output,
                        text = BindOutput {eq.D},
                    },
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {txt, u"x₁ ="},
                    TextBox {
                        output,
                        text = BindOutput {eq.x1},
                    },
                    TextBlock {txt, u",", Margin {0, 0, 16, 0}},
                    TextBlock {txt, u"x₂ ="},
                    TextBox {
                        output,
                        text = BindOutput {eq.x2},
                    },
                },
            },
        }
    };

    window.appWindow().resize({1380, 800});
    window.activate();

    // Продлеваем время жизни модели
    return [model](TeardownReason) {};
}
