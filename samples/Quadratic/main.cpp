// Квадратное уравнение: три коэффициента на входе, дискриминант и корни на выходе.
// Модель — поля с оповещением, интерфейс — одно выражение, привязанное к ним.

// Стандартные заголовки — до заголовков wxl: те импортируют wxl.core, а после
// импорта MSVC не принимает стандартный заголовок, которого ещё не видел.
#include <charconv>

#include "Bind.h"
#include "Card.h"
#include "CompositionWindow.h"
#include "aliases.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/styles.h"
#include "launch.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

using text_field = core::observable<core::u16_text>;

// Коэффициент из текста поля: дробь пишется и через запятую, минус — и математическим знаком.
bool parseCoefficient(core::u16_text const& text, double& value) {
    std::array<char, 64> narrow;
    if (text.empty() || text.size() > narrow.size()) return false;

    std::size_t length = 0;
    for (char16_t unit : text.plain()) {
        if (unit == u',') unit = u'.';
        if (unit == u'−') unit = u'-';
        if (unit >= 0x80) return false;
        narrow[length++] = static_cast<char>(unit);
    }

    return core::try_parse(std::string_view{narrow.data(), length}, value);
}

// Число без локали, до двенадцати значащих цифр и с математическим минусом.
void appendNumber(core::u16_text& out, double value) {
    if (value < 0) out.push_back(U'−');

    std::string digits;
    core::append_number(digits, std::abs(value), std::chars_format::general, 12);
    for (char const digit : digits)
        out.push_back(static_cast<char32_t>(digit));
}

core::u16_text number(double value) {
    core::u16_text text;
    appendNumber(text, value);
    return text;
}

core::u16_text complex(double real, double imaginary) {
    core::u16_text text = number(real);
    if (imaginary < 0)
        text += u" − ";
    else
        text += u" + ";
    appendNumber(text, std::abs(imaginary));
    text += u"i";
    return text;
}

struct Answer {
    core::u16_text discriminant, x1, x2;
};

// Корни через q = −(b + sign(b)·√D)/2 как q/a и c/q: без вычитания близких чисел,
// которое съедает точность меньшего по модулю корня. При a = 0 уравнение линейное.
Answer solve(double a, double b, double c) {
    core::u16_text const none{u"—"};

    if (a == 0) {
        if (b != 0) return {none, number(-c / b), none};
        if (c == 0) return {none, core::u16_text{u"ℝ"}, none};
        return {none, core::u16_text{u"∅"}, none};
    }

    double const d = b * b - 4 * a * c;

    if (d < 0) {
        double const real = -b / (2 * a);
        double const imaginary = std::sqrt(-d) / (2 * std::abs(a));
        return {number(d), complex(real, -imaginary), complex(real, imaginary)};
    }

    double const q = -0.5 * (b + std::copysign(std::sqrt(d), b));
    double const first = q / a;
    double const second = q == 0 ? first : c / q;
    return {number(d), number(std::min(first, second)), number(std::max(first, second))};
}

// Коэффициенты пишут поля ввода, ответы показывают поля вывода. Любая правка
// коэффициента пересчитывает ответы, пока все три читаются как числа.
struct Equation {
    text_field a, b, c;
    text_field discriminant, x1, x2;

    Equation() {
        for (text_field* coefficient : {&a, &b, &c})
            coefficient->on_change([this](core::u16_text const&) noexcept { update(); });
    }

    void update() noexcept {
        double va = 0, vb = 0, vc = 0;
        Answer answer;
        if (parseCoefficient(a.get(), va) && parseCoefficient(b.get(), vb) &&
            parseCoefficient(c.get(), vc))
            answer = solve(va, vb, vc);

        discriminant.set(std::move(answer.discriminant));
        x1.set(std::move(answer.x1));
        x2.set(std::move(answer.x2));
    }
};

}  // namespace

wxl::Teardown wxl_launched() {
    auto const model = core::make_refcounted<Equation>();
    Equation* eq = model.get();

    auto const math = Preset{
        fontFamily = u"Cambria Math",
        fontSize = 22,
        vAlign.center,
    };

    auto const coefficient = Preset{
        fontSize = 18,
        width = 110,
        vAlign.center,
    };

    auto const result = Preset{
        fontSize = 18,
        width = 200,
        isReadOnly = true,
        vAlign.center,
    };

    auto window = CompositionWindow {
        title = u"WXL Quadratic",
        minSize = {720, 340},
        background = BackgroundImage{u"Assets/bk2.png", BackgroundFill::Tile},

        Card {
            hAlign.center,
            vAlign.center,
            Margin {16},
            Padding {28, 20, 28, 28},

            StackPanel {
                spacing = 20,

                TextBlock {styles.TextBlock.Subtitle, u"Решение квадратного уравнения"},

                StackPanel {
                    orientation.horizontal,
                    spacing = 10,

                    TextBox {
                        coefficient,
                        placeholderText = u"a",
                        text = Bind{eq->a},
                        onLoaded = [](TextBox const& box) {
                            box.focus(FocusState::Programmatic);
                        },
                    },
                    TextBlock {math, u"· x² +"},
                    TextBox {coefficient, placeholderText = u"b", text = Bind{eq->b}},
                    TextBlock {math, u"· x +"},
                    TextBox {coefficient, placeholderText = u"c", text = Bind{eq->c}},
                    TextBlock {math, u"= 0"},
                },

                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {math, u"D ="},
                    TextBox {result, text = Bind{eq->discriminant}},
                },

                StackPanel {
                    orientation.horizontal,
                    spacing = 10,
                    TextBlock {math, u"x₁ ="},
                    TextBox {result, text = Bind{eq->x1}},
                    TextBlock {math, u",", Margin {0, 0, 16, 0}},
                    TextBlock {math, u"x₂ ="},
                    TextBox {result, text = Bind{eq->x2}},
                },
            },
        },
    };

    window.centreWithClientSize({760, 360});
    window.activate();

    // Окно и модель живут, пока жив обработчик
    return [model, window](TeardownReason) {};
}
