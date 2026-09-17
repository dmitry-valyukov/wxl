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

std::optional<double> to_double(const u16_text & wstr)  {
    sta_string str;
    unicode::append_utf8(str, wstr);
    // try_parse парсит в локали C
    std::replace(str.begin(), str.end(), ',', '.');
    double value;
    if (!try_parse(str, value))
        return std::nullopt;

    return value;
};

struct Answer {
    u16_text D, x1, x2;
};

using opt_double = std::optional<double>;

// Корни через q = −(b + sign(b)·√D)/2 как q/a и c/q: без вычитания близких чисел,
// которое съедает точность меньшего по модулю корня. При a = 0 уравнение линейное.
static Answer solve(opt_double va, opt_double vb, opt_double vc) {
    u16_text const none{u"—"};

    if(va && vb && vc) {
        double a = *va, b = *vb, c = *vc;

        if (a == 0) {
            if (b != 0) return {none, to_string(-c / b), none};
            if (c == 0) return {none, u16_text{u"ℝ"}, none};
            return {none, u16_text{u"∅"}, none};
        }

        double const d = b * b - 4 * a * c;

        if (d < 0) {
            double const real = -b / (2 * a);
            double const imaginary = std::sqrt(-d) / (2 * std::abs(a));
            return {to_string(d), to_string(real, -imaginary), to_string(real, imaginary)};
        }

        double const q = -0.5 * (b + std::copysign(std::sqrt(d), b));
        double const first = q / a;
        double const second = q == 0 ? first : c / q;
        return {to_string(d), to_string(std::min(first, second)), to_string(std::max(first, second))};
    }

    return {none, none, none};
}

// Коэффициенты пишут поля ввода, ответы показывают поля вывода. Любая правка
// коэффициента пересчитывает ответы, пока все три читаются как числа.
class Equation : public noncopyable
{
    observable<std::optional<double>> va, vb, vc;
    observable<Answer> answer;

public:
    observable<u16_text> a, b, c;
    observable<u16_text> D, x1, x2;

    Equation() {
        va.follow(a, to_double);
        vb.follow(b, to_double);
        vc.follow(c, to_double);

        answer
            .follow(va, vb, vc, solve)
            .on_change([this](Answer const& answer) noexcept {
                D.set(answer.D);
                x1.set(answer.x1);
                x2.set(answer.x2);
            });
    }
};

auto const common = Preset {
    FontWeight {600},
    hAlign.center,
};

auto const txt = Preset {
    common,
    styles.TextBlock.Subtitle,
};

auto const input = Preset {
    common,
    fontSize = 18,
    width = 200,
};

auto const output = Preset {
    common,
    fontSize = 18,
    width = 300,
    isReadOnly = true,
};

}  // namespace

wxl::Teardown wxl_launched() {
    // Время жизни модели надо будет продлить
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
                    txt,
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBox {
                        input,
                        placeholderText = u"a",
                        text = Bind {eq.a},
                        onLoaded = [](TextBox const& box) {
                            box.focus(FocusState::Programmatic);
                        },
                    },
                    TextBlock {txt, u"· x² +"},
                    TextBox {
                        input,
                        placeholderText = u"b",
                        text = Bind {eq.b},
                    },
                    TextBlock {txt, u"· x +"},
                    TextBox {
                        input,
                        placeholderText = u"c",
                        text = Bind {eq.c},
                    },
                    TextBlock {txt, u"= 0"},
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {txt, u"D ="},
                    TextBox {
                        output,
                        text = Bind {eq.D},
                    },
                },
                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {txt, u"x₁ ="},
                    TextBox {
                        output,
                        text = Bind {eq.x1},
                    },
                    TextBlock {txt, u",", Margin {0, 0, 16, 0}},
                    TextBlock {txt, u"x₂ ="},
                    TextBox {
                        output,
                        text = Bind {eq.x2},
                    },
                },
            },
        }
    };

    window.appWindow().resize({920, 400});
    window.activate();

    // Продлеваем время жизни модели
    return [model](TeardownReason) {};
}
