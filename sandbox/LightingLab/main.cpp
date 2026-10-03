// Стенд освещения: одно окно, слева сцена опыта, справа выбор опыта и его
// настройки. Здесь отрабатываются способы показать объём светом -- свет
// композитора с картой нормалей, прожектор за указателем, эффекты освещения
// Direct2D по карте высот, свой шейдер и тень от источника света.
//
// Новый опыт -- свой .cpp с функцией, строящей lab::Experiment, строка в
// Lab.h и строка в каталоге ниже.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

struct Entry {
    char16_t const* name;
    char16_t const* about;
    lab::Build build;
};

constexpr Entry catalogue[] = {
    {u"Свет композитора: материал",
     u"Источники света Microsoft.UI.Composition и SceneLightingEffect с картой нормалей. Свет считает "
     u"композитор XAML этого потока, движение света -- его выражение или ключевые кадры.",
     &lab::materialPage},
    {u"Свет композитора: прожектор за указателем",
     u"Клавиши XAML под двумя прожекторами, место которых выражение берёт из набора свойств указателя: "
     u"один высвечивает пятно на клавише, другой -- рамки соседних клавиш.",
     &lab::revealPage},
    {u"Direct2D: освещение по карте высот",
     u"Встроенные эффекты освещения Direct2D: альфа-канал картинки -- высота, нормаль берётся оператором "
     u"Собеля. Кадр рисует приложение в поверхность композитора при каждом изменении.",
     &lab::direct2dPage},
    {u"Direct3D: свой шейдер",
     u"Пиксельный шейдер считает высоту клавиши по расстоянию до её края, нормаль, свет, тень и "
     u"затенение у основания. Кадр рисует приложение в поверхность композитора.",
     &lab::shaderPage},
    {u"Свет композитора: тень от источника",
     u"CompositionProjectedShadow: тень, которую источник света бросает от одних визуалов на другие.",
     &lab::shadowPage},
    {u"Scenes: клавиша настоящей геометрией",
     u"Microsoft.UI.Composition.Scenes: сетки треугольников с материалом «металличность — шероховатость» "
     u"в дереве композитора XAML этого потока.",
     &lab::scenesPage},
};

constexpr char16_t const* names[] = {
    catalogue[0].name, catalogue[1].name, catalogue[2].name, catalogue[3].name, catalogue[4].name, catalogue[5].name,
};

// Опыт на экране. Поле page пишет список опытов; сцена и панель настроек
// меняют ребёнка по тому же полю, уже после того, как опыт построен.
struct Shell {
    core::observable<int> page {-1};
    core::observable<hstring> about;
    std::unique_ptr<lab::Experiment> current;
    // Прежний опыт живёт, пока его элементы не сменены новыми.
    std::unique_ptr<lab::Experiment> retired;

    Shell() {
        page.on_change([this](int const& index) noexcept { open(index); });
    }

    void open(int index) noexcept {
        retired = std::move(current);
        if (index < 0 || index >= static_cast<int>(std::size(catalogue))) {
            return;
        }
        try {
            current = std::make_unique<lab::Experiment>(catalogue[index].build());
            about.set(hstring {catalogue[index].about});
        } catch (...) {
            about.set(lab::refusalOfCurrentException(catalogue[index].name));
        }
    }

    void close() noexcept {
        current.reset();
        retired.reset();
    }
};

}  // namespace

wxl::Teardown wxl_launched() {
    auto const shell = std::make_shared<Shell>();

    auto window = Window {
        title = u"wxl — стенд освещения",
        minSize = {1100, 640},
        Grid {
            columnDefinitions = u"*,380",
            background = brushes.SolidBackgroundFillColor.Base,

            Border {
                column = 0,
                [shell](Border const& host) {
                    shell->page.on_change([host, shell = shell.get()](int const&) noexcept {
                        if (shell->current) {
                            host.child(shell->current->stage);
                        }
                    });
                },
            },

            Border {
                column = 1,
                borderBrush = brushes.Card.StrokeColorDefault,
                BorderThickness {1, 0, 0, 0},
                background = brushes.SolidBackgroundFillColor.Secondary,
                Grid {
                    rowDefinitions = u"auto,*",
                    StackPanel {
                        row = 0,
                        spacing = 8.0,
                        Margin {12, 12, 12, 4},
                        lab::choiceRow(u"Опыт", shell->page, names),
                        TextBlock {text = BindOutput {shell->about}, textWrapping = TextWrapping::Wrap, opacity = 0.75},
                    },
                    ScrollViewer {
                        row = 1,
                        content = Border {
                            [shell](Border const& host) {
                                shell->page.on_change([host, shell = shell.get()](int const&) noexcept {
                                    if (shell->current) {
                                        host.child(shell->current->settings);
                                    }
                                });
                            },
                        },
                    },
                },
            },
        },
    };

    shell->page.set(0);

    window.appWindow().resize({1500, 900});
    window.activate();

    return [shell](wxl::TeardownReason) -> std::optional<int> {
        shell->close();
        return std::nullopt;
    };
}
